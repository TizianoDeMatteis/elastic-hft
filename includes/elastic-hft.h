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
	Headers containing the various definitions for the functional partitioning
	version with count based windows
*/
#ifndef _FUNC_PART_H
#define _FUNC_PART_H
#include <pthread.h>
#include "general.h"
#include "repository.hpp"
#include "strategy_descriptor.hpp"
#include <ff/buffer.hpp>
#include <ff/allocator.hpp>
#include <vector>
#include <assert.h>


/**
	Data structure passed to the various entities
*/
//Struttura dati dello stato dell'emettitore:
typedef struct emitter_data {
	int port; //port from which receive the connection from the generator
	int num_workers; //number of workers
	int num_classes; //number of classes

	ff::SWSR_Ptr_Buffer **outqueue; //queues towards workers
	pthread_barrier_t *barrier; //initial synchronization barrier
	//class_descr_t *schedulingTable; //Tabella delle informazioni per la schedulazione "smart".
	ticks monitoring_step; //time interval between two monitoring phases
	//char *scheduling_table; //scheduling table
	ff::ff_allocator *ffalloc; //fastflow memory allocator
	ticks *start_global_ticks; //start time on module side expressed in clock cycles
    long *first_tuple_timestamp; //the usec timestamp of the first arrived tuple
    long *start_global_usecs;
	long int freq;
	char *suffix;

	//output queue towards the controller
	ff::SWSR_Ptr_Buffer *cn_outqueue;
	//input queue from the controller
	ff::SWSR_Ptr_Buffer *cn_inqueue;

	//repository for configuration
	void *repo;
    Repository *repository;
	int window_slide;

    //Strategy descriptor
    StrategyDescriptor* sd;
} emitter_data_t;

//Struttura dati dello stato del collettore:
typedef struct collector_data {
	int num_workers; //number of workers
	int num_classes; //number of classes
	ff::SWSR_Ptr_Buffer **inqueue; //input queues from workers
	ff::SWSR_Ptr_Buffer **wqueue; //queues emitter-> workers (only for monitoring purposes, read only)
	pthread_barrier_t *barrier; //initial synchronization barrier
	ticks *start_global_ticks; //start time, derived from the first tuple
    long *first_tuple_timestamp; //the usec timestamp of the first arrived tuple
    long *start_global_usecs;
	//for testing only
	long int freq; //processor frequency

	char *suffix;
	int window_slide;
	//output queue towards the controller
	ff::SWSR_Ptr_Buffer *cn_outqueue;
	ff::SWSR_Ptr_Buffer *cn_inqueue;
	int max_workers;
    //Strategy descriptor
    StrategyDescriptor* sd;
} collector_data_t;

//Struttura dati dello stato del generico worker:
typedef struct worker_data {
	ff::SWSR_Ptr_Buffer *inqueue; //queue from Emitter
	ff::SWSR_Ptr_Buffer *outqueue; //queue toward Collector
	int workerId; //Id
	pthread_barrier_t *barrier; ////initial synchronization barrier
	int window_size;
	int window_slide;
	ticks *start_global_ticks; //start time, derived from the first tuple
	
	//double *comp_time; //computation times for the various classes
	ff::ff_allocator *ffalloc; //fastflow memory allocator
	int num_classes;
	//for testing only
	long int freq; //processor frequency

	//output queue towards the controller
	ff::SWSR_Ptr_Buffer *cn_outqueue;
	//repository for configuration
	void *repo;
    Repository *repository;
    //Strategy descriptor
    StrategyDescriptor* sd;

	

} worker_data_t;

typedef struct controller_data {
	int num_workers; //number of workers
	int num_classes; //number of classes
	long int freq;
	char *suffix;
	//input queues
	ff::SWSR_Ptr_Buffer *e_inqueue;
	ff::SWSR_Ptr_Buffer **w_inqueue;
	ff::SWSR_Ptr_Buffer *c_inqueue;
	
	//output queues
	ff::SWSR_Ptr_Buffer *e_outqueue;
	ff::SWSR_Ptr_Buffer *c_outqueue;
	ticks *start_global_ticks; //start time, derived from the first tuple
    long *start_global_usecs;
	//repository for configuration
	void *repo;
    Repository *repository;

	int *affinities;
	int max_workers;
	//these will be used for thread spawning
	int window_size;
	int window_slide;
	//double *comp_time; //computation times for the various classes
	ff::ff_allocator *ffalloc; //fastflow memory allocator
    //Strategy descriptor
    StrategyDescriptor* sd;
	
}controller_data_t;


/**

/**
	Function Declarations
*/
void *emitter(void *args);
void * worker(void *args);
void * collector(void *args);
void *controller (void *args);

#endif
