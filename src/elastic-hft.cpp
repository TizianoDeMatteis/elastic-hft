/**
    ---------------------------------------------------------------------

    Copyright (C) 2015- by Tiziano De Matteis (dematteis <at> di.unipi.it)

    This file is part of elastic-hft.

    elastic-hft is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    ---------------------------------------------------------------------

    Main program for elastic-hft
*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <ff/buffer.hpp>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <sched.h>
#include <ff/allocator.hpp>
#include "../includes/cycle.h"
#include "../includes/general.h"
#include "../includes/elastic-hft.h"
#include "../includes/repository.hpp"
#include "../includes/strategy_descriptor.hpp"
#include "../includes/statistics.hpp"
#include "../includes/utils.h"

using namespace ff;
using namespace std;

int mon_step=0;


int main(int argc, char *argv[])
{

	int num_workers;
	int num_classes;
	int port;
	int i;
	int window_size;
	int window_slide;
	cpu_set_t cpuset;
	SWSR_Ptr_Buffer **quEW; //queues emitter->worker
	SWSR_Ptr_Buffer **quWC; //queues worker->collector
	#if defined(MONITORING)
		SWSR_Ptr_Buffer *quECN; //queue emitter->controller
		SWSR_Ptr_Buffer *quCCN; //queue collector->controller
		SWSR_Ptr_Buffer **quWCN; //queues workers->controller
		SWSR_Ptr_Buffer *quCNE; //queue controller->emitter
		SWSR_Ptr_Buffer *quCNC; //queue controller->collector
        Repository *repository; //repository that will contain all the structures needed for reconfigurations
	#endif
	//arguments for threads
	worker_data_t *worker_data;
	emitter_data_t emitter_data;
	collector_data_t collector_data;
	controller_data_t controller_data;

	//threads ids
	pthread_t *wtids;
	pthread_t ctid,cntid;

    //stats returned by controller and collector
    stats::ExecutionStatistics *coll_stats=nullptr;
    stats::ReconfigurationStatistics *rec_stat=nullptr;
	/**
		check line arguments...
	*/
    if(argc<7)
	{
        fprintf(stderr, "Usage: %s num_keys num_replicas port window_size window_slide config_file\n",argv[0] );
		return EXIT_FAILURE;
	}
	num_classes=atoi(argv[1]);
	num_workers=atoi(argv[2]);
	port=atoi(argv[3]);
	window_size=atoi(argv[4]);
    window_slide=atoi(argv[5]);
    //read strategy config file
    StrategyDescriptor *sd=new StrategyDescriptor(argv[6]);

	assert(window_size%window_slide==0);
    #ifndef MONITORING
        sd->type=StrategyType::NONE;  //in this case we cannot use any strategy
    #endif
    //get the core ids, derive the maximum number of workers and the affinities of the entities
    vector<int>* core_ids=getCoreIDs();
#if defined GENERATOR_ON_DIFFERENT_MACHINE
    int max_workers=core_ids->size()-3;
    int emitter_affinity=core_ids->at(0);
#else
    int max_workers=core_ids->size()-4;
    int emitter_affinity=core_ids->at(1);
#endif

    //da gestire il caso generatore sulla stessa macchina
    int collector_affinity=core_ids->at(core_ids->size()-2);
    int controller_affinity=core_ids->at(core_ids->size()-1);
    int *affinities=new int[max_workers];
    for(int i=0;i<max_workers;i++)
    {
#if defined GENERATOR_ON_DIFFERENT_MACHINE
        affinities[i]=core_ids->at(1+i);
#else
        affinities[i]=core_ids->at(2+i);
#endif

    }
    //affinities: first core for the generator, second for the emitter, then workers and the last two for collector and controller
    if(num_workers>max_workers)
    {

        cerr << ANSI_COLOR_RED << "Error: the number of starting replica exceeds the maximum allowed that for this machine is equal to "<<max_workers<<ANSI_COLOR_RESET<<endl;
        exit(-1);
    }
    assert(sd->control_step%PRINT_RATE==0); //da spostare poi nel main. Altrimenti non funge, serve per garantire i dati corretti al controllore
    CONTROL_PRINT(cout<<"Threads Affinities (core ids):"<<endl;)
    CONTROL_PRINT(cout << "Splitter on: "<< emitter_affinity<< " Merger: "<< collector_affinity<<" Controller: "<< controller_affinity<<endl;)
    CONTROL_PRINT(cout << "Replicas on: [";
    for(int i=0;i<max_workers;i++)
        cout << affinities[i]<<" ";
    cout << "]"<<endl;)

    sd->print();

    float freq=getMaximumFrequency();
    float minFreqGHz=getMinimumFrequency()/1000.0;
    CONTROL_PRINT(cout << "CPU Nominal Frequency: "<<freq<< " MHz"<<endl;)


	/**
		Initialize data structures
	*/
	pthread_barrier_t barrier;
	pthread_barrier_init (&barrier, NULL, num_workers + 2);
	//queues (we allocate space for having max_workers queues in case of reconfigurations that involve changes in par degree)
	quEW = (SWSR_Ptr_Buffer**) malloc(max_workers * sizeof(SWSR_Ptr_Buffer*));
	ERRNULL(quEW);
	quWC = (SWSR_Ptr_Buffer**) malloc(max_workers * sizeof(SWSR_Ptr_Buffer*));
	ERRNULL(quWC);
	#if defined(MONITORING)
		quWCN= (SWSR_Ptr_Buffer**) malloc(max_workers * sizeof(SWSR_Ptr_Buffer*));
		ERRNULL(quWCN);
		quECN=new SWSR_Ptr_Buffer(QUEUE_SIZE_MON);
		quECN->init();
		quCCN=new SWSR_Ptr_Buffer(QUEUE_SIZE_MON);
		quCCN->init();
		quCNE=new SWSR_Ptr_Buffer(QUEUE_SIZE_MON);
		quCNE->init();
		quCNC=new SWSR_Ptr_Buffer(QUEUE_SIZE_MON);
		quCNC->init();
       repository=new Repository(max_workers,num_classes);
    #endif

	#ifdef QSPLITTED
	int qsize=QUEUE_SIZE/num_workers;
	#else
	int qsize=QUEUE_SIZE;
	#endif	
	//We initialize up to the actual par degree (not the maximum one)
	for (i = 0; i < num_workers; i++) {
		quEW[i] = new SWSR_Ptr_Buffer(qsize);
		quEW[i]->init();
		quWC[i] = new SWSR_Ptr_Buffer(qsize); //technically we could reduce this
		quWC[i]->init();
		#if defined(MONITORING)
			quWCN[i]=new SWSR_Ptr_Buffer(QUEUE_SIZE_MON);
			quWCN[i]->init();
		#endif
	}
	

	ticks *start_global_ticks=(ticks*)calloc(1,sizeof(ticks));
    long *first_tuple_timestamp=(long *)calloc(1,sizeof(long));
    long *start_global_usecs=(long *)calloc(1,sizeof(long));
	//suffix for output files
	char suffix[80]; //(it will exist until all activities are not finished)
	//suffix for output files
	sprintf(suffix,"%d_%d_%d_%d",num_classes,num_workers,window_size,window_slide);
    #if defined(USE_FFALLOC)
            ff_allocator* ffalloc=new ff_allocator;
            assert(ffalloc);
            //qui indicano il numero di segmenti per dimensione 32,64,128,...
            int nslabs[N_SLABBUFFER]={0,QUEUE_SIZE*num_workers,0,0,0,0,0,0,0 };
            if (ffalloc->init(nslabs)<0) {
                error("FATAL ERROR: allocator init failed\n");
                abort();
            }
	#endif    
    #if defined(USE_FFALLOC)
            printf("Fastflow Allocator will be used\n");
    #endif
	
	/**
		Threads creations
	*/
	//Workers creation
	wtids = (pthread_t*) malloc(num_workers * sizeof(pthread_t));
	worker_data =(worker_data_t*) malloc(num_workers * sizeof(worker_data_t));
	for (i = 0; i < num_workers; i++) {		
		pthread_t tid;
		worker_data[i].workerId = i;
		worker_data[i].inqueue = quEW[i];
		worker_data[i].outqueue = quWC[i];

		worker_data[i].barrier=&barrier;
		worker_data[i].window_size=window_size;
		worker_data[i].window_slide=window_slide;
		worker_data[i].start_global_ticks=start_global_ticks;
		//worker_data[i].comp_time=comp_time;
        worker_data[i].freq=freq;
		worker_data[i].num_classes=num_classes;
        worker_data[i].sd=sd;
        #if defined(USE_FFALLOC)
			worker_data[i].ffalloc=ffalloc;
		#endif
		#if defined(MONITORING)
			worker_data[i].cn_outqueue=quWCN[i];
            worker_data[i].repository=repository;
		#endif
		pthread_create(&tid, NULL, worker, &(worker_data[i]));
		//set CPU affinity of worker threads
		CPU_ZERO(&cpuset);
		CPU_SET(affinities[i], &cpuset);

		if (pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset)) {
			cerr << "Cannot set thread to CPU " << i << endl;
		}
		//worker_data[i].num_workers = num_workers;
		wtids[i] = tid;
	}

	//collector creation
	collector_data.num_workers=num_workers;
	collector_data.num_classes=num_classes;
	collector_data.inqueue=quWC;
	collector_data.wqueue=quEW;
	collector_data.barrier=&barrier;
    collector_data.freq=freq;
	collector_data.start_global_ticks=start_global_ticks;
    collector_data.start_global_usecs=start_global_usecs;
    collector_data.first_tuple_timestamp=first_tuple_timestamp;
	collector_data.suffix=suffix;
	collector_data.window_slide=window_slide;
	collector_data.max_workers=max_workers;
    collector_data.sd=sd;
	#if defined(MONITORING)
		collector_data.cn_outqueue=quCCN;
		collector_data.cn_inqueue=quCNC;
	#endif
	pthread_create(&ctid, NULL, collector, &collector_data);
	//set CPU affinity of collector thread
	CPU_ZERO(&cpuset);
	CPU_SET(collector_affinity, &cpuset);

	if (pthread_setaffinity_np(ctid, sizeof(cpu_set_t), &cpuset)) {
		cerr << "Cannot set thread to CPU " << i << endl;
	}

	#if defined(MONITORING)
	//controller creation
	controller_data.num_workers=num_workers;
	controller_data.num_classes=num_classes;
	controller_data.e_inqueue=quECN;
	controller_data.w_inqueue=quWCN;
	controller_data.c_inqueue=quCCN;
	controller_data.e_outqueue=quCNE;
	controller_data.c_outqueue=quCNC;
	controller_data.suffix=suffix;
    controller_data.repository=repository;
	controller_data.affinities=affinities;
	controller_data.window_size=window_size;
	controller_data.window_slide=window_slide;
	controller_data.max_workers=max_workers;
    controller_data.freq=freq;
	controller_data.start_global_ticks=start_global_ticks;
    controller_data.start_global_usecs=start_global_usecs;
    controller_data.sd=sd;
    #if defined(USE_FFALLOC)
		controller_data.ffalloc=ffalloc;
	#endif

	pthread_create(&cntid, NULL, controller, &controller_data);
	//set CPU affinity of collector thread
	CPU_ZERO(&cpuset);
	CPU_SET(controller_affinity, &cpuset);
	if (pthread_setaffinity_np(cntid, sizeof(cpu_set_t), &cpuset)) {
		cerr << "Cannot set thread to CPU " << i << endl;
	}

	#endif
	// Start working as emitter
	emitter_data.num_workers=num_workers;
	emitter_data.num_classes=num_classes;
	emitter_data.port=port;
	emitter_data.outqueue=quEW;
	emitter_data.barrier=&barrier;
	// emitter_data.classes_freq=classes_freq;
	//emitter_data.scheduling_table=scheduling_table;
	emitter_data.start_global_ticks=start_global_ticks;
    emitter_data.first_tuple_timestamp=first_tuple_timestamp;
    emitter_data.start_global_usecs=start_global_usecs;
    emitter_data.freq=freq;
	emitter_data.window_slide=window_slide;
    emitter_data.sd=sd;
    #if defined(USE_FFALLOC)
		emitter_data.ffalloc=ffalloc;
	#endif
	#if defined(MONITORING)
		emitter_data.cn_outqueue=quECN;
		emitter_data.cn_inqueue=quCNE;
        emitter_data.repository=repository;
	#endif
	// Set my affinity
	CPU_ZERO(&cpuset);
	CPU_SET(emitter_affinity, &cpuset);
	pthread_t etid = pthread_self();

	if (pthread_setaffinity_np(etid, sizeof(cpu_set_t), &cpuset)) {
		cerr << "Cannot set thread to CPU " << i << endl;
	}
	emitter(&emitter_data);

	// Wait for the completion of the threads
	void *retval ;
	for (i = 0; i < num_workers; i++) {
		pthread_join(wtids[i], &retval);
	}

    //wait for controller

    pthread_join(ctid, &retval);
    coll_stats=(stats::ExecutionStatistics *)retval;

	#if defined(MONITORING)
	pthread_join(cntid, &retval);
    rec_stat=(stats::ReconfigurationStatistics *)retval;

	#endif
    char out_file[100];
    //sprintf(out_file,"stats_%s.dat",suffix);
    sprintf(out_file,"stats.dat",suffix);
    FILE* fout=fopen(out_file,"w");
    if(sd->type==StrategyType::NONE || rec_stat==nullptr)
    {
        //we have only the stats from the collector
        fprintf(fout,"#Second\tNum_res\tLatency\t95-Perc\t99-Perc\tTop-Lat\tStdDev-Lat\n");
        for(int i=0;i<coll_stats->getStatsNumber();i++)
        {

            fprintf(fout,"%-6.3f\t%-6Ld\t%-6.4f\t%-6.4f\t",coll_stats->getTime(i),coll_stats->getRecvResults(i),coll_stats->getLatency(i),coll_stats->getLatencyPercentile95(i));
            fprintf(fout,"%-6.3f\t%-6.3f\t%-6.3f\n",coll_stats->getLatencyPercentile99(i),coll_stats->getLatencyTop(i),coll_stats->getStdDev(i));
        }

    }
    else
    {
        fprintf(fout,"#Second\tNum_res\tLatency\tLatency-95-Perc\tNum_replicas\tCpu_Freq\tCore_Joules\tCpu_Joules\n");
        //merge the two statistics and print them to file
        //assuming that collector stats are reported on a second basis
        //and that control step is multiple of second
        //consider the ratio between print rate and control step
        int ratio=(int)(sd->control_step/PRINT_RATE);
        int violations=0;
        int j=-1;
        //compute desired stats in terms of used resources and reconf amplitude
        int used_cores=0;
        int last_par_deg=-1;
        int last_freq=-1;
        double reconf_amplitude=0;

        //same step for printing and control (at most one stat of difference)

        for(int i=0;i<coll_stats->getStatsNumber();i++)
        {
            if(i%ratio==0 && j<rec_stat->getStatsNumber()-1)
                j++;
            fprintf(fout,"%-6.3f\t%-6Ld\t%-6.4f\t%-6.4f\t",coll_stats->getTime(i),coll_stats->getRecvResults(i),coll_stats->getLatency(i),coll_stats->getLatencyPercentile95(i));
            fprintf(fout,"%-6d\t%-6d\t",rec_stat->getParDegree(j),(int)rec_stat->getFrequency(j));
            fprintf(fout,"%-6.3f\t%-6.3f\n",rec_stat->getJouleCore(j),rec_stat->getJouleCpu(j));
            //we add also stat on the latencies, needed for testing (not used here)
            //fprintf(fout,"%-6.3f\t%-6.3f\t%-6.3f\n",coll_stats->getLatencyPercentile99(i),coll_stats->getLatencyTop(i),coll_stats->getStdDev(i));
            if(sd->type==StrategyType::LATENCY  || sd->type == StrategyType::LATENCY_RULE || sd->type==StrategyType::LATENCY_ENERGY) //check violation to latency threshold
            {
                if(coll_stats->getLatency(i)/1000.0>sd->threshold)
                    violations++;
            }
            used_cores+=rec_stat->getParDegree(j);
            if(last_freq!=-1)
            {
                double ampl=(rec_stat->getParDegree(j)-last_par_deg)*(rec_stat->getParDegree(j)-last_par_deg);
                //euclidean distance
                ampl+=((((double)last_freq)/1000000.0-minFreqGHz)*10-(((double)rec_stat->getFrequency(j))/1000000.0-minFreqGHz)*10)*((((double)last_freq)/1000000.0-minFreqGHz)*10-(((double)rec_stat->getFrequency(j))/1000000.0-minFreqGHz)*10);
                reconf_amplitude+=sqrt(ampl);
                last_par_deg=rec_stat->getParDegree(j);
                last_freq=(int)rec_stat->getFrequency(j);
            }
            else
            {
                last_par_deg=rec_stat->getParDegree(j);
                last_freq=(int)rec_stat->getFrequency(j);
            }

        }

        fprintf(fout,"#Total number of reconfigurations:      %d\n",rec_stat->getTotReconf());
        fprintf(stdout,"#Total number of reconfigurations:      %d\n",rec_stat->getTotReconf());
        if(sd->type==StrategyType::LATENCY_ENERGY)
        {
            fprintf(fout,"#Adjustments to the number of replicas:  %d\n",rec_stat->getParDegreeReconf());
            fprintf(stdout,"#Adjustments to the number of replicas:  %d\n",rec_stat->getParDegreeReconf());
            fprintf(fout,"#Adjustements to the CPU frequency:      %d\n",rec_stat->getFreqReconf());
            fprintf(stdout,"#Adjustements to the CPU frequency:      %d\n",rec_stat->getFreqReconf());
        }
        if(sd->type==StrategyType::LATENCY || sd->type == StrategyType::LATENCY_ENERGY || sd->type == StrategyType::LATENCY_RULE ) //check violation to latency threshold
        {
            fprintf(fout,"#Violations wrt the threshold:          %d\n",violations);
            fprintf(stdout,"#Violations wrt the threshold:          %d\n",violations);
        }
        if(sd->type==StrategyType::LATENCY)
        {
            fprintf(fout,"#Average Number of used replica:        %f\n",((double)used_cores)/rec_stat->getStatsNumber());
            fprintf(stdout,"#Average Number of used replica:        %f\n",((double)used_cores)/rec_stat->getStatsNumber());
        }
        //fprintf(fout,"#Total Core Joules:                     %f\n",rec_stat->getTotJoulesCore());
        //fprintf(fout,"#Total Cpu Joules:                      %f\n",rec_stat->getTotJoulesCpu());
        fprintf(fout,"#Average Watt consumed:                 %f\n",rec_stat->getTotJoulesCore()/(coll_stats->getTime(coll_stats->getStatsNumber()-1)-coll_stats->getTime(0))); //joules per second
        fprintf(stdout,"#Average Watt consumed:                 %f\n",rec_stat->getTotJoulesCore()/(coll_stats->getTime(coll_stats->getStatsNumber()-1)-coll_stats->getTime(0))); //joules per second
        fprintf(fout,"#Reconfiguration amplitude:             %.3f\n",reconf_amplitude/rec_stat->getTotReconf());
        fprintf(stdout,"#Reconfiguration amplitude:             %.3f\n",reconf_amplitude/rec_stat->getTotReconf());
        fprintf(fout,"#Strategy: %s\n",sd->toString());

    }


    fclose(fout);
    exit(EXIT_SUCCESS);
}
