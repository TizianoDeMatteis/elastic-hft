/***
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

	Fixed generator: this generator randomly generates bid/ask price and volume.
	Timestamp (i.e. the rate of generation) and/or the class type are generated
	using the information passed by the user
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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "../includes/random_generator.h"
#include "../includes/cycle.h"
#include "../includes/general.h"
#include "../includes/utils.h"


int num_task=1000000;
int numb_class;

static double *sums=NULL;
//These function will be used for generating a random class using some given probability distribution
inline void recomputeSum(double *probs, int numClass)  __attribute__((always_inline));
inline int generateRandomClass(double *probs, int numClass) __attribute__((always_inline));
inline int bsearch(double AR[], int N, double VAL) __attribute__((always_inline));


inline void recomputeSum(double *probs, int numClass)
{
	sums = (double *) malloc(sizeof(double) * numClass);
    bzero((void *) sums, numClass * sizeof(double));
    sums[0] = probs[0];
    for(int i = 1; i < numClass; i++) {
      sums[i] = sums[i-1] + probs[i];
      //printf("%d %1.6f\n",i,sums[i]);
    }
}
inline int bsearch(double AR[], int N, double VAL)
{
	int Mid,Lbound=0,Ubound=N-1;

	while(Lbound<=Ubound)
	{
		Mid=(Lbound+Ubound)/2;
		if(VAL>AR[Mid])
			Lbound=Mid+1;
		else if(VAL<AR[Mid])
			Ubound=Mid-1;
		else
			return Mid;
	}

	if(VAL<=AR[Lbound])
	{
		return Lbound;
	}
	else
		if(Lbound<N-1)return Lbound+1;

}
inline int generateRandomClass(double *probs, int numClass) {
  if(sums==NULL)
  { 
    recomputeSum(probs,numClass);
  }

  double random = rand()/((double)RAND_MAX + 1);

  int index = bsearch(sums,numClass,random);

  return index;
}

/*
	Reads the distribution from the file passed as argument
	File format:
		<num_class> <num_distributions>
		<time_0> <vect_0>
		...
	where times are expressed in second and represent the time from which the relative distribution (expressed with the vector
	of probability) is valid.
	The first time is assumed to be 0.

	Return the number of distribution.
	The pointer to the vector of times and the pointer to the matrix of distributions are reported in the 
	the parameters times and distributions
*/
int readDistributions(char *file, int **times, double ***distributions)
{
	FILE *fdistr=fopen(file,"r");
    srand(10);
	int nclass;
	int ndistributions;
	int i,j;
	fscanf(fdistr,"%d %d\n",&nclass,&ndistributions);
	//allocate the space
	int *t=(int *)malloc(sizeof(int)*nclass);
	double **d=(double **)malloc(sizeof(double *)*ndistributions);
	for(i=0;i<ndistributions;i++)
		d[i]=(double *)malloc(sizeof(double)*nclass);

	//start reading
	for(i=0;i<ndistributions;i++)
	{
		fscanf(fdistr, "%d",&t[i]);
		for(j=0;j<nclass;j++)
		{
			fscanf(fdistr,"%lf",&d[i][j]);
			// printf("%3.5f ",d[i][j]);
		}
		
	}
	// printf("\n");
	*times=t;
	*distributions=d;
	return ndistributions;

}

/*
	Reads additional data rate (part from the one indicated via command line) from file
	FILE format: 
	 	- first row: number of rates
	 	- for each row there is the time at which the rate become valid (in second) and the rate expressed in tuples/s
	  It is assumed that the first time is  zero (the base rate)
	*/
int readRates(char *file, int **rate_times, double **rates)
{
	FILE *frate=fopen(file,"r");
	int nrates;
	fscanf(frate,"%d\n",&nrates);
	
	int *rt=(int *)malloc(sizeof(int)*nrates);
	double *r=(double *)malloc(sizeof(double)*nrates);
	for(int i=0;i<nrates;i++)
	{
		fscanf(frate,"%d %lf\n",&rt[i],&r[i]);
		if(i==0)
			assert(rt[i]==0);
	}
	fclose(frate);
	*rate_times=rt;
	*rates=r;
	return nrates;
}

int main(int argc, char *argv[])
{
	
	cpu_set_t cpuset;
	struct timeval tmp_t;
	int ret;
	//rates from file
	int nrates=0;
	double *rates=NULL;
	int *rate_times=NULL;
	int next_rate=0;
	if(argc<7)
	{
        printf("Usage: %s hostname port number_keys distribution_file rate(msg/sec) num_elements [rate_file]\n", argv[0]);
		exit(-1);
	}
	numb_class=atoi(argv[3]);
	// double alpha=atof(argv[4]);
	double rate=atof(argv[5]);
	num_task=atoi(argv[6]);
    //get affinity
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
	//Reads the distributions from file
	int *times;
	double **distributions;
	int ndistributions=readDistributions(argv[4],&times, &distributions);

	int next_distr=1;
	int act_distr=0;
	double act_rate=rate;
	if(argc == 8)
	{
		
		nrates=readRates(argv[7],&rate_times,&rates);
		rate=rates[0];
		next_rate=1; //the next is the first read
	}


	
    sleep(1);
	int socket=connect_to(argv[1],atoi(argv[2]));
    //int one = 1;
    //setsockopt(socket, SOL_SOCKET, TCP_NODELAY, &one, sizeof(one));

	// fcntl(socket, F_SETFL, O_NONBLOCK);
	// printf("Generator with non blocking socket\n");
	// sleep(1);
	
	
	//set the same seed in order to have the same executions
	RandomGenerator generator(1);
    double waitingTime=(((double)1000000000)/(rate)); //expressed in nano secs
	
	//send the first dummy task for synchronizing the global start time...
    tuple_t t;
	t.type=-10;
    ticks no_more_init=(unsigned long long)NO_MORE_INIT;


    timestamp_t start_t=current_time_usecs();
    //start from zero
    t.timestamp=0;
    t.original_timestamp=0;
    if((ret=socket_send(socket,&t,sizeof(tuple_t)))!=sizeof(tuple_t))
	{
			fprintf(stderr,"The receiving program is a bottlenck\n");
			exit(BOTTLENECK_ERR);
	}
	
        bool bsend=true;
	int sent=0;
	long int start_slot=start_t;
    tuple_t task;
	task.original_timestamp=0;
	task.timestamp=0;
    double next_send_time_nsecs=0;          //the time (in nanosec) at which the next tuple has to be sent
    long start_time=current_time_nsecs();
	//the first task will have timestamp equal to zero
	for(int i=0;i<num_task;i++)
	{
		//periodically check if we have to change probability distribution
        if( i%((int)(act_rate/8))==0 )
		{
            timestamp_t end_t=current_time_usecs()-start_t;
            if(next_distr<ndistributions && end_t>times[next_distr]*1000000)
			{
				recomputeSum(distributions[next_distr],numb_class);
				act_distr=next_distr;
				next_distr++;

			}
			//printf("check rate: %Ld\n",i);
            if(next_rate<nrates && end_t>rate_times[next_rate]*1000000)
			{
				//change rate
                waitingTime=(long)(((double)1000000000)/(rates[next_rate]));
				act_rate=rates[next_rate];
				next_rate++;
			}
            mon_times.push_back(end_t/1000000.0);
            mon_rates.push_back(((double)sent/(end_t-start_slot))*1000);
			mon_distribution.push_back(act_distr);
			sent=0;
			start_slot=end_t;
		}

        //fill the tuple
        task.id=i;
		task.type=generateRandomClass(distributions[act_distr],numb_class);
		// printf("Invio classe: %d\n",task.type);
		task.bid_price=generator.uniform(100, 200);
		task.bid_size=generator.uniform(0, 200);
		task.ask_price=generator.uniform(100, 200);
		task.ask_size=generator.uniform(0, 200);

        volatile long end_wait=start_time+next_send_time_nsecs;
       //T volatile long end_wait=start_time+task.timestamp;
        volatile long curr_t=current_time_nsecs();
        while(curr_t<end_wait)
            curr_t=current_time_nsecs();
       //T task.timestamp=(curr_t-start_time)/1000;
        task.original_timestamp=((int)(task.timestamp/1000))*1000;
       // printf("%Ld %Ld\n",task.timestamp ,task.original_timestamp);
        //altrimenti:
        //-ri-timstampa qui, con il minimo tra next_send... e tempo attuale
        //- la attesa sopra la fai sul timestamp
        //---- in questa maniera non gestisci il collo di bottiglia però!!
        ret=socket_send(socket,&task,sizeof(tuple_t));

        if(ret!=sizeof(tuple_t))
		{
			if(ret==BOTTLENECK_ERR)
			{
                fprintf(stderr,"The receiving program is a bottleneck\n");
				exit(BOTTLENECK_ERR);
			}
		}
        next_send_time_nsecs+=waitingTime;
        task.timestamp=(long)(next_send_time_nsecs/1000.0);  //the timestamp is in usec basis
         task.original_timestamp=((int)(task.timestamp/1000))*1000;

        //T task.timestamp=task.timestamp+waitingTime; //nsecs
        if(bsend && current_time_usecs()-start_t>no_more_init)
			{
				bsend=false;
				printf("No more bsend\n");
				fcntl(socket, F_SETFL, O_NONBLOCK);
				printf("Generator with non blocking socket\n");

			}
		sent++;
	}
	task.type=-1;
    if((ret=socket_send(socket,&task,sizeof(tuple_t)))!=sizeof(tuple_t))
	{
		if(ret==BOTTLENECK_ERR)
		{
			fprintf(stderr,"The receiving program is a bottlenck at the end\n");
			exit(BOTTLENECK_ERR);
		}
	}
		gettimeofday(&tmp_t,NULL);
    timestamp_t end_t=current_time_usecs()-start_t;

    printf("Gen: Tempo passato (msec): %f, msg per second: %6.3f\n",(double)end_t,((double)num_task)/((double)end_t/1000000));

	//print to file the various monitored metrics
    char fname[]="generator.dat";

	FILE *fgenerator=fopen(fname,"w");
    fprintf(fgenerator, "TIME\tRATE(Kt\\s)\n");
	for(int i=0;i<mon_times.size();i++)
	{
        fprintf(fgenerator,"%3.3f\t%4.3f\n",mon_times[i],mon_rates[i]);
	}

	fclose(fgenerator);


	return 0;
}

