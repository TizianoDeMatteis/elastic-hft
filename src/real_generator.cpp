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



	This generator is specifically designed to ingest trading dataset
	downloadable at nysedata
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <vector>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>

#include <iostream>
#include <fstream>

#include <unordered_map>
#include "../includes/cycle.h"
#include "../includes/general.h"
#include "../includes/utils.h"

using namespace std;
int numb_class;
float FREQ;
static int cmp(const void *p1, const void *p2)
{
   
   return *(int *)p1-*(int*)p2;
}

/**
	Function for reading a Dataset formatted according to the 
	Daily TAQ client specification 2.0 (http://www.nyxdata.com/data-products/daily-taq)

	Param
	- path of the dataset
    - number of tuple to be read
	- scaling factor for the time

	NOTE:
	- alphanumerical stock symbol are transleted into unique id
	- it is assumed that the row are ordered according to the trade time
*/
tuple_t* readDailyQuote( char * path, int num_row, int time_scaling)
{
	FILE *fin=fopen(path,"rb");
	if(!fin)
		exit(-1);
    tuple_t *tasks;	posix_memalign((void **)&tasks,CACHE_LINE_SIZE,num_row*sizeof(tuple_t));
    fread(tasks,sizeof(tuple_t),num_row,fin);
	
    double first_tuple_time;
	vector<int> msg_per_second;
	std::unordered_map<int, int> freq_per_symb;
	int msg_last_second=0;

    first_tuple_time=((double)tasks[0].original_timestamp)/((double)time_scaling);
    double start_second_time=first_tuple_time;
	for(int i=0;i<num_row;i++)
	{
        //pay attention: in the newer version we have that original_timestamp is reported in usecs, while here is reported in msec
       // tasks[i].timestamp=(ticks)((((double)tasks[i].original_timestamp)/time_scaling-first_tuple_time)*FREQ);

		//printf("task i timestamp: %Ld\n",tasks[i].timestamp);
         tasks[i].original_timestamp=(((double)tasks[i].original_timestamp)/time_scaling);
         tasks[i].timestamp=tasks[i].original_timestamp;
		//msg per second (considering also the time scaling)
		if(((double)tasks[i].original_timestamp)/time_scaling-start_second_time>1000)
		{
			start_second_time=((double)tasks[i].original_timestamp)/time_scaling;
			msg_per_second.push_back(msg_last_second);
			msg_last_second=0;
		}
		msg_last_second++;

		//per symbol
		freq_per_symb[tasks[i].type]++;
	
	}

    //stats...
    if(msg_last_second>0)
		msg_per_second.push_back(msg_last_second);


	int max=0;
	int *frequencies=(int *)calloc(freq_per_symb.size(),sizeof(int));
	int indx=0;
	for ( auto it = freq_per_symb.begin(); it != freq_per_symb.end(); it++ )
	{
		if(it->second>max)
		{
			max=it->second;
			
		}
			frequencies[indx++]=it->second;
	}
	qsort(frequencies,freq_per_symb.size(),sizeof(int),cmp);

	return tasks;
		
}

int main(int argc, char *argv[])
{
    cpu_set_t cpuset;
	struct timeval tmp_t;
	int ret;
	
	if(argc<5)
	{
        printf("Usage: %s hostname port dataset num_task [-s time scale]\n", argv[0]);
		exit(-1);
	}

	int num_task=atoi(argv[4]);
	char *tuple_dataset=argv[3];
	char *host=argv[1];
	int port=atoi(argv[2]);
    //get max frequncy and affinity
    FREQ=getMaximumFrequency();
    vector<int>* core_ids=getCoreIDs();
    int generator_affinity=core_ids->at(0);
	//set affinity
	CPU_ZERO(&cpuset);
	CPU_SET(generator_affinity, &cpuset);
	pthread_t tid = pthread_self();

	if (pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset)) {
		fprintf(stderr,"Cannot set thread to CPU %d\n", generator_affinity);
	}
		
	//for monitoring during testing phase
	vector<double> mon_times; //times at which we take the various metric
	vector<double> mon_rates;
	vector<int> mon_distribution; //probability distribution used
	
	//READ OTHER OPTIONS
	int c;
	int time_scale=1;

  	opterr = 0;

    while ((c = getopt (argc, argv, "s:")) != -1)
    	switch (c)
      	{
      		case 's': //scale factor
      			time_scale=atoi(optarg);
      			break;
        }
    tuple_t* tasks=readDailyQuote(tuple_dataset,num_task,time_scale); //da sistemare time scaling


    cout<<"Expected data generation time (sec): "<<(tasks[num_task-1].original_timestamp-tasks[0].original_timestamp)/1000000.0<<endl;
	
	int socket=connect_to(host,port);

	// fcntl(socket, F_SETFL, O_NONBLOCK);

	//send the first dummy task for synchronizing the global start time...
	volatile ticks start_ticks=getticks();
    long  start_t=current_time_usecs();
    tuple_t t;
	t.type=-10;
    ticks no_more_init=(unsigned long long)FREQ*NO_MORE_INIT; //for the moment is set to an high value in order to not be used
    t.timestamp=tasks[0].original_timestamp;
    t.original_timestamp=tasks[0].original_timestamp;

    if((ret=socket_send(socket,&t,sizeof(tuple_t)))!=sizeof(tuple_t))
	{
		
			fprintf(stderr,"The receiving program is a bottlenck\n");
			exit(BOTTLENECK_ERR);
		
	}
	
	bool bsend=true;
	int sent=0;

    gettimeofday(&tmp_t,NULL);
    start_ticks=getticks();
    long start_time=current_time_nsecs();
    long first_tuple_timestamp=tasks[0].timestamp;
    long last_stat=current_time_usecs();
    for(int i=0;i<num_task;i++)
    {


        long end_wait=start_time+(tasks[i].timestamp-first_tuple_timestamp)*1000;
        long curr_t=current_time_nsecs();
        while(curr_t<end_wait)
            curr_t=current_time_nsecs();
        //altrimenti anche qui, partendo dal primo timestamp, ci aggiungi quello che serve
        //tasks[i].timestamp=tasks[i].original_timestamp;
        ret=socket_send(socket,&tasks[i],sizeof(tuple_t));

        if(ret!=sizeof(tuple_t))
        {
            if(ret==BOTTLENECK_ERR)
            {
                exit(BOTTLENECK_ERR);
            }
        }
        if(bsend && getticks()-start_ticks>no_more_init)
            {
                bsend=false;
                fcntl(socket, F_SETFL, O_NONBLOCK);
                printf("Generator with non blocking socket\n");

            }
        sent++;
        //save some data every second
        if(i%10000==0 )
        {
            long  curr_t=current_time_usecs();
            if((curr_t-last_stat)>1000000)
            {
                mon_times.push_back((curr_t-start_t)/1000000.0);
                mon_rates.push_back(sent);
                sent=0;
                last_stat=curr_t;
            }
        }


    }

	t.type=-1;
    if((ret=socket_send(socket,&t,sizeof(tuple_t)))!=sizeof(tuple_t))
	{
		if(ret==BOTTLENECK_ERR)
		{
			fprintf(stderr,"The receiving program is a bottlenck at the end\n");
			exit(BOTTLENECK_ERR);
		}
	}

    long int end_t=current_time_usecs();
    cout << "Elapsed time (msec): "<< (end_t-start_t)/1000000.0<< " Average number of messages per seconds: "<< ((double)num_task)/((double)(end_t-start_t)/1000000.0)<<endl;
	
	//print to file the various monitored metrics

	//sprintf(fname,"generator_%s.dat",tuple_dataset);
    FILE *fgenerator=fopen("generator.dat","w");
	fprintf(fgenerator, "TIME\tRATE(Kt\\s)\n");
	//group per second
	int last_second=mon_times[0];
	double rate_last_second=mon_rates[0];
	for(int i=1;i<mon_times.size();i++)
	{
		if(mon_times[i]-last_second>1)
		{
			fprintf(fgenerator,"%3.3f\t\t%4.3f\n",mon_times[i],rate_last_second);
			rate_last_second=mon_rates[i];
			last_second=mon_times[i];
		}
		else
			rate_last_second+=mon_rates[i];
	}

    fclose(fgenerator);
	

	return 0;
}



