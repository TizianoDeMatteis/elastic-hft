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

    Controller for elastic-hft
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
#include <ff/allocator.hpp>	
#include <math.h>
#include <climits>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <sched.h>
#include <map>
#include "../includes/cycle.h"
#include "../includes/general.h"
#include "../includes/elastic-hft.h"
#include "../includes/messages.hpp"
#include "../includes/HoltWinters.h"
#include "../includes/strategies.hpp"
#include "../includes/sched_tables.hpp"
#include "../includes/utils.h"
#include "../includes/repository.hpp"
#include "../includes/derived_metrics.hpp"
#include "../includes/statistics.hpp"
#include <ff/buffer.hpp>
#include <ff/allocator.hpp>
#include <mammut/cpufreq/cpufreq.hpp>
#include <mammut/energy/energy.hpp>


using namespace ff;
using namespace std;


/**
	Forecast the next h step of the interarrival rate. 
    Results are stored in the forecast array

	We can choose between two prediction model:
	- Simple Moving Average: will consider a window of lenght specified
    - HoltWinter:
*/
inline void forecast(vector<double> obs_v,int h, double *forecasted) __attribute__((always_inline));

/**
 Main function executed by the control thread
*/
void *controller (void *args)
{

	/*
		Take the parameters passed as arguments
	*/
	controller_data_t *data = (controller_data_t *) args;
	int num_workers=data->num_workers;
	int *affinities=data->affinities;
	int num_classes=data->num_classes;
	SWSR_Ptr_Buffer *e_inqueue=data->e_inqueue;
	SWSR_Ptr_Buffer **w_inqueue=data->w_inqueue;
	SWSR_Ptr_Buffer *c_inqueue=data->c_inqueue;
	SWSR_Ptr_Buffer *e_outqueue=data->e_outqueue;
	SWSR_Ptr_Buffer *c_outqueue=data->c_outqueue;
	char *suffix=data->suffix;
	long int freq=data->freq;
	int window_size=data->window_size;
	int window_slide=data->window_slide;
    int max_workers=data->max_workers;
	ticks *start_global_ticks=data->start_global_ticks;
    StrategyDescriptor *sd=data->sd;
    Repository *repository=data->repository;
    #if defined(USE_FFALLOC)
		ff_allocator *ffalloc=data->ffalloc;
	#endif
	cpu_set_t cpuset;

    //Monitoring messages
    msg::EmitterMonitoring *em=nullptr;
    msg::EmitterMonitoring *top_em=nullptr; //this is used to check wether the Emitter send an EOS just before the Controllore spawn something

    msg::WorkerMonitoring **wm=new msg::WorkerMonitoring*[max_workers];
    msg::CollectorMonitoring *cm;
	int64_t monitoring_step=0;

	//The controller will be stopped by the emitter
	bool stop=false;

    // Data about spawned workers
    worker_data_t *worker_data; //for thread spawning
    pthread_t *wtids=new pthread_t[max_workers]();
	char tid_idx=0;
	int threads_spawned=0; //number of thread spawned (that have to be checked on)


	bool exiting=false;
    int n_opt=0; //the optimal par_degree choosed by the adaptation strategy (if any)

    //data structure for metrics and stats
    DerivedMetrics metrics(num_classes,max_workers);
    stats::ReconfigurationStatistics *rec_stats=new stats::ReconfigurationStatistics();
    stats::ComputationStatistics *comp_stats=new stats::ComputationStatistics(num_classes);
	
    long reconf_start_t;

    /**
      variables needed for predictive strategies
    */
    vector<double> rate_per_msecond; //history of the rates per msecond seen so far
    vector<double> forecasts;

    double *forecasted;                             //forecasted values of the interarrival time
    int *pred_trajectory;                           // trajectory of reconfiguration variable for the case in which energy is not involved (they are essentially the par degree)
    reconf_choice_energy_t *pred_trajectory_energy;  //trajectory for the case in which we have energy (they are a vector [par_degree freq])
    vector<double> mape; //just for statistics
    //Metrics that will be used for  kingman formula
    //just for testing: keep track of the values predicted by kingman (if a latency strategy is adopted) and print them to file
    vector<double> king_values;
    vector<double> exp_resp_times;

    //keep track of the step at which we had a reconfiguration (frequency or par_degree)
    vector<bool> reconf_at_step;
    int space_resvd_reconf_at_step=200;
    reconf_at_step.reserve(space_resvd_reconf_at_step);
    double ksf=1;
    double exp_rt;
    double kingman_prev;
    if(sd->predictive)
    {
        forecasts.push_back(0);
        forecasted=new double[sd->horizon]();
        if( sd->type==StrategyType::LATENCY)
            pred_trajectory=new int[sd->horizon]();
        else
            pred_trajectory_energy=new reconf_choice_energy_t[sd->horizon]();

    }

    /**
        Variables needed for tpds strategy
    */
    //We should keep some information.
    //The current adaptation period (denoted by P in the article) is kept in the monitoring_step variable
		
    int current_level; //the current level
    //we have to keep some information on a level basis
    int *p_i,*thr_first_i,*thr_last_i;
    bool *c_i;
    double s;
    //if required by the strategy, initialize them
    if(sd->type==StrategyType::TPDS)
    {
        p_i=new int[sd->max_level]();               //the last period during which the algorithm was at this level
        c_i=new bool[sd->max_level]();              // whether congestion was observed the last time the algorithm was at this level
        thr_first_i=new int[sd->max_level]();       //the throughput observed during the first of the periods the last time the algorithm stayed consectuive periods at this level
        thr_last_i=new int[sd->max_level]();        //the throughput observed the last time the algorithm was at this level
        //init
        current_level=num_workers;
        s=0.1+(1.0-sd->change_sensitivity)*0.9;
        for(int i=0;i<=sd->max_level;i++)           //max_level = max_workers-1
        {
            p_i[i]=-1; //nan
            c_i[i]=true;
            thr_last_i[i]=INT_MAX;
            thr_first_i[i]=-1;
        }
    }

	/*
        Data Structures for Energy Management (mesurements and change frequency)
        Set Governor and take Cpu Counters

	*/
    double joulesCore, joulesCpu, totalJoulesCores=0, totalJoulesCpu=0;
	double current_frequency;
	mammut::cpufreq::Frequency freq_opt=0,freq_pred=0;
	mammut::cpufreq::CpuFreq* frequency= mammut::cpufreq::CpuFreq::local();
	vector<mammut::cpufreq::Domain*> domains = frequency->getDomains();
	vector<mammut::cpufreq::Frequency> available_frequencies = domains.at(0)->getAvailableFrequencies();

	for(int i=0;i<domains.size();i++)
		assert(domains.at(i)->setGovernor(mammut::cpufreq::GOVERNOR_USERSPACE));
    mammut::energy::Energy* energy = mammut::energy::Energy::local();
	vector<mammut::energy::CounterCpu*> counters = energy->getCountersCpu();
	//Initally, we set the frequency at the maximum
	for(int i=0;i<domains.size();i++)
		if(domains.at(i)-> setHighestFrequencyUserspace()==false)
		{
            std::cerr <<ANSI_COLOR_RED "[CONTROLLER] error in setting the cpu frequency" ANSI_COLOR_RESET<<endl;
		}
    //load voltage table from file
    map<pair<int,int>,double> *voltages=loadVoltageTable("./voltages.txt");


    double ksf_samples[5]={1,1,1,1,1};
    char ksf_samples_idx=0;
    int num_rebalancing=0;

    //wait the begining of the program and reset counters

    while(*start_global_ticks==0)
        REPEAT_25(asm volatile("PAUSE" ::: "memory");)
    for(auto c:counters)
        c->reset();

	while(!stop)
	{

		/**
			Start receiving monitoring stats from emitter-worker-collector
			If there are more than one message in queue, we will take the last
			one
		*/

		receiveLast((void **)&em,e_inqueue);

		if(em->stop) //EOS
			stop=true;
        else
		{
			//receive from workers
			for(int i=0;i<num_workers;i++)
			{
				receiveLast((void **)&wm[i],w_inqueue[i]);	
				// check if  we are exiting from the program
				//this is the only occasion in which this event can occur
                if(wm[i]->tag==msg::MonitoringTag::EOS_TAG)//EOS ARRIVED
				{
					exiting=true;
					break;
				}
			}
			if(exiting)
			{
                CONTROL_PRINT(cout << "[CONTROLLER] exiting!"<<endl;)
				break; //come back to emitter data reading (we are exiting...)
			}
			
			receiveLast((void **)&cm,c_inqueue);

            /******************************************
                COMPUTATION OF METRICS
            *******************************************/
            metrics.updateMetrics(num_workers,em,wm,cm);

            //Energy: get current frequency (BY ASSUMPTION all the domains have the same frequency)
            current_frequency=domains.at(0)->getCurrentFrequencyUserspace();
            CONTROL_PRINT(cout<< fixed << std::setprecision(3) << ANSI_COLOR_BLUE "[CONTROLLER] Module's rho: "<<metrics.module_rho<<" ,Ta (msec): "<< metrics.tta_msec << ", Rate (TT/s): "<<1000/metrics.tta_msec<< ", Tcalc (msec): "<< metrics.module_tcalc << ", c_arr: "<<metrics.c_arr<<", c_serv: "<<metrics.c_serv<<", Frequency (KHz): "<<current_frequency<<ANSI_COLOR_RESET""<<endl;)
            if(sd->type!=StrategyType::NONE)
            {

                //note: the energy consumption will be taken just before applyng a possible reconfiguration
                rec_stats->addStats((double)(getticks()-*start_global_ticks)/(freq*1000000),num_workers,current_frequency);
                //Just for info (it is not mandatory)
                //Counts the total number of received elements per class
                //the total computation time and number of results for printing them on file
                for(int i=0;i<num_classes;i++)
                {

                    comp_stats->addElementsToClass(i,em->elements_per_class[i]);
                    for(int j=0;j<num_workers;j++)
                    {
                        if(wm[j]->tag==msg::MonitoringTag::MONITORING_TAG) //it is valid
                        {
                            //just for info
                            comp_stats->addCompTimeToClass(i,wm[j]->tcalc_per_class[i]);
                            comp_stats->addResultsToClass(i,wm[j]->computations_per_class[i]);
                        }
                    }
                }

                /*****************************************************
                    APPLICATION OF THE STRATEGY FOR THE RECONFIGURATION
                ******************************************************/


                if(sd->predictive) //Uses the predictive strategy
                {
                    //rate forecasting, will be conducted reasoning on a msec basis
                    //save the actual rate per second
                    rate_per_msecond.push_back(metrics.trigger_per_second/1000.0);
                    //lets compute the error  with the previously predicted rate (TMP)
                    mape.push_back(100*fabs(metrics.trigger_per_second/1000.0-forecasted[0])/(metrics.trigger_per_second/1000.0));

                    //Predict the next rate
                    forecast(rate_per_msecond,sd->horizon,forecasted); //NOTE: We are working on a millisecond base

                    forecasts.push_back(forecasted[0]);
                    //compute the kingman scaling function (ksf)
                    if(monitoring_step>2 )
                    {
                        // ksf=(cm->avg_lat/1000.0 - module_tcalc)/kingman_prev; //it is the ratio between the waiting time measured and the one expected (that was computed at the previous step)
                        ksf_samples[ksf_samples_idx]=(cm->avg_lat/1000.0 - metrics.module_tcalc)/kingman_prev; //the response time expected and computed at the previous step

                        ksf=(ksf_samples[0]+ksf_samples[1]+ksf_samples[2])/3;
                        ksf_samples_idx=(ksf_samples_idx+1)%3;
                    }

                    long start_pred=current_time_usecs();

                    //resp time
                    if(sd->type==StrategyType::LATENCY)
                    {
                        predict_reconf_rt(sd,max_workers,num_workers,forecasted,metrics.module_tcalc,metrics.c_arr,metrics.c_serv,ksf,pred_trajectory,&exp_rt, &kingman_prev);
                        //TMP just for statics and check the kingman evaluation

                        //ATTENTION: The actual service time is given by the average tcalc/N
                        king_values.push_back(kingman_prev+metrics.module_tcalc); //TODO: va capito bene come valutarlo
                        exp_resp_times.push_back(exp_rt);
                    }
                    // printf("According to the strategy the next par degree should be: %d, Expected rt: %f\n",pred_trajectory[0],exp_rt);

                    if(sd->type==StrategyType::LATENCY_ENERGY)
                    {
                        predict_reconf_energy_rt(sd,max_workers,available_frequencies,voltages, num_workers,current_frequency,forecasted,metrics.module_tcalc,metrics.c_arr,metrics.c_serv,ksf,pred_trajectory_energy,&exp_rt, &kingman_prev);
                        king_values.push_back(kingman_prev+metrics.module_tcalc);
                        exp_resp_times.push_back(exp_rt);
                    }

                    // predict_reconf_nc(PRED_HORIZON, max_workers,num_workers,forecasted,module_tcalc, max_tcalc_sum,  rt_max_threshold,  nc_sf, pred_trajectory, &exp_max_rt, &nc_prev);
                    // printf("According to the strategy the next par degree should be: %d, Expected max rt: %f NC:%f\n",pred_trajectory[0],exp_max_rt,nc_prev);

                    CONTROL_PRINT(cout << "[CONTROLLER] Strategy resolution time (usec): "<<(current_time_usecs()-start_pred)<<endl;)

                    //w/o energy
                    if( sd->type==StrategyType::LATENCY)
                        n_opt=pred_trajectory[0];
                    else      //energy version
                    {
                       n_opt=pred_trajectory_energy[0].nw;
                       freq_opt=pred_trajectory_energy[0].freq;
                    }
                }
                else

                    if(sd->type==StrategyType::TPDS)
                    {
                        //we use the congestione measured by the emitter
                        bool congestion=em->congestion;
                        int thr=(cm->results/(sd->control_step/1000.0));
                        n_opt=predict_tpds(sd,monitoring_step,&current_level,thr,congestion,p_i,c_i,thr_last_i,thr_first_i,s);
                    }
                    else
                        if(sd->type==StrategyType::LATENCY_RULE)
                        {
                                //strategy latency-oriented based on rules
                                if(cm->avg_lat/1000.0 > sd->threshold*1.1)
                                    n_opt=MIN(num_workers+1,max_workers);
                                if(cm->avg_lat/1000.0 < sd->threshold*0.7)
                                    n_opt=MAX(num_workers-1,1);

                        }
                        else
                        {
                            //the optimal par degree is simply computed as the ratio between the tcalc and the interarrival time
                            //(properly corrected)

                            //compute optimal parallelism degree
                            if(metrics.module_tcalc>0 && metrics.tta_msec>0) //data are valid
                                n_opt=ceil(metrics.module_tcalc/(metrics.tta_msec*MAX_RHO_MODULE));
                            else
                                n_opt=num_workers;
                        }

                //some checks
                if(n_opt<=0) //(something is going wrong... n_opt must be at least 1)
                {
                    CONTROL_PRINT(cout<<ANSI_COLOR_RED_BOLD<< "Humh...something goes wrong: n_opt must be at least 1"<< ANSI_COLOR_RESET<<endl;)
                    n_opt=1;
                }
                if(n_opt>max_workers) //we have not sufficient resource
                    n_opt=max_workers;

                /*
                    Take note of the energy consumed (just before applying some reconfiguration)
                    and reset the counters
                */
                joulesCore=0;
                joulesCpu=0;
                for(auto c:counters)
                {
                    joulesCore+=c->getJoulesCores();
                    joulesCpu+=c->getJoulesCpu();

                    c->reset();
                }
                totalJoulesCores+=joulesCore;
                totalJoulesCpu+=joulesCpu;
                rec_stats->addEnergyStats(joulesCore,joulesCpu);


                if(monitoring_step>1)
                {

                    //if needed, change the frequency (note: we can change the frequency but mantain the par degree)
                    if(freq_opt!=freq_pred)
                    {
                        cout << ANSI_COLOR_YELLOW "[Reconfiguration] Changing frequency to "<<freq_opt/1000.0<<" MHz"<<endl;
                        CONTROL_PRINT(cout << ANSI_COLOR_YELLOW "[CONTROLLER] Changing frequency to "<<freq_opt<<endl;)

                        for(int i=0;i<domains.size();i++)
                        {
                            //printf("Setto su dominio: %d\n",i);
                            if(domains.at(i)-> setFrequencyUserspace(freq_opt)==false)
                            {
                                cerr << ANSI_COLOR_RED "[CONTROLLER] Error in setting frequency " ANSI_COLOR_RESET<<endl;
                            }
                        }
                        freq_pred=freq_opt;
                        if(monitoring_step>=space_resvd_reconf_at_step)
                        {
                            space_resvd_reconf_at_step+=100;
                            reconf_at_step.resize(space_resvd_reconf_at_step);
                        }
                        reconf_at_step[monitoring_step]=true;
                    }


                    //cleanup: delete old monitoring message
                    for(int i=0;i<num_workers;i++)
                        delete(wm[i]);

                    int changes=n_opt-num_workers;
                    threads_spawned+=changes;

                    if(changes!=0)
                    {
                        if(monitoring_step>=space_resvd_reconf_at_step)
                        {
                            space_resvd_reconf_at_step+=100;
                            reconf_at_step.resize(space_resvd_reconf_at_step);
                        }
                        reconf_at_step[monitoring_step]=true;
                    }

                    if(!reconf_at_step[monitoring_step]&& !reconf_at_step[monitoring_step-1])
                    {
                        //No reconfiguration has been planned for this step neither at the previous one.
                        //However we could have some
                        //load unbalancing on the workers that we can solve by rebalancing the scheduling table
                        //We don't rebalance after a reconfiguration since maybe the metrics for worker are not
                        //already stabilized

                        bool reconfigure=false;
                        double rho_min=1.5, rho_max=0;
                        for(int i=0;i<num_workers;i++)
                        {
                            /*if(metrics.rho[i]>MAX_RHO)
                                reconfigure=true;*/
                            if(metrics.rho[i]>rho_max)
                                rho_max=metrics.rho[i];
                            if(metrics.rho[i]<rho_min)
                                rho_min=metrics.rho[i];
                        }
                        if(rho_max>MAX_RHO_WORKER || rho_max/rho_min>1+((double)MAX_RHO_UNBALANCE_PERCENTAGE)/100.0)
                            reconfigure=true;

                        if(reconfigure && sd->type!=StrategyType::TPDS) //do no take it into account for the tpds strategy
                        {
                                num_rebalancing++;
                                reconf_at_step[monitoring_step]=true;
                                reconf_start_t=current_time_usecs();
                                //create the message: scheduling table is copied since still used by the emitter

                                msg::ReconfEmitter *reconf_data_em=new msg::ReconfEmitter(0,num_classes,em-> scheduling_table);
                                CONTROL_PRINT(cout<< ANSI_COLOR_BLUE_REVERSE "[CONTROLLER] Rebalancing classes... " ANSI_COLOR_RESET<<endl;)
                                compute_fb_st(num_workers, num_classes,metrics.weighted_tcalc_per_class,reconf_data_em->scheduling_table);
//                                 compute_st_flux(num_workers, num_workers, num_classes,metrics.weighted_tcalc_per_class,reconf_data_em->scheduling_table);

                                bsend(reconf_data_em,e_outqueue);

                                //wait for the reply from the emitter
                                do
                                {
                                    delete(em);
                                    receive((void **)&em,e_inqueue);
                                }while((em->tag!=msg::MonitoringTag::RECONF_FINISHED_TAG));
                                //QUI DOVREMMO GARANTIRE CHE UNA NUOVA RICONFIGURAZIONI NON INIZI PRIMA CHE LA PRECEDENTE TERMINI
                                //Wait for completetion of reconfiguration on Worker Side
                                repository->waitReconfFinished();
                                CONTROL_PRINT(cout << ANSI_COLOR_CYAN << "[CONTROLLER] Reconfiguration finished in "<< current_time_usecs()-reconf_start_t<<" usecs"<<endl;)

                                delete(em);
                                delete(cm);
                        }
                        else
                            reconf_at_step[monitoring_step]=false;
                    }

                    if(changes>0) //increase the number of workers
                    {
                        CONTROL_PRINT(cout <<ANSI_COLOR_YELLOW "[CONTROLLER] spawning "<< changes <<" workers" ANSI_COLOR_RESET<<endl;)
                        cout << ANSI_COLOR_YELLOW "[Reconfiguration] Add "<<changes<<" replica(s)"<<endl;
                        //changes=max_workers-num_workers;


                        reconf_start_t=current_time_usecs();

                        //just check if the Emitter has just terminated the computation
                        //it could happen that the Emitter has just finished: if we spawn some thread
                        //there will be a deadlock since the collector may wait for message from these new threds.
                        //Therefore we check into the queue if there is a stop message from the Emitter, in that case
                        //we don't do anything
                        top_em=(msg::EmitterMonitoring *)e_inqueue->top();
                        if(top_em==NULL  || !top_em->stop) //the EOS is not arrived
                        {

                            //create the message for emitter:
                            msg::ReconfEmitter *reconf_data_em=new msg::ReconfEmitter(changes,num_classes,em-> scheduling_table);
                            msg::ReconfCollector *reconf_data_c=new msg::ReconfCollector(changes);
                            worker_data=(worker_data_t *)malloc(sizeof(worker_data_t)*changes);


                            for(int i=0;i<changes;i++)
                            {
                                reconf_data_em->wqueues[i]=new SWSR_Ptr_Buffer(QUEUE_SIZE);
                                reconf_data_em->wqueues[i]->init();
                                reconf_data_c->wqueues[i]=new SWSR_Ptr_Buffer(QUEUE_SIZE);
                                reconf_data_c->wqueues[i]->init();
                                //assuming that there is enough space, define queue worker->controller
                                w_inqueue[num_workers+i]=new SWSR_Ptr_Buffer(QUEUE_SIZE_MON);
                                w_inqueue[num_workers+i]->init();
                            }

                            //thread spawning
                            for(int i=0;i<changes;i++)
                            {
                                pthread_t tid;
                                worker_data[i].workerId = i+num_workers;
                                worker_data[i].inqueue = reconf_data_em->wqueues[i];
                                worker_data[i].outqueue = reconf_data_c->wqueues[i];
                                worker_data[i].barrier=NULL; //in this way, newly spawned threads will not perform wait on barrier
                                worker_data[i].window_size=window_size;
                                worker_data[i].window_slide=window_slide;
                                worker_data[i].freq=freq;
                                worker_data[i].num_classes=num_classes;
                                worker_data[i].start_global_ticks=start_global_ticks;
                                worker_data[i].sd=sd;
                                #if defined(USE_FFALLOC)
                                    worker_data[i].ffalloc=ffalloc;
                                #endif

                                worker_data[i].cn_outqueue=w_inqueue[num_workers+i];
                                worker_data[i].repository=repository;
                                pthread_create(&tid, NULL, worker, &(worker_data[i]));
                                //set CPU affinity of worker threads
                                CPU_ZERO(&cpuset);
                                CPU_SET(affinities[i+num_workers], &cpuset);

                                if (pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset)) {
                                    cerr << "Cannot set thread to CPU " << i << endl;
                                }
                                //worker_data[i].num_workers = num_workers;
                                wtids[tid_idx] = tid;
                                tid_idx++;
                            }

                            num_workers+=changes;

                            //compute the optimal scheduling table and the expected load (load_w will containt the expected wtcalc_per_worker)

                            compute_fb_st(num_workers, num_classes,metrics.weighted_tcalc_per_class,reconf_data_em->scheduling_table);

                            //compute_st_flux(num_workers, num_workers-changes,num_classes,metrics.weighted_tcalc_per_class,reconf_data_em->scheduling_table);


                            //send the data toward the emitter
                            bsend(reconf_data_em,e_outqueue);

                            //communication toward the collector
                            bsend(reconf_data_c,c_outqueue);
                            //delete the previous message received from the emitter
                            do
                            {
                                delete(em); //delete previous uneeded message
                                receive((void **)&em,e_inqueue);
                                //fprintf(stderr,"Reconfiguration error\n");
                                //exit(RECONF_ERR);
                            }while((em->tag!=msg::MonitoringTag::RECONF_FINISHED_TAG && em->tag!=msg::MonitoringTag::EOS_TAG));
                            if(em->tag==msg::MonitoringTag::EOS_TAG)
                            {
                                stop=true;
                                break;
                            }
                            do
                            {
                                delete(cm); //delete previous uneeded message
                                receive((void **)&cm,c_inqueue);
                            }while((cm->tag!=msg::MonitoringTag::RECONF_FINISHED_TAG && cm->tag!=msg::MonitoringTag::EOS_TAG));
                            if(cm->tag==msg::MonitoringTag::EOS_TAG)
                            {
                                stop=true;
                                break;
                            }

                            //Receive the ack from the Workers that their reconfigurations are completed in order to assure
                            //that a new reconfiguration can not start if the previous one is not yet completed

                             repository->waitReconfFinished();

                            CONTROL_PRINT(cout << ANSI_COLOR_YELLOW "[CONTROLLER] increased par degree in " << current_time_usecs()-reconf_start_t<<" usecs" ANSI_COLOR_RESET<<endl;)

                            //message cleenup
                            delete(em);
                            delete(cm);
                        }
                        //else an EOS has arrived, don't do anything
                    }

                    if(changes<0) //we have to erase some worker
                    {
                        CONTROL_PRINT(cout <<ANSI_COLOR_YELLOW "[CONTROLLER] deleting "<< abs(changes) <<" workers" ANSI_COLOR_RESET<<endl;)
                        cout << ANSI_COLOR_YELLOW "[Reconfiguration] Remove "<<abs(changes)<<" replica(s)"<<endl;
                        reconf_start_t=current_time_usecs();
                        //just check if the Emitter has just terminated the computation
                        top_em=(msg::EmitterMonitoring *)e_inqueue->top();
                        if(top_em==NULL  || !top_em->stop) //the EOS is not arrived
                        {
                            //create the message
                            msg::ReconfEmitter *reconf_data_em=new msg::ReconfEmitter(changes,num_classes,em-> scheduling_table);
                            msg::ReconfCollector *reconf_data_c=new msg::ReconfCollector(changes);

                            num_workers+=changes;

                            //compute the new scheduling table
                            compute_fb_st(num_workers, num_classes,metrics.weighted_tcalc_per_class,reconf_data_em->scheduling_table);
    //                         compute_st_flux(num_workers,num_workers-changes, num_classes,metrics.weighted_tcalc_per_class,reconf_data_em->scheduling_table);
                             //
                            //send the data toward the emitter
                            bsend(reconf_data_em,e_outqueue);

                            //communication toward the collector
                            bsend(reconf_data_c,c_outqueue);

                            //delete the previous message
                            //wait for the reply from the emitter
                            do
                            {
                                delete(em);
                                receive((void **)&em,e_inqueue);
                                //fprintf(stderr,"Reconfiguration error\n");
                                //exit(RECONF_ERR);
                            }while((em->tag!=msg::MonitoringTag::RECONF_FINISHED_TAG && em->tag!=msg::MonitoringTag::EOS_TAG));
                            if(em->tag==msg::MonitoringTag::EOS_TAG)
                            {
                                stop=true;
                                break;
                            }
                            //printf("Controller: emitter ha finito\n");
                            do
                            {
                                delete(cm);
                                receive((void **)&cm,c_inqueue);
                            }while((cm->tag!=msg::MonitoringTag::RECONF_FINISHED_TAG && cm->tag!=msg::MonitoringTag::EOS_TAG));
                            if(cm->tag==msg::MonitoringTag::EOS_TAG)
                            {
                                stop=true;
                                break;
                            }

                            //Receive the ack from the Workers that their reconfigurations are completed in order to assure
                            //that a new reconfiguration can not start if the previous one is not yet completed
                            repository->waitReconfFinished();
                            for(int i=num_workers;i<num_workers-changes;i++)
                            {
                                //receive the last message (while exiting) by the workers that have been removed
                                do
                                {

                                    receiveLast((void **)&wm[i],w_inqueue[i]);
        //                            std::cout << "tag: "<<(wm[i]->tag==msg::MonitoringTag::EOS_TAG?0:-1)<<std::endl;
                                }while(wm[i]->tag!=msg::MonitoringTag::EOS_TAG);
                                //free queue from workers
                                delete(w_inqueue[i]);
                                delete(wm[i]);

                                w_inqueue[i]=NULL;
                                if(tid_idx>0)
                                    tid_idx--;
                            }

                            CONTROL_PRINT(cout<<ANSI_COLOR_YELLOW<< "[CONTROLLER] decreased par degree in "<<current_time_usecs()-reconf_start_t<<" usecs"<< ANSI_COLOR_RESET<<endl;)
                            //message cleenup
                            delete(em);
                            delete(cm);
                        }
                    }
                }
            }
		}
		monitoring_step++;		
	}

	//wait for the threads that have been spawned
	// Wait for the completion of the threads
    void *retval ;
    for (int i = 0; i < tid_idx; i++) {
        pthread_join(wtids[i], &retval);
    }

	//print to file classes statistics
    /*char out_file[100];
    sprintf(out_file,"%s_%s.dat",CLASS_FILE,suffix);
    comp_stats->writeToFile(out_file);
    if(sd->predictive)
    {
        FILE *fout=fopen("forecast.dat","w");
        for(int i=0;i<forecasts.size();i++)
            fprintf(fout,"%.4f %.4f\n",rate_per_msecond[i],forecasts[i]);
        fclose(fout);

        if(sd->type==StrategyType::LATENCY || sd->type==StrategyType::LATENCY_ENERGY)
        {
            fout=fopen("kingman.dat","w");
            for(int i=0;i<king_values.size();i++)
                fprintf(fout,"%.4f\t%.4f\n",king_values[i],exp_resp_times[i]);
            fclose(fout);
        }

    }
    //save the numb of class rebalancing
    rec_stats->setNumClassRebalancing(num_rebalancing);*/

    return rec_stats;
}

//HWFilter filter;
//for the real dataset alpha=0.67, beta=0.26

//const double holt_winter_alpha=1;             //alpha parameter of the HoltWinter filter
//const double holt_winter_beta=0.015;              //beta parameter 0.015 for rw, 0.9 for sin
inline void forecast(vector<double> obs_v,int h, double *forecasted)
{

    /*
        Simple Moving Average (SMA) VERSION
    */
     int length=3;

    if(obs_v.size()<length)
    {
        forecasted[0]=obs_v[obs_v.size()-1];
        return;
    }
    for(int i=0;i<h;i++)
    {
        forecasted[i]=0;
        //add the obs value
        for(int j=0;j<length-i;j++)
        {
            //printf("Step: %d, i: %d, aggiungo l'elemento: %d\n",i,obs_v.size(),obs_v.size()-length+j+i);
            forecasted[i]+=obs_v[obs_v.size()-length+j+i];
        }
        //add the already forecasted values (they are h+1)
        for(int j=0;j<i;j++)
        {
            forecasted[i]+=forecasted[j];
        }
        forecasted[i]/=length;
    }

    /*
        Holt Winter version
    */

   /* if(obs_v.size()<2) //not enough data
    {
        forecasted[0]=obs_v[obs_v.size()-1];
        return;
    }
    if(obs_v.size()==2)		//we can initialize the filter
        filter.initialize(holt_winter_alpha,holt_winter_beta,obs_v[0],obs_v[1]-obs_v[0]);
    else
        filter.updateSample(obs_v[obs_v.size()-1]);

    filter.forecast(forecasted,h);*/

}

