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

    Replica (aka Worker) for elastic-hft
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
#include <set>
#include "../includes/cycle.h"
#include "../includes/general.h"
#include "../includes/elastic-hft.h"
#include "../includes/messages.hpp"
#include "../includes/cbwindow.hpp"
#include "../includes/strategy_descriptor.hpp"
#include <ff/allocator.hpp>
#include <ff/buffer.hpp>


using namespace ff;
using namespace std;

void processAndSendTask(CBWindow *window,tuple_t *task,winresult_t *res_buff, int& bi,int buff_size, int worker_id,SWSR_Ptr_Buffer *outqueue,msg::WorkerMonitoring *monitoring,long int freq) __attribute__((always_inline));
/**
 * @brief standardProcessTask process the task passed, inserting into the window and triggering the computation if needed.
 * It performs also monitoring
 * @param window the window in which insert the task
 * @param task the task to insert
 * @param res_buff the result buffer, containing all the results to be sent on the collector (we use buffer just for recycle memory)
 * @param bi buffer index. It will be modified
 * @param outqueue queue toward collector
 */
inline void processAndSendTask(CBWindow *window, tuple_t *task, winresult_t *res_buff, int& bi, int buff_size, int worker_id, SWSR_Ptr_Buffer *outqueue, msg::WorkerMonitoring *monitoring, long freq)
{
    #if defined(MONITORING)
        asm volatile("":::"memory");
//        ticks start=getticks();
        long start_nsecs=current_time_nsecs();
        asm volatile("":::"memory");
    #endif
    //insert the element in window
    window->insert(*task);
    if(window->isComputable())
    {
        window->compute(res_buff[bi]);
        //set the timestamp to the timestamp of the one of the task that has triggered the computation for taking the latency at collector
        res_buff[bi].timestamp=task->timestamp;
        res_buff[bi].original_timestamp=task->original_timestamp;

        //the same for the id
        res_buff[bi].id=task->internal_id; //for the moment just for ordering
        res_buff[bi].type=task->type;
        res_buff[bi].isEOS=false;
        res_buff[bi].wid=(char)worker_id;
        if((res_buff[bi].id+1)%25!=0) //a check used while programming this stuff
        {
            cerr<<ANSI_COLOR_RED "[WORKER "<<worker_id<<"] Fatal error: computed erronoeusly on class "<<task->type <<" with int id "<<task->internal_id<< ANSI_COLOR_RESET<<endl;
            exit(-1);
        }
        //send result to the collector
        send(&res_buff[bi],outqueue);
        //advance the buffer index
        bi=(bi+1)%buff_size;
       // printf("[%d] Computato risultato per: %d\n",worker_id,task->type);

        #if defined(MONITORING)
        //take the various metrics
        double last_tcalc=(double)(current_time_nsecs()-start_nsecs)/1000.0;
        monitoring->tcalc_per_class[task->type]+=last_tcalc; //usec
        monitoring->computations_per_class[task->type]++;
        monitoring->computations++;
        //calc_times->push_back((double)(getticks()-start)/freq);
        (monitoring->calc_times)->push_back(last_tcalc);
        //if((double)(getticks()-start)/freq>monitoring->max_tcalc_per_class[task->type])
        //    monitoring->max_tcalc_per_class[task->type]=(double)(getticks()-start)/freq;
        #endif
    }
}

void * worker(void *args) {


    //Init: take data passed from main
	worker_data_t *data = (worker_data_t *) args;
	pthread_barrier_t *barrier = data->barrier;
	int id = data->workerId;
	SWSR_Ptr_Buffer *inqueue=data->inqueue;
	SWSR_Ptr_Buffer *outqueue=data->outqueue;
	int window_slide=data->window_slide;
	int window_size=data->window_size;
	long int freq=data->freq;
	int num_classes=data->num_classes;
	ticks *start_global_ticks=data->start_global_ticks;
    StrategyDescriptor *sd=data->sd;
    #if defined(USE_FFALLOC)
			ff_allocator *ffalloc=data->ffalloc;
            ffalloc->register4free(); //register this worker into the allocator
	#endif
	#if defined(MONITORING)
		SWSR_Ptr_Buffer *cn_outqueue=data->cn_outqueue;	
	#endif
    tuple_t *tmp;

    winresult_t *res_buff; //result buffer in order to reuse memory
    int buff_size;
    int bi=0;
    CBWindow *window;
    //map that contains the various association key->window
    unordered_map<int,CBWindow*> map;
    //create a buffer of results that have to be sent to the collector
    //in order to reuse memory (we can have a lot o messages) we allocate an additional number of messages
    buff_size=QUEUE_SIZE+10;
    posix_memalign((void **)&res_buff,CACHE_LINE_SIZE, buff_size*sizeof(winresult_t));
	

    //define the data structures for monitoring
    ticks start;
    volatile ticks monitoring_timer;
    volatile ticks monitoring_step=sd->control_step*1000*freq; //after how many cycles we have to monitor
    msg::WorkerMonitoring *monitoring=nullptr;
    //adaptivity data structures
    Repository *repository=data->repository;
    bool reconfiguration_phase_in=false; //indicates if the worker is currently in the reconfiguration phase (moving_in)
    bool reconfiguration_phase_out=false; //indicates if the worker is currently in the reconfiguration phase (moving_out)
    bool reconfiguration_phase=false; //indicate the whole process
    set<int> classes_moving_in; //it will contains all the id of the classes that are currently moving in
    classes_moving_in.clear();
    vector<tuple_t *> task_moving_in; //it will contain all the task belonging to moving in classes (not yet arrived to the worker) (to see why i choose vector look at the explnation below, when i use it)
    int max_enqueued=0; //for testing the state migration protocol, if needed
    #if defined(MONITORING)
        monitoring=new msg::WorkerMonitoring(num_classes);
        task_moving_in.reserve(10000);
	#endif



    if(barrier)	//it is a thread spawned at the start of the program
    {
		pthread_barrier_wait(barrier);
	}

	while(*start_global_ticks==0)
		REPEAT_25(asm volatile("PAUSE" ::: "memory");) //wait for the correct time
	

	#if defined(MONITORING)
    //define a timer for sending monitoring data
	if(barrier)
		monitoring_timer=*start_global_ticks+monitoring_step; //we skip the first step due to starting phase
	else //it a newly thread
	{
		monitoring_timer=*start_global_ticks+((getticks()-*start_global_ticks)/monitoring_step+1)*monitoring_step;
		 //printf("%d Prima del monitoring ho %Ld\n",id,monitoring_timer-getticks());
	}
	#endif


	
	/**
		Start receiving elements
	*/

	receive((void **)&tmp,inqueue);




    while(tmp->type!=-1)
    {

        //printf("Ricevuto elemento %Ld con tipo: %d\n",tmp->id,tmp->type);
        #if defined(MONITORING)
            asm volatile("":::"memory");
            start=getticks();
            monitoring->elements_per_class[tmp->type]++;
            monitoring->elements_rcvd++;
            asm volatile("":::"memory");
        #endif
        if(sd->type!=StrategyType::NONE)
        {

            //Version with adaptivity: we have to handle all the messages for reconfiguration
            if(tmp->punctuation==NO) //standard task
            {

                if(!reconfiguration_phase) //no reconf phase, just process it
                {
                    window=map[tmp->type];
                    if(window==NULL) //it's a new logical stream that has arrived
                    {
                        window=new CBWindow(window_size, window_slide);
                        map[tmp->type]=window;
                    }
                    //insert the element in window
                    processAndSendTask(window,tmp,res_buff,bi,buff_size,id,outqueue,monitoring,freq);
                    #if !defined(TASK_BUFF)
                        #if defined(USE_FFALLOC)
                            ffalloc->free(tmp);
                        #else
                        free(tmp);
                        #endif
                    #endif
                }
                else
                {

                    reconfiguration_phase_out=false; //this for sure (reconfiguration messages are sent all togheter from the emitter before new standard task)

                    //if we are in a reconfiguration phase look if the task arrived refers to a moving in class
                    if(reconfiguration_phase_in && classes_moving_in.size()>0)
                    {

                        set<int>::iterator it = classes_moving_in.begin();

                        //if needed, this is to keep track of the maximum number of enqueued tuples during a reconfiguration
//                        if(task_moving_in.size()>max_enqueued)
//                            max_enqueued=task_moving_in.size();

                        for (it = classes_moving_in.begin(); it != classes_moving_in.end(); )
                        {
                            //let's see if in the repository there is the class's window for it, in case acquire it
                            int moving_class=*it;

                           if(repository->isWindowPresent(moving_class))
                            {
                                //take it, add to the map of class's windows hold by the worker
                                //add to my map
                                //get the window from repository (it will be also removed)
                                window=repository->getAndRemoveWindow(moving_class);
                                map[moving_class]=window;
                                //erase from the set of class thare are currently ''come'' toward this worker
                                classes_moving_in.erase(it++);
                                //look in the vector of task arrived during the coming_in phase: if some of them refer to this class add to the window
                                //we don't erase them from the vector: this will be an efficient operation
                                //on the other hand, use a structure such as a list is efficient in term of insertion operations (each of them will require a memory allocation)
                                //for this reason, elements are kept in the vector and when the reconfiguration phase phinishes the whole vector is cleared
                                int ntask=0;
                                for(int i=0;i<task_moving_in.size();i++)
                                {
                                    if(task_moving_in[i]->type==moving_class)
                                    {
                                        //printf("Inserisco task con id: %Ld\n",task_moving_in[i]->internal_id);
                                        processAndSendTask(window,task_moving_in[i],res_buff,bi,buff_size,id,outqueue,monitoring,freq);
                                        ntask++;
                                    }
                                }
                            }
                            else
                                ++it;
                        }

                        if(classes_moving_in.size()==0)
                        {
                            //we finished the reconfiguration phase
                            reconfiguration_phase_in=false;
                            //free the elements in the vector (we cannot free them previously otherwise new task will take their place in vector due to the heap recyclement)
                            #if !defined(TASK_BUFF)
                            for(int i=0;i<task_moving_in.size();i++)
                            {
                                #if defined(USE_FFALLOC)
                                ffalloc->free(task_moving_in[i]);
                                #else
                                free(task_moving_in[i]);
                                 #endif
                            }
                            #endif
                            task_moving_in.clear();

                        }
                    }
                    if(reconfiguration_phase)
                    {//(necessary if we have only a reconf out)
                        if (!reconfiguration_phase_in && !reconfiguration_phase_out)
                        {
                            //FINISHED
                            reconfiguration_phase=false;
                            //signal into the repository
                            repository->setWorkerFinished(id,true);
                            asm volatile("":::"memory");
                        }
                    }



                    if(reconfiguration_phase_in && classes_moving_in.count(tmp->type)==1) //the new task refers to a still moving in class
                    {
                        //we don't see the class in the repository
                        //add the task to task_coming_in (respecting the order)
                        task_moving_in.push_back(tmp);
                    }
                    else
                    {
                        //it is a task that refer to a class currently held by the worker (or that it has just moved in)
                        window=map[tmp->type];

                        if(window==NULL) //it's a new logical stream that has arrived
                        {
                            window=new CBWindow(window_size, window_slide);
                            map[tmp->type]=window;
                        }
                        //insert the element in window
                        processAndSendTask(window,tmp,res_buff,bi,buff_size,id,outqueue,monitoring,freq);

                        #if !defined(TASK_BUFF)
                            #if defined(USE_FFALLOC)
                            ffalloc->free(tmp);
                            #else
                            free(tmp);

                            #endif
                        #endif
                     }
                }
            }
            else
            {
                if(tmp->punctuation==MOVING_OUT)
                {
                    if(!reconfiguration_phase_out)
                    {
                        reconfiguration_phase_out=true; //we are entering in the reconfiguration phase
                        reconfiguration_phase=true;
                    }
                    if(!(window=map[tmp->type]))
                    {
                        //the worker does not have this class. Probably it is not yet arrived. We will create a new window
                        //and insert it into the repository
                        window=new CBWindow(window_size, window_slide);
                        repository->setWindow(tmp->type,window);
                        map.erase(tmp->type);
                    }
                    else
                    {
                        repository->setWindow(tmp->type,map[tmp->type]);
                        //remove from the worker's map
                        map.erase(tmp->type); //NOTA: se ci fossero problemi (in teoria dovrebbe richiamare il distruttore), semplicemente mettere a NULL
                    }
                    //moving_time[tmp->type]=getticks();
                    free(tmp); //allocated by the emitter
                }
                else
                    if(tmp->punctuation==MOVING_IN)
                    {

                        if(!reconfiguration_phase_in)
                        {
                            reconfiguration_phase_in=true; //we are entering in the reconf phase for incoming classes
                            reconfiguration_phase=true;
                            //mov_time=0;
                            //moved=0;
//                            max_enqueued=0;
                        }
                        classes_moving_in.insert(tmp->type);

                        //printf("Worker %d: inserted class size%d\n",id,classes_moving_in.size());
                        free(tmp); //it was dynamically allocated by the emitter
                    }
            }
        }
        else
        {
            //no adaptivity: simply insert tasks
            window=map[tmp->type];

            if(window==NULL) //it's a new logical stream that has arrived
            {
                window=new CBWindow(window_size, window_slide);
                map[tmp->type]=window;
            }
            //insert the element in window
            processAndSendTask(window,tmp,res_buff,bi,buff_size,id,outqueue,monitoring,freq);
            #if !defined(TASK_BUFF)
                #if defined(USE_FFALLOC)
                    ffalloc->free(tmp);
                #else
                    free(tmp);
                #endif
            #endif
        }
        //if it is time, send the monitoring data
        #if defined(MONITORING)
            if(getticks()>monitoring_timer)
            {

                bsend(monitoring,cn_outqueue);
                monitoring_timer=getticks();
                //create a new monitoring message
                monitoring=new msg::WorkerMonitoring(num_classes);
                monitoring_timer+=monitoring_step; //advance monitoring timer

            }
        #endif
        //receive the next element
        receive((void **)&tmp,inqueue);
    }

    if(sd->type!=StrategyType::NONE)
    {
        //at this point no other valid tuple can arrive, we have only to wait some moving in class if any
        while(reconfiguration_phase_in && classes_moving_in.size()>0)
        {

            set<int>::iterator it = classes_moving_in.begin();

            // iterate through the set and erase
            for (it = classes_moving_in.begin(); it != classes_moving_in.end(); )
            {
                //let's see if in the repository there is the class's window for it, in case acquire it
                int moving_class=*it;
                if(repository->isWindowPresent(moving_class))
                {

                    //take it, add to the map of class's windows hold by the worker
                    window=repository->getAndRemoveWindow(moving_class);
                    map[moving_class]=window;
                    //erase from the set of class thare are currently ''come'' toward this worker
                    classes_moving_in.erase(it++);
                    for(int i=0;i<task_moving_in.size();i++)
                    {
                        if(task_moving_in[i]->type==moving_class)
                        {
                            processAndSendTask(window,task_moving_in[i],res_buff,bi,buff_size,id,outqueue,monitoring,freq);
                        }
                    }
                }
                else
                    ++it;
            }

            if(classes_moving_in.size()==0)
            {
                //we finished the reconfiguration phase
                reconfiguration_phase_in=false;
                //free the elements in the vector (we cannot free them previously otherwise new task will take their place in vector)
                #if !defined(TASK_BUFF)
                for(int i=0;i<task_moving_in.size();i++)
                {
                    #if defined(USE_FFALLOC)
                    ffalloc->free(task_moving_in[i]);
                    #else
                    free(task_moving_in[i]);
                    #endif
                }
                #endif
                task_moving_in.clear();
                //repo->reconfiguration_finished[id].store(1); we do it after
            }
        }
        //otherwise it could happen that we had to move out some classes and then an EOS arrived: also
        //in this case we have to notify that reconfiguration ends
        repository->setWorkerFinished(id,true);
    }
#if defined(MONITORING)
	//send the last monitoring, in order to avoid stall on the controller
    //indicating that we have finished
    monitoring->tag=msg::MonitoringTag::EOS_TAG;
    bsend(monitoring,cn_outqueue);
	#endif
	//send EOS to collector	
	res_buff[bi].isEOS=true;
	res_buff[bi].res_buff=res_buff;
	send(&res_buff[bi],outqueue);
    return NULL;
}
