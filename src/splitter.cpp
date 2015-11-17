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

    Splitter (aka Emitter) for elastic-hft
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
#include "../includes/cycle.h"
#include "../includes/general.h"
#include "../includes/elastic-hft.h"
#include "../includes/messages.hpp"
#include "../includes/repository.hpp"
#include "../includes/strategy_descriptor.hpp"

#include <sys/ioctl.h>
#include <linux/sockios.h>
#include "../includes/statistics.hpp"

using namespace ff;
using namespace std;


/**
	Determines the id of the Worker to which send the task
	according to a round robin allocation strategy of the logical streams
	to the Workers
*/
char next_schedulingRR=0;
char * scheduling_table;
inline char schedulingRR (tuple_t *t, int num_workers)
{
	//The scheduling table is a simple array with numb_classes positions
	//if the i-th element is zero then for that logical stream we don't have 
	//an assigned Worker. Otherwise it contains the ID_Worker+1 to which send
	//the tasks of that class
	if(scheduling_table[t->type]==0)
	{
		//DEBUG(printf("***New logical stream, assigned to Worker: %d\n",next_schedulingRR));
		scheduling_table[t->type]=next_schedulingRR	+1;
		next_schedulingRR=(next_schedulingRR+1)%num_workers;
	}
	return scheduling_table[t->type]-1;

}


/**
    Main emitter functionality
  */
void *emitter(void *args)
{

    tuple_t tmp;                            //received tuple
    struct timeval tmp_t;                   //timing
	long start_t=0, end_t=0;
	

	char to_send_to;
    int ret;
    //Init: take data passed from main
	emitter_data_t *data = (emitter_data_t *) args;
	pthread_barrier_t *barrier = data->barrier;
	int num_workers=data->num_workers;
	int port=data->port;
	int64_t msg=0;
	SWSR_Ptr_Buffer **outqueue=data->outqueue;
	ticks *start_global_ticks=data->start_global_ticks;
    long *first_tuple_timestamp=data->first_tuple_timestamp;
    long *start_global_usecs=data->start_global_usecs;
	int num_classes=data->num_classes;
    long int freq=data->freq;
    int window_slide=data->window_slide;
    tuple_t eos_t; //tuple for terminating workers
    StrategyDescriptor *sd=data->sd;
	eos_t.type=-1;
    scheduling_table=new char[num_classes](); //the mapping function class_id(aka key)->worker
    //by default use a round robin mapping
    char w=0;
    for(int i=0;i<num_classes;i++)
    {
        scheduling_table[i]=w+1;
        w++;
        w%=num_workers;
    }

    //Statistics computed on the fly  considering the timestamps of the tuples
    stats::RunningStat stat_timestamp;

	//frequency counter for the various classes
    int64_t *classes_freq=new int64_t[num_classes]();

	#if defined(TASK_BUFF)
		printf(ANSI_COLOR_RED "Pay attention to this version with task_buff!\n" ANSI_COLOR_RESET);
		//create the buffer for tasks
        tuple_t * task_buff;
		//We do not know how large should be this buffer in order to guarantee correctness
		int buff_size=num_workers*(QUEUE_SIZE+2)*num_workers;
        posix_memalign((void **)&task_buff,CACHE_LINE_SIZE, buff_size*sizeof(Tuple));
		int bi=0; //index pointer for the task buffer
	#else
        #if defined(USE_FFALLOC)

            ff_allocator *ffalloc=data->ffalloc;
		#endif
	#endif
    tuple_t *tb;

	#if defined(MONITORING)
		//define the data structures for monitoring
		//queue towards the controller
		SWSR_Ptr_Buffer *cn_outqueue=data->cn_outqueue;		
        long monitoring_timer;
        long monitoring_step_usecs=sd->control_step*1000; //after how many usecs we have to send monitoring data
        ticks monitoring_step_ticks=sd->control_step*1000*freq;
        //reconfiguration messages
        //reconf_data_em_t *reconf_data;
        msg::ReconfEmitter *reconf_data;

        msg::EmitterMonitoring* monitoring=new msg::EmitterMonitoring(num_classes);
        Repository *repository=data->repository;
        SWSR_Ptr_Buffer *cn_inqueue = nullptr;
        if(sd->type!=StrategyType::NONE)//there is an adaptation strategy
        {
			//queue from the controller for incoming reconfiguration commands
            cn_inqueue =data->cn_inqueue;
			//this particular monitoring data is used to notify that the reconfiguration is finished
            //it does not need to be deleted by the controller
        }
	#endif
	

	DEBUG(printf("Emitter ready, running on: %d\n",sched_getcpu()))
	//synchronization barrier

    pthread_barrier_wait(barrier);

    //accept connection from generator
    #if defined (USE_ZMQ)
        void *context = zmq_ctx_new ();
        void *socket=receive_connection(context,port);
    #else
        int socket;
        socket=*(int *)receive_connection(1,port);
    #endif

	/**
		The first receives are for tacking global start time used for
        computing the latency. It reports the original_timestamp of the first tuple
	*/

    if((ret=socket_receive(socket, &tmp, sizeof(tuple_t)))!=sizeof(tuple_t))
	{
		fprintf(stderr,"The program is a bottleneck\n");
		exit(BOTTLENECK_ERR);
	}
	asm volatile ("" ::: "memory");
    *first_tuple_timestamp=tmp.timestamp;
    *start_global_usecs=current_time_usecs();
	// *start_global_ticks=tmp.timestamp; //ticks at generator side; this will be a sort of synchronized wall clock time, assuming that the tasks are timestamped starting from zero
    *start_global_ticks=getticks(); //just take the time, it will be used for computing the latency (pay attention: this could lead to a slightly approximated result)

	#if defined(MONITORING)
		//start monitoring
        monitoring_timer=*start_global_usecs+monitoring_step_usecs; //start_global_ticks act as a shared wall clock time, we need this concept for synchronizing monitoring

    #endif


	/**
		Start receiving real elements
	*/

	#if defined(TASK_BUFF)
		tb=&task_buff[bi];
	#else
        #if defined(USE_FFALLOC)
            tb=(tuple_t *)ffalloc->malloc(sizeof(tuple_t)); //posix memalign does not work for ff allocator (bugged)
		#else
            posix_memalign((void **)&tb,CACHE_LINE_SIZE,sizeof(tuple_t));
		#endif
	#endif
    if((ret=socket_receive(socket, tb, sizeof(tuple_t)))!=sizeof(tuple_t))
	{
        std::cerr<<"The program is a bottleneck\n"<<endl;
		exit(BOTTLENECK_ERR);
	}

    //take the initial time
    gettimeofday(&tmp_t,NULL);
    start_t=tmp_t.tv_sec*1000+(tmp_t.tv_usec)/1000;


    //for computing variance on the fly (all referring to ticks)

    long curr_usecs;
    double last_recv_timestamp=tb->timestamp;
    //for TPDS strategy
    ticks congestion_index=0;
    //start receiving and distribute task
    while(tb->type!=-1)
	{
		msg++;
		#if defined (MONITORING)
            monitoring->elements++;
            monitoring->elements_per_class[tb->type]++;
			//on the fly variance, but we have to consider only elements that trigger a computation
			//(the interarrival time refers to interarrival of tuples that trigger a slide)
            if(monitoring->elements%window_slide==0) //we take it every window_slide
			{

				//stat computed by using the timestamp into the tuples
                stat_timestamp.Push(tb->original_timestamp-last_recv_timestamp);
                last_recv_timestamp=tb->original_timestamp;

			}
		#endif

		tb->internal_id=classes_freq[tb->type]++;
		tb->punctuation=NO;
		to_send_to=schedulingRR(tb,num_workers); //e qui

        if(sd->type!=StrategyType::TPDS)
        {
            if(!send(tb,outqueue[to_send_to]))
            {
                cerr<<ANSI_COLOR_RED<< "Replica "<<to_send_to<<" is a bottleneck"<<ANSI_COLOR_RESET<<endl;
                exit(BOTTLENECK_ERR);
            }
        }
        else
        {
            //we have to count the congestion
            congestion_index+=ci_send(tb,outqueue[to_send_to]);
        }


        //take the pointer to the next task that will be received
		#if defined(TASK_BUFF)
            //advance buffer index
			bi=(bi+1)%(buff_size);
			tb=&task_buff[bi];
		#else
            #if defined(USE_FFALLOC)
                tb=(tuple_t *)ffalloc->malloc(sizeof(tuple_t));
			#else
                posix_memalign((void **)&tb,CACHE_LINE_SIZE,sizeof(tuple_t));
			#endif
		#endif

		#if defined(MONITORING)

            if(sd->type!=StrategyType::NONE)
            {
                //check if there are reconfiguration messages from the controller
                if(cn_inqueue->pop((void **)(&reconf_data)))
                {

                    //check if there are newly spawned threads. In this case we have to bind the queues of newly workers
                    // in order to perform state migration
                    if(reconf_data->tag==msg::ReconfTag::INCREASE_PAR_DEGREE)
                    {
                        CONTROL_PRINT(cout << ANSI_COLOR_YELLOW "[EMITTER] reconfiguration message: add " << reconf_data->par_degree_changes <<" workers " ANSI_COLOR_RESET<<endl;)
                        //take the new queues
                        for(int i=0;i<reconf_data->par_degree_changes;i++)
                            outqueue[num_workers+i]=reconf_data->wqueues[i];
                        //increments the par degree
                        num_workers+=reconf_data->par_degree_changes;
                    }
                    else
                        if(reconf_data->tag==msg::ReconfTag::DECREASE_PAR_DEGREE)//par degree decrease (we will handle them after that their state has been moved out)
                        {
                            CONTROL_PRINT(cout << ANSI_COLOR_YELLOW "[EMITTER] reconfiguration message: remove " << abs(reconf_data->par_degree_changes)<< " workers" ANSI_COLOR_RESET<<endl;)
                        }
                        else
                        {
                            CONTROL_PRINT(cout << ANSI_COLOR_YELLOW "[EMITTER] reconfiguration message: rebalance scheduling table!" << endl;)
                        }


                    //Find and handle differences between current and previous scheduling table. Start proper state migrations.
                    int differences=0;


                    for(int i=0;i<num_classes;i++)
                    {

                        if(scheduling_table[i]!=reconf_data->scheduling_table[i])
                        {
                            differences++;
                            //send the proper signal to the worker that until now has mantained the class
                            //it is sent as a special tuple
                            tuple_t *signalt=new tuple_t;
                            signalt->type=i; //signal the class that has to be moved
                            signalt->punctuation=MOVING_OUT;
                            //<R4-R2>: This is used for testing proprerty R4 and R2 of the state migration
                            //protocol (involved workers are blocked during reconfiguration)
                            //repository->setHasToMoveOut(scheduling_table[i]-1,true);//</R4>
                            if(!send(signalt,outqueue[scheduling_table[i]-1]))
                            {
                                cerr << ANSI_COLOR_RED "[EMITTER] Worker "<<scheduling_table[i]-1<<" is a bottleneck" ANSI_COLOR_RESET<<endl;
                                exit(BOTTLENECK_ERR);
                            }
                            //set the atomic value to zero (it will be used to understand when the reconfiguration has finished)
                            repository->setWorkerFinished(scheduling_table[i]-1,false);

                        }
                    }
                    //now send move_in
                    for(int i=0;i<num_classes;i++)
                    {

                        if(scheduling_table[i]!=reconf_data->scheduling_table[i])
                        {
                            tuple_t *signalt=new tuple_t;
                            signalt->type=i; //signal task
                            signalt->punctuation=MOVING_IN;
                            if(!send(signalt,outqueue[reconf_data->scheduling_table[i]-1]))
                            {
                                cerr << ANSI_COLOR_RED "[EMITTER] Worker "<<scheduling_table[i]-1<<" is a bottleneck" ANSI_COLOR_RESET<<endl;
                                exit(BOTTLENECK_ERR);
                            }
                            repository->setWorkerFinished(scheduling_table[i]-1,false);
                        }
                        //copy the entry in the scheduling table
                        scheduling_table[i]=reconf_data->scheduling_table[i];
                    }

                    //if this is a reconfiguration that requires the termination of some worker,
                    //send the EOS to them (it is assumed that scheduling table was correctly derived from the strategy)
                    if(reconf_data->tag==msg::ReconfTag::DECREASE_PAR_DEGREE)
                    {
                        //we will terminate the workers with highest id
                        for(int i=0;i>reconf_data->par_degree_changes;i--)
                        {
                            bsend(&eos_t,outqueue[num_workers+i-1]);
                            //set to NULL outqueues (they will destructed by workers)
                            outqueue[num_workers+i-1]=NULL;
                        }
                        //set the new num_workers
                        num_workers+=reconf_data->par_degree_changes;

                    }

                    CONTROL_PRINT(cout << ANSI_COLOR_YELLOW "[EMITTER] "<<differences<<" associations have changed " ANSI_COLOR_RESET<<endl;)
                    //ok, notify to the controller that the reconfiguration has finished
                    msg::EmitterMonitoring* reconf_finished=new msg::EmitterMonitoring(0);
                    reconf_finished->tag=msg::MonitoringTag::RECONF_FINISHED_TAG;
                    bsend((void*)reconf_finished,cn_outqueue);


                }
            }

			//check if we can send monitoring data to controller

            curr_usecs=current_time_usecs();
            if(curr_usecs>monitoring_timer)
			{ 	
                //copy the pointer to the current scheduling table
                monitoring->scheduling_table=scheduling_table;
                monitoring->ta_timestamp=stat_timestamp.Mean()/1000.0;

                //Scaleup: since we are monitoring through the tuples timestamp, we could not capture the real
                //current  interarrival time. Therefore if there a certain (large) number of tuples
                //in the incoming socket we will lower the monittored interarrival time in order force scaleup
                int value;
                ioctl(socket, SIOCINQ, &value);
                int nenq=value/sizeof(tuple_t);

                if(nenq>10000)//force scaleup by saying that ta is smaller than the currently monitored
                {
                    //std::cout<<"---------Too much enqueued"<<std::endl;
                    monitoring->ta_timestamp*=0.9;
                }

                //we have to convert it into msec
                monitoring->std_dev_timestamp=stat_timestamp.StandardDeviation()/1000.0;
				 
				//compute congestion index (approximated, considering the monitoring step)

                if(sd->type==StrategyType::TPDS)
                {
                    double cong_value=((double)congestion_index/monitoring_step_ticks);
                    monitoring->congestion=cong_value>sd->congestion_threshold;
                    congestion_index=0;
                }

                bsend(monitoring,cn_outqueue); //pay attention, if queue is full it is blocked
                monitoring_timer=current_time_usecs();
                //create a new message
                monitoring=new msg::EmitterMonitoring(num_classes);
                monitoring_timer+=monitoring_step_usecs;

				//reset variables for variance calculation
				stat_timestamp.Clear();

            }
			
		#endif
        if((ret=socket_receive(socket, tb, sizeof(tuple_t)))!=sizeof(tuple_t))
		{
            cerr << ANSI_COLOR_RED "[EMITTER] Error in receiving from the  socket. The operator is a bottleneck?"<<endl;
			exit(BOTTLENECK_ERR);
		}
		
	}

    #if defined(MONITORING)
    //end of stream: check if there are communication by the controller
    if(sd->type!=StrategyType::NONE)
    {

        //Since we are exiting it is useless to move classes, just send EOS also newly created workers
        if(cn_inqueue->pop((void **)(&reconf_data)))
        {

            //check if there are newly spawned threads
            if(reconf_data->par_degree_changes>0)
            {
                //increments the par degree
                num_workers+=reconf_data->par_degree_changes;
            }

            msg::EmitterMonitoring* reconf_finished=new msg::EmitterMonitoring(0);
            reconf_finished->tag=msg::MonitoringTag::RECONF_FINISHED_TAG;
            bsend((void*)reconf_finished,cn_outqueue);

        }
    }
    #endif
	
	//EOS: send terminating task to all the workers
	for(int i=0;i<num_workers;i++)
	{
		//printf("Emitter, send EOS to:%d\n",i);
		send(tb,outqueue[i]);
	}
	
	#if defined(MONITORING)
		//send termination to controller
        monitoring->stop=true;
        monitoring->tag=msg::MonitoringTag::EOS_TAG;
        bsend(monitoring,cn_outqueue);
		
	#endif
	//compute statistics
	gettimeofday(&tmp_t,NULL);
	end_t=tmp_t.tv_sec*1000+(tmp_t.tv_usec)/1000-start_t;
	//pthread_barrier_wait(barrier);
    CONTROL_PRINT(cout << ANSI_COLOR_GREEN "[EMITTER] Elapsed time (msec): "<<end_t <<" msg per second: "<<((double)msg)/((double)end_t/1000) <<ANSI_COLOR_RESET<<endl;)

	closeSocket(socket);


	return NULL;
}
