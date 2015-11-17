/*
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


    Header containing general declaration.
	It contains also all the definitions for inlineable functions

*/

#ifndef _GENERAL
#define _GENERAL
//Include:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include "cycle.h"
#include <assert.h>
#include <ff/buffer.hpp>
#include <ff/allocator.hpp>	



/****************************************
	DEFINEs
*****************************************/

#define GENERATOR_ON_THE_SAME_MACHINE
#define CACHE_LINE_SIZE 64                  //cache block size
#define QUEUE_SIZE 10000                    //replicas' queue length

#define MAX_RHO_WORKER 1.1                  //maximum rho sustainable by a replica
#define MAX_RHO_UNBALANCE_PERCENTAGE 30     //max rho unbalance between workers (percentage)

//errors definition
#define BOTTLENECK_ERR -100
#define INTERNAL_ERR -200
#define RECONF_ERR -300
#define MAX_LAT 1000000
#define NO_MORE_INIT 6000000000             //used for max sustainable rate test. For the moment set to a very high value
//Names of output files
#define CLASS_FILE "classes"
#define LAT_FILE "latencies"
#define DEBUG(X) X;
#if defined(PRINT_CONTROL_INFO)
#define CONTROL_PRINT(X) X;                 //used to print verbose informations about reconfigurations
#else
#define CONTROL_PRINT(X)
#endif
//Monitoring definitions
#define NMONITORING 10                      //number of monitoring data structure for each entity
#define MONITORING_STEP 1000                //minimum time interval between two monitoring phases of monitoring (milliseconds)
#define QUEUE_SIZE_MON 5                    //size of queues used for sent monitoring data
#define PRINT_RATE 1000                     //defined in msec


//for colored prints


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32;1m"
#define ANSI_COLOR_RED_BOLD "\x1b[31;1m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE_REVERSE		 "\x1b[34;7m"


//Utility macros
#define FROM_TICKS_TO_USECS(ticks,cpu_freq) ((double)(ticks)/(cpu_freq))
#define ERRNULL(X) if( (X)==NULL)  return -1;
#define REPEAT_25( x ) { x x x x x x x x x x x x x x x x x x x x x x x x x }



/*************************
	Type definitions
**************************/


typedef long unsigned int timestamp_t;

//Used to specify different type of tuple
typedef enum punctation_t{
	NO=0, //in order to be the default value
	MOVING_IN=1,
    MOVING_OUT=2,
    TESTING=3
}punctation_t;




/**
 * Generic tuple definition. It is defined as structure is it contains only data (and not pointers)
 *
 * Implementation notes. It is defined as a PDO object in order to be easily transferrable through a tcp connection
 * into the same machine. It is padded at 64 byte to help cache alignment.
 *
 * It contains some additional field (such as punctuation, internal id...) that are used in the parallel implementation
 * of the operator. This has be done in order to reduce the number of messages. Moreover in this way, on the data channels (em->workers) will
 * travel always the same type of data.
 * In a real environment, we will receive from outside only the informations about the quote and they will be packed with the internal info
 */
typedef struct {
    union{
        struct
        {	//DO NOT MODIFY THIS ORDER, OTHERWISE YOU HAVE TO RECOMPUTE THE BINARY DATASET
            int64_t id; //Unique ID of the task: its meaning may depends from the particular implemenetation
            int type; //Class of the task (with -1  it representsEOS) We represent the various symbol with unique id (see the generator for the associations)

            float bid_price; //the price
            int bid_size;
            float ask_price;
            int ask_size;


            long original_timestamp; //the timestamp reported in dataset (msec basis converted in usec (used for the window computation))
            long  timestamp; //used for latency computation: its represent the sending time from the generator side (equal to original_timestamp for the real dataset)

            int64_t internal_id; //id assigned to the task while being computed in the program (e.g. incremental for each class). Not necessary for all implementations
            char worker; //tmp for debug
            punctation_t punctuation;
        };
        char padding[CACHE_LINE_SIZE];
    };
} tuple_t;


/**
 * Definition of return types and grains (i.e. internal iteration) for the functions. Since their implementation is strictly related to
the data structure used to keep the elements in window, their definition is in the various data structure header files
(such as cbwindow.hpp).
*/
typedef struct{

    //parabola parameter
    double p0_bid,p1_bid,p2_bid;
    double p0_ask,p1_ask,p2_ask;
    //candlestick parameters
    double open_bid,close_bid,high_bid,low_bid;
    double open_ask,close_ask,high_ask,low_ask;

    long long timestamp; //timestamp for monitoring purposes
    long original_timestamp; //timestamp of the triggering tuple

    int64_t id; //id of the task that has triggered the computation
    bool isEOS; //true if it will represent to the collector the end of the stream
    int type; //class
    char wid; //id of the worker that performed the computation
    ticks ts;
    void *res_buff=NULL; //this will be valid only if it is the EOS sent from a Worker to the Collector (for freeing the result_buffer)
} winresult_t;


/**************************************
	Function definitions
****************************************/

/**
	Fastflow queue operations wrappers
*/
inline int send ( void *t, ff::SWSR_Ptr_Buffer *outqueue) __attribute__((always_inline))/* __attribute__((noinline))*/; 
inline int bsend ( void *t, ff::SWSR_Ptr_Buffer *outqueue) __attribute__((always_inline))/* __attribute__((noinline))*/; 
inline void receive ( void **t, ff::SWSR_Ptr_Buffer *inqueue) __attribute__((always_inline))/* __attribute__((noinline))*/;
inline void receiveLast ( void **t, ff::SWSR_Ptr_Buffer *inqueue) __attribute__((always_inline))/* __attribute__((noinline))*/; 

//this difference (with blocking send) is only for testing purposes
inline int send ( void *t, ff::SWSR_Ptr_Buffer *outqueue)
{
	int attempt=0;
	while(!outqueue->push((void*)t))
	{
		REPEAT_25(asm volatile("PAUSE" ::: "memory");)

		#ifdef NON_BLOCKING_FF
		// if(!blocking_send)
		// {
		attempt++;
		if(attempt>MAX_SEND_RETRIES)
			return 0; //send failure
		// }
		#endif

	}
	return 1;
	
}

/**
       This version return the ticks spent in stall due to backpressure from the workers
       used for tpds strategy
*/
inline ticks ci_send ( void *t, ff::SWSR_Ptr_Buffer *outqueue)
{
       int attempts=0;
       ticks start=getticks();
       while(!outqueue->push((void*)t))
       {
               REPEAT_25(asm volatile("PAUSE" ::: "memory");)
               attempts++;
       }
       if(attempts>0)
               return getticks()-start;
       else
               return 0;
}


inline int bsend ( void *t, ff::SWSR_Ptr_Buffer *outqueue)
{
	int attempt=0;
	while(!outqueue->push((void*)t))
	{
		REPEAT_25(asm volatile("PAUSE" ::: "memory");)
	}
	return 1;
	
}


inline void receive ( void **t, ff::SWSR_Ptr_Buffer *inqueue)
{
	while (!inqueue->pop( t)) {
		// Small sleep
		REPEAT_25(asm volatile("PAUSE" ::: "memory");)
	}
}

/**
	Receive on FF queues. If there is more than one element, returns the last inserted
*/

inline void receiveLast ( void **t, ff::SWSR_Ptr_Buffer *inqueue)
{
	do
	{
		while (!inqueue->pop( t)) {
			// Small sleep
			REPEAT_25(asm volatile("PAUSE" ::: "memory");)
		}
	}while(!inqueue->empty());
}


/**
	Timing functions
*/
inline timestamp_t current_time_usecs() __attribute__((always_inline));
inline timestamp_t current_time_usecs(){
	struct timeval t;
	gettimeofday(&t, NULL);
	return (t.tv_sec)*1000000L + t.tv_usec;

}

inline long current_time_nsecs() __attribute__((always_inline));
inline long current_time_nsecs(){
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return (t.tv_sec)*1000000000L + t.tv_nsec;
}

/**
	Fitting function
*/
//Levenberg-Marquardt
//fitting will be performed using a parabola
static double parabola( double t, const double *p )
{
	return p[0] + p[1]*t + p[2]*t*t;
}

/*
	Networking functions
*/

#ifdef USE_ZMQ
void* connect_to(void *zmq_context, const char*address, int port);
void * receive_connection(void *zmq_context,int port) ;
int  socket_bsend(void* socket, void *msg, size_t len);
int socket_send(void* socket, void *msg, size_t len);
int socket_receive(void *socket, void *msg, size_t len);
int close(void * socket);
#else
//Funzione per creare una connessione TCP verso un IP address e una porta (e restituire il socket):
int connect_to(const char *ip_address, int port);

//Funzione per ricevere num_conn connessioni TCP su una porta (ritorna un array di socket):
int * receive_connection(int num_conn, int port);

//Funzione per trasmettere un messaggio su un socket (SEND):
inline size_t socket_send(int s, void *msg, ssize_t len) __attribute__((always_inline));

//Funzione per ricevere un messaggio su un socket (RECEIVE):
inline size_t socket_receive(int s, void *vtg, ssize_t len) __attribute__((always_inline));
inline size_t socket_send(int s, void *msg, ssize_t len) {
    ssize_t sentBytes = 0;
    ssize_t sent = 0;
	char *msg_char = (char *) msg;
	//Faccio le send sul socket fintanto che non ho mandato len bytes:
	while(sentBytes < len) {
		//sent = send(s, msg_char, len, (int) MSG_DONTWAIT); 
		sent = send(s, msg_char, len, 0); 
		if(sent==-1 && errno==EWOULDBLOCK)
		{
			perror("Error send(): buffer full");
			return BOTTLENECK_ERR;
		}
		if (sent == -1) { //per renderlo bloccante, si lascia questo ultimo if e non si setta il flag sul socket
			perror("Error send() call");
			return 0;
		}
		else if(sent <= len) {
			//printf("send parziale");
			//exit(-1);
			//Altrimenti si tratta di una send parziale che devo completare:
			sentBytes += sent;
			msg_char += (sent);
		}
	}
	return sentBytes;
}

//Funzione per ricevere un messaggio su un socket (RECEIVE):
inline size_t socket_receive(int s, void *vtg, size_t len) {
	char *vtg_char = (char *) vtg;
	//Effettuo la receive sul socket:
	size_t received = recv(s, vtg_char, len, MSG_WAITALL);
	if(received != len){
		perror("Errore recv() call");
		return 0;
	}
	return received;
}
static bool blocking_send=true;
inline void set_blocking(bool value)
{
	blocking_send=value;
	printf("New value of blocking send: %d\n",blocking_send);
}



//Funzione per selezionare non-deterministicamente un Socket Descriptor da cui ricevere:
int selectConnection(fd_set *fds, int *s_array, int nConnections, int lastSocket, int max);

bool set_socket_blocking_enabled(int fd, bool blocking);
#endif

//Funzione per la chiusura di un socket
int closeSocket(int socket);


#endif
