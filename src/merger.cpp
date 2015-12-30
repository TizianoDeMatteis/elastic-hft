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

    Merger (aka collector) for elastic-hft
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <vector>
#include <sched.h>
#include <algorithm> 
#include <ff/buffer.hpp>
#include <ff/allocator.hpp>
#include "../includes/random_generator.h"
#include "../includes/general.h"
#include "../includes/elastic-hft.h"
#include "../includes/messages.hpp"
#include "../includes/statistics.hpp"
#include "../includes/strategy_descriptor.hpp"

using namespace ff;
using namespace std;
void * collector(void *args) {

    //Init: take data from main
	collector_data_t *data = (collector_data_t *) args;
	long int freq=data->freq;
	char *suffix=data->suffix;
	ticks *start_global_ticks=data->start_global_ticks;
    int received_EOS=0; //number of workers from which the collector has received the EOS
    winresult_t *rcvd=(winresult_t*)malloc(sizeof(winresult_t));
    int index=0;
    double dlat=0.0; //latency in usec
    int64_t rcvd_results=0;
    int partial_rcvd=0;
    long start_usecs,last_print;
    long  print_rate=(long)PRINT_RATE*1000; //NON TOCCARE, ALTRIMENTI DEVI SISTEMARE IL CALCOLO DELLA LATENZA MONITORATA
	int window_slide=data->window_slide;
	int num_workers=data->num_workers;
	int num_classes=data->num_classes;
	int max_workers=data->max_workers;
    StrategyDescriptor *sd=data->sd;


	//vector for computing response time percentile (maybe we can find something more efficient)
	vector<double> percentile_lat;
	//Ordering variables
	//expected internal id for the received results (it will be used for correctness)
    int64_t *expected_iid=new int64_t[num_classes]();
	//number of results received disordered partial results for each class
    int *disordered=new int[num_classes]();
	//buffer for results received out of order
    vector<vector<winresult_t>> buffer_disordered;
    //initialize them
    for(int i=0;i<num_classes;i++)
        expected_iid[i]=window_slide-1; //since the ids start from 0
    for(int i=0;i<num_classes;i++)
        buffer_disordered.emplace_back(5);

	//latencies, percentiles, actual par degree... all these metrics are printed in a file
	//at the end of the program (on a second basis)

    stats::ExecutionStatistics *stats=new stats::ExecutionStatistics();
    //for printing statics on throughput of single workers
    int *recvd_per_worker=new int[max_workers]();
    //latency filename
    char lat_out_file[100];
    sprintf(lat_out_file,"%s_%s.dat",LAT_FILE,suffix);

    //stats on service time
    stats::RunningStat stat_service_time;
    //statistics on Mean and Standard dev of single latencies
    stats::RunningStat latency_stats; //these are kept in usecs
    //tmp value for statics that have to be saved
    double to_save_lat95, to_save_lat99,to_save_lat_top;

	pthread_barrier_t *barrier = data->barrier;
	SWSR_Ptr_Buffer **inqueue=data->inqueue;
    //the queue for sending monitoring data to the controller
    SWSR_Ptr_Buffer *cn_outqueue=data->cn_outqueue;

    //define the data structure for monitoring
    long monitoring_timer;
    long monitoring_step_usecs=sd->control_step*1000; //express it in usecs
    msg::CollectorMonitoring *monitoring=nullptr;
    SWSR_Ptr_Buffer *cn_inqueue=nullptr;
    msg::ReconfCollector *reconf_data;
    bool reconf_phase_pard_down=false; //if it is true, it means that we are in a reconfiguration phase in which we have to terminate some worker
    char work_down_degree;

    #if defined(MONITORING)
        monitoring=new msg::CollectorMonitoring(num_classes);
        if(sd->type!=StrategyType::NONE)
        {
			//in the case of the adaptive version we have also incoming data (the reconfiguration commands) from the controller
            cn_inqueue=data->cn_inqueue;
        }

	#endif

    #if defined(SAVE_RESULTS)
    //file in which store the results
    FILE *fresults=fopen("interp_results.dat","w");
    FILE *fresults_candle=fopen("candle_sticks.dat","w");
    fprintf(fresults,"#Key\tCoeff_0 Ask\tCoeff_1 Ask\tCoeff_2 Ask\tCoeff_0 Bid\tCoeff_1 Bid\tCoeff_2 Bid\n");
    fprintf(fresults_candle,"#Key\tType\tOpen\tClose\tLow\tHigh\n");
    #endif

	//synchronization barrier
	DEBUG(printf("Collector ready, running on %d\n",sched_getcpu()));
	pthread_barrier_wait(barrier);
	
	/**
		Start receiving from Workers
	*/
	
	while(*start_global_ticks==0)
		REPEAT_25(asm volatile("PAUSE" ::: "memory");) //wait for the correct time
    long first_tuple_timestamp=*data->first_tuple_timestamp;
    long start_global_usecs=*(data->start_global_usecs);
	#if defined(MONITORING)
        monitoring_timer=start_global_usecs+monitoring_step_usecs;
	#endif
    last_print=start_global_usecs;
    start_usecs=current_time_usecs();

    long start_nsecs=current_time_nsecs();
    long last_recvd_nsecs=start_nsecs;
    long curr_nsecs;

	while(received_EOS<num_workers) //the collector has to receive the EOS from all the workers
	{
		//Round-robin polling from the various workers
		if(inqueue[index]->pop((void **)&rcvd)) 
        {
            //cout <<"Result arrived"<<endl;
            #if defined(MONITORING)
            //save the value of service time as the time elapsed from the last reception (nsecs)
            curr_nsecs=current_time_nsecs();
            stat_service_time.Push((curr_nsecs-last_recvd_nsecs)/1000000.0);
            last_recvd_nsecs=curr_nsecs;

            #endif

			//something has been received from a worker
            if(rcvd->isEOS)
			{				
                if(sd->type!=StrategyType::NONE)
                {
                    if(!reconf_phase_pard_down && cn_inqueue->pop((void **)(&reconf_data)))
                    {
                        //This is a particular case in which an EOS has arrived before the message from the controller
                        if(reconf_data->tag==msg::ReconfTag::DECREASE_PAR_DEGREE)
                        {
                            //we should expect some EOS from them
                            reconf_phase_pard_down=true;
                            work_down_degree=reconf_data->par_degree_changes;
                        }
                    }
                    //If we are in a reconfiguration phase in which we downgrade the parallelism degree
                    //we should check if the EOS is arriving from a worker that has to be deleted
                    if(reconf_phase_pard_down && index>=num_workers+reconf_data->par_degree_changes)
                    {

                        work_down_degree++;
                        //free the result buffer of the worker (if we free it on worker side, we would not be able to read the message from this side)
                        free(rcvd->res_buff); //todo: brutto ma necessario se vogliamo riciclare i dati
                        if(work_down_degree==0) //reconf finished
                        {

                            for(int i=0;i>reconf_data->par_degree_changes;i--) //(att: par_degree_cahnges<0)
                            {
                                //destroy the queues
                                delete(inqueue[num_workers+i-1]);
                                //set to NULL outqueues (they will destructed by workers)
                                inqueue[num_workers+i-1]=NULL;
                            }

                            //set the new num_workers
                            num_workers+=reconf_data->par_degree_changes;
                            reconf_phase_pard_down=false;
                            //send ack to the controller
                            msg::CollectorMonitoring *reconf_finished=new msg::CollectorMonitoring(0); //we don't need additional data
                            reconf_finished->tag=msg::MonitoringTag::RECONF_FINISHED_TAG;
                            bsend(reconf_finished,cn_outqueue);
                            DEBUG(cout<<ANSI_COLOR_YELLOW "[COLLECTOR] reconf finished" ANSI_COLOR_RESET<<endl;)
                        }

                    }
                    else
                        received_EOS++;
                }
                else
                {
                    received_EOS++;
                }
			}
			else
			{
				//standard result coming from a Worker
				rcvd_results++;
				partial_rcvd++;
				recvd_per_worker[rcvd->wid]++;

				if((rcvd->id)>expected_iid[rcvd->type])
				{
					//we have received a partial result out of order
					//we have to buffer it and reuse when necessary
					//CURRENTLY IT IS IMPLEMENTED, BUT NOT INTENSIVELY TESTED

					//buffer it
                    //cerr<<"[CONTROLLER] Received partial result disordered for class: "<<rcvd->type <<" from Worker "<<index <<" expected: " << expected_iid[rcvd->type] <<" received: "<<rcvd->id <<endl;
					//insert it into the right position (in order)
					buffer_disordered[rcvd->type].push_back(*rcvd);
					disordered[rcvd->type]++;
				}
				else
				{
					//IN REAL IMPLEMENTATIONS, the result will be sent afterward to the next module
                    //e.g. save data to file for succesive computation
                    #if defined(SAVE_RESULTS)
                    fprintf(fresults,"%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\n",rcvd->type,rcvd->p0_ask,rcvd->p1_ask,rcvd->p2_ask,rcvd->p0_bid,rcvd->p1_bid,rcvd->p2_bid);
                    fprintf(fresults_candle,"%d\tASK\t%.3f\t%.3f\t%.3f\t%.3f\n",rcvd->type,rcvd->open_ask,rcvd->close_ask,rcvd->low_ask,rcvd->high_ask);
                    fprintf(fresults_candle,"%d\tBID\t%.3f\t%.3f\t%.3f\t%.3f\n",rcvd->type,rcvd->open_bid,rcvd->close_bid,rcvd->low_bid,rcvd->high_bid);
                    #endif

                    //the latency is computed as the difference between the time elapsed from the beginning of the
                    //operator computattion and the difference between the tuples' timestamps
                    //the original timestamps are reported in usecs

                    long lat=((current_time_usecs()-start_global_usecs)-(rcvd->timestamp-first_tuple_timestamp));

					percentile_lat.push_back(lat);

                    //compute average latency and standard dev
					dlat+=lat;
                    latency_stats.Push(lat);
					#if defined (MONITORING)
                        monitoring->results++;
                        monitoring->results_per_class[rcvd->type]++;
                        monitoring->latency_per_class[rcvd->type]+=lat; //usec
					#endif
					expected_iid[rcvd->type]+=window_slide;

					if(disordered[rcvd->type]>0) //previously we received disordered results: the idea is that we sent all the results
					{
						//look if there is the next in the vector
						for(int i=0;i<buffer_disordered[rcvd->type].size();i++)
						{
							if(buffer_disordered[rcvd->type][i].id==expected_iid[rcvd->type])
							{
								//ok it is the right one
								//send afterwards
								//....
                                #if defined(SAVE_RESULTS)
                                fprintf(fresults,"%d\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\n",rcvd->type,rcvd->p0_ask,rcvd->p1_ask,rcvd->p2_ask,rcvd->p0_bid,rcvd->p1_bid,rcvd->p2_bid);
                                fprintf(fresults_candle,"%d\tASK\t%.3f\t%.3f\t%.3f\t%.3f\n",rcvd->type,rcvd->open_ask,rcvd->close_ask,rcvd->low_ask,rcvd->high_ask);
                                fprintf(fresults_candle,"%d\tBID\t%.3f\t%.3f\t%.3f\t%.3f\n",rcvd->type,rcvd->open_bid,rcvd->close_bid,rcvd->low_bid,rcvd->high_bid);
                                #endif
                                double lat=(double)((current_time_usecs()-start_global_usecs)-(buffer_disordered[rcvd->type][i].timestamp-first_tuple_timestamp));
								dlat+=lat;
								#if defined (MONITORING)
                                    monitoring->results++;
                                    monitoring->results_per_class[rcvd->type]++;
                                    monitoring->latency_per_class[rcvd->type]+=lat; //usec
								#endif
								expected_iid[rcvd->type]+=window_slide;
								//erase the element
								buffer_disordered[rcvd->type].erase(buffer_disordered[rcvd->type].begin()+i);
								disordered[rcvd->type]--;
								//reset i to zero, we have to start from the beginning
								i=0;
                               // cerr<<"[CONTROLLER] Received partial result out of order for class: "<<rcvd->type<<" problem resolved!"<<endl;
							}
						}
					}
				}
			}
		}
		
		//check if we have to print
        if(current_time_usecs()-last_print>print_rate)
		{
            double avglat=latency_stats.Mean();
            double std_dev_lat=latency_stats.StandardDeviation();
            last_print=current_time_usecs();
            int indexp=0;
			//sort percentiles
            double curr_sec=((double)(last_print-start_usecs))/1000000.0;
            //for the moment being we handle this here
			if(percentile_lat.size()>0)

			{
				sort(percentile_lat.begin(), percentile_lat.end()); 
                indexp=percentile_lat.size()*0.95;

                //cout<< fixed<<std::setprecision(3)<<ANSI_COLOR_GREEN "Time: "<< curr_sec <<", Num Replica: "<<num_workers<<", recvd results: "<<partial_rcvd <<", avg latency (usec): "<< avglat<<", 95-percentile: "<<percentile_lat[indexp]<<" (top: " <<percentile_lat[percentile_lat.size()-1] << ")" ANSI_COLOR_RESET<<endl;
                cout<< fixed<<std::setprecision(3)<<ANSI_COLOR_GREEN "Time: "<< curr_sec <<", Num Replicas: "<<num_workers<<", recvd results: "<<partial_rcvd <<", avg latency (usec): "<< avglat<< ANSI_COLOR_RESET<<endl;

                #if defined(MONITORING)
                monitoring->lat_95+=percentile_lat[indexp];
                #endif
                // monitoring->lat_95+=percentile_lat[percentile_lat.size()-1];
			}
			else
                cout<< fixed<<std::setprecision(3)<<ANSI_COLOR_GREEN "Time: "<< curr_sec <<", Num Replicas: "<<num_workers<<", recvd results: "<<partial_rcvd <<", avg latency (usec): "<< avglat<< ANSI_COLOR_RESET<<endl;
			
            dlat=0;
			//throughput per worker
            /*cout << ANSI_COLOR_GREEN "Thr per Worker: ";
			for(int i=0;i<num_workers;i++)
                cout << recvd_per_worker[i]<<"\t";
            cout << ANSI_COLOR_RESET ""<<endl;*/

            //save metrics for print them to file
            #if defined(MONITORING)
            if(percentile_lat.size()>0)
            {
                to_save_lat95=percentile_lat[indexp];
                to_save_lat99=percentile_lat[(int)(percentile_lat.size()*0.99)];
                to_save_lat_top=percentile_lat[percentile_lat.size()-1];
            }
            else{
                to_save_lat95=0;
                to_save_lat99=0;
                to_save_lat_top=0;
            }
            stats->addStat(((double)(current_time_usecs()-start_usecs))/1000000.0,partial_rcvd,avglat,to_save_lat95,to_save_lat99,to_save_lat_top,std_dev_lat);
            monitoring->avg_lat+=avglat; //sign it in order to take into account the average lat per mon step
            #endif
			partial_rcvd=0;
			percentile_lat.clear();
            latency_stats.Clear();
			memset(recvd_per_worker,0,num_workers*sizeof(int));

        }
        #if defined(MONITORING)
        //check if we can send monitoring data to controller
        if(current_time_usecs()>monitoring_timer)
        {

            monitoring_timer=current_time_usecs()+monitoring_step_usecs;

            asm volatile("":::"memory");
            // printf("Monitoring data %d are valid. Elements received: %Ld\n",mon_ptr,monitoring[mon_ptr].elements);
            //compute average latencies
            for(int i=0;i<num_classes;i++)
                monitoring->latency_per_class[i]=((monitoring->latency_per_class[i])/(monitoring->results_per_class[i]));
            monitoring->monitoring_time=current_time_usecs();
            //compute the average latency
            monitoring->avg_lat/=sd->control_step/1000.0;
            monitoring->lat_95/=sd->control_step/1000.0;
            monitoring->c_serv=stat_service_time.StandardDeviation()/stat_service_time.Mean();
            //blocking send towards the collector
            bsend(monitoring,cn_outqueue);
            //create a new message
            monitoring=new msg::CollectorMonitoring(num_classes);
            //printf("Collector sent step: %d, lqueue:%d\n",mon_step++,cn_outqueue->length());
            stat_service_time.Clear();
        }
        #endif

		
		//check if there are reconfiguration messages from the controller
        if(sd->type!=StrategyType::NONE)
        {
            if(cn_inqueue->pop((void **)(&reconf_data)))
			{
				
                if(reconf_data->tag==msg::ReconfTag::INCREASE_PAR_DEGREE)
				{
					//if there are newly spanned threads, copy their queues
                    DEBUG(cout <<ANSI_COLOR_YELLOW "[COLLECTOR] Received message: add "<<reconf_data->par_degree_changes<<" workers" ANSI_COLOR_RESET<<endl;)
					
					//take the new queues
					for(int i=0;i<reconf_data->par_degree_changes;i++)
						inqueue[num_workers+i]=reconf_data->wqueues[i];
					//increments the par degree
					num_workers+=reconf_data->par_degree_changes;
                    //ok, notify to the collector that the reconfiguration has finished
                    msg::CollectorMonitoring *reconf_finished=new msg::CollectorMonitoring(0);
                    reconf_finished->tag=msg::MonitoringTag::RECONF_FINISHED_TAG;
                    bsend(reconf_finished,cn_outqueue);
				}
				else
                    if(reconf_data->tag==msg::ReconfTag::DECREASE_PAR_DEGREE)
					{
                        DEBUG(cout <<ANSI_COLOR_YELLOW "[COLLECTOR] Received message: remove "<<abs(reconf_data->par_degree_changes)<<" workers" ANSI_COLOR_RESET<<endl;)
						//we should expect some EOS from them
						reconf_phase_pard_down=true;
						work_down_degree=reconf_data->par_degree_changes;
					}
			}
        }
		index=(index+1)%num_workers;
	}
	
	//last print
    double avglat=latency_stats.Mean();
    double std_dev_lat=latency_stats.StandardDeviation();
	//order percentile
    int indexp=percentile_lat.size()*0.95;
    double curr_sec=((double)(last_print-start_usecs))/1000000.0;

	if(percentile_lat.size()>0)
	{
		sort(percentile_lat.begin(), percentile_lat.end()); 
        //cout<< fixed<<std::setprecision(3)<<ANSI_COLOR_GREEN "Time: "<< curr_sec <<", Num Replica: "<<num_workers<<", recvd results: "<<partial_rcvd <<", avg latency (usec): "<< avglat<<", 95-percentile: "<<percentile_lat[indexp]<<" (top: " <<percentile_lat[percentile_lat.size()-1] << ")" ANSI_COLOR_RESET<<endl;
        cout<< fixed<<std::setprecision(3)<<ANSI_COLOR_GREEN "Time: "<< curr_sec <<", Num Replicas: "<<num_workers<<", recvd results: "<<partial_rcvd <<", avg latency (usec): "<< avglat<< ANSI_COLOR_RESET<<endl;
	}
	else
        cout<< fixed<<std::setprecision(3)<<ANSI_COLOR_GREEN "Time: "<< curr_sec <<", Num Replicas: "<<num_workers<<", recvd results: "<<partial_rcvd <<", avg latency (usec): "<< avglat<< ANSI_COLOR_RESET<<endl;

    //for print to file
    if(percentile_lat.size()>0)
    {
        to_save_lat95=percentile_lat[indexp];
        to_save_lat99=percentile_lat[(int)(percentile_lat.size()*0.99)];
        to_save_lat_top=percentile_lat[percentile_lat.size()-1];
    }
    else{
        to_save_lat95=0;
        to_save_lat99=0;
        to_save_lat_top=0;
    }
    stats->addStat((double)(current_time_usecs()-start_usecs)/1000000.0,partial_rcvd,avglat,to_save_lat95,to_save_lat99,to_save_lat_top,std_dev_lat);

	#if defined(MONITORING)
	//send the last monitoring, just for terminating the controller
    monitoring->tag=msg::MonitoringTag::EOS_TAG;
    bsend(monitoring,cn_outqueue);

	#endif

    cout<< "Program terminated, received results: "<<rcvd_results<<endl;
    #if defined(SAVE_RESULTS)
    fclose(fresults);
    fclose(fresults_candle);
    #endif
    return stats; //return stats to main that will print to file
}

