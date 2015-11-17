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
    */
#ifndef MESSAGES_HPP
#define MESSAGES_HPP
namespace msg{
enum class MonitoringTag{
    MONITORING_TAG,             //used to indicate that the message is used for monitored data
    RECONF_FINISHED_TAG,        //used to indicate that the message is used to indicate that a reconfiguration has finished
    EOS_TAG                     //used to indicate the termination of an entity to the controller
};

enum class ReconfTag{
    INCREASE_PAR_DEGREE,        //used to indicate that the message is used for increasing the parallelism degree
    DECREASE_PAR_DEGREE,        //used to indicate that the message is used for decreasing the parallelism degree
    REBALANCE                   //used to indicate that the message is used to rebalance load through a new sched table
};

/**
 * @brief The EmitterMonitoring class for monitored data sent from the Emitter towards the Controller
 */
class EmitterMonitoring{

public:
    MonitoringTag tag;          //type of message
    int64_t elements;           //number of elements received in the last monitoring step
    int buffer_elements;        //number of elements that are in the tcp buffer while sending monitoring data
    int* elements_per_class;    //numb of elements per class
    bool stop;                  //used for stopping monitoring
    char *scheduling_table;     //actual scheduling table (pointer)
    double ta_timestamp;        //interarrival time in msec for the last monitoring step
    double std_dev_timestamp;   //standard deviation of interarrival time in msec for the last monitoring step
    bool congestion;            //used by TPDS strategy for signaling a congestion

    /**
     * @brief Default constructor
     * @param num_classes number of currently managed classes
     */
    EmitterMonitoring(int num_classes)
    {
        tag=MonitoringTag::MONITORING_TAG;
        //tag=MON_TAG;
        elements=0;
        stop=false;
        scheduling_table=NULL;
        elements_per_class=new int[num_classes](); //initialized to zero
        _num_classes=num_classes;
        congestion=false;
    }


    ~EmitterMonitoring()
    {
        delete[] elements_per_class;

    }

    /**
     * @brief EmitterMonitoring copy constructor not defined
     */
    EmitterMonitoring ( const EmitterMonitoring & ){
        throw std::logic_error("Copy Constructor for EmitterMonitoring not defined!\n");
    }

    /**
     * @brief reset the message
     */
    void reset()
    {
        elements=0;
        buffer_elements=0;
        memset(elements_per_class,0,_num_classes*sizeof(int));
    }



private:
    int _num_classes;

}__attribute__((__aligned__(64)));



/**
 * @brief The WorkerMonitoring class for monitored data sent from Worker to Controller
 */
class WorkerMonitoring{

public:
    MonitoringTag tag;                 //the type of data
    int *elements_per_class;            //numb of elements per class (it is redundant but we have to keep it for the worker)
    int *computations_per_class;        //numb of computation per class (it is redundant but we have to keep it for the worker and will be helpful in computing the right metrics when the module is bottleneck)
    double *tcalc_per_class;            //tcalc expressed in usec (total tcalc per class, not on average)
    double *max_tcalc_per_class;        //TMP per vedere come va NC (usecs)
    std::vector<double> * calc_times;   //reports all the computation times (for every class assigned to the worker)
    int elements_rcvd;                  //total number of element received
    int computations;                   //number of computation performed

    /**
     * @brief WorkerMonitoring default constructor
     * @param num_classes
     */
    WorkerMonitoring(int num_classes)
    {
        tag=MonitoringTag::MONITORING_TAG;
        elements_rcvd=0;
        computations=0;
        elements_per_class=new int[num_classes]();
        computations_per_class=new int[num_classes]();
        tcalc_per_class=new double[num_classes]();
        max_tcalc_per_class=new double[num_classes]();
        calc_times=new std::vector<double>;
        calc_times->reserve(500);
        _num_classes=num_classes;

    }

    /**
     * @brief WorkerMonitoring copy constructor not defined
     */
    WorkerMonitoring ( const WorkerMonitoring & ){
        throw std::logic_error("Copy Constructor for WorkerMonitoring not defined!\n");
    }

    ~WorkerMonitoring()
    {
        delete[] elements_per_class;
        delete[] computations_per_class;
        delete[] tcalc_per_class;
        delete[] max_tcalc_per_class;
        delete(calc_times);
    }

    /**
     * @brief reset the message's fields
     */
    void reset()
    {
        elements_rcvd=0;
        computations=0;
        memset(elements_per_class,0,_num_classes*sizeof(int));
        memset(computations_per_class,0,_num_classes*sizeof(int));
        memset(tcalc_per_class,0,_num_classes*sizeof(double));
        memset(max_tcalc_per_class,0,_num_classes*sizeof(double));
        calc_times->clear();


    }


private:
    int _num_classes;
}__attribute__((__aligned__(64)));


/**
 * @brief The CollectorMonitoring class for send Collector monitoring data to Controller
 */
class CollectorMonitoring{

public:

    MonitoringTag tag;                 //the type of data
    int results;                        //total received results
    int *results_per_class;             //results received for each class
    double *latency_per_class;          //expressed in usec (averaged for each class)        IS THIS REALLY USED?
    long monitoring_time;
    double avg_lat;                     //overall latency expressed in usec
    double lat_95;                      //95-percentile expressed in usecs
    double c_serv;                      //coefficient of variation of services, monitored by the collector
    /**
     * @brief CollectorMonitoring default constructor
     * @param num_classes
     */
    CollectorMonitoring(int num_classes)
    {
        tag=MonitoringTag::MONITORING_TAG;
        results=0;
        avg_lat=0;
        lat_95=0;
        results_per_class=new int[num_classes]();
        latency_per_class=new double[num_classes]();
        _num_classes=num_classes;

    }

    /**
     * @brief WorkerMonitoring copy constructor not defined
     */
    CollectorMonitoring ( const CollectorMonitoring & ){
        throw std::logic_error("Copy Constructor for CollectorMonitoring not defined!\n");
    }

    ~CollectorMonitoring()
    {
        delete[] results_per_class;
        delete[] latency_per_class;
    }

    /**
     * @brief reset the message's fields
     */
    void reset()
    {
        results=0;
        avg_lat=0;
        lat_95=0;
        memset(results_per_class,0,_num_classes*sizeof(int));
        memset(latency_per_class,0,_num_classes*sizeof(double));

    }

private:
    int _num_classes;
}__attribute__((__aligned__(64)));


/**
 * @brief The ReconfEmitter class is a reconfiguration message sent from the Controller to the Emitter
 * Its member are public: this a run time support message only and will be handled only the run time support
 * Some utility methods are provided.
 */
class ReconfEmitter{

public:
    ReconfTag tag;                  //identifies the type of message
    int par_degree_changes;         //identifies how many worker have been added (if >0) or removed (if <0). It is equal to zero if there are no changes
    char *scheduling_table;         //the new scheuling table
    ff::SWSR_Ptr_Buffer **wqueues;  //queues to the newly spawned workers (if any)

    /**
     * @brief ReconfEmitter constructor used for a message for increasing the par degree
     * Note: the scheduling table has to be copied since (at the moment of the creation of this message) it is still in use by the emitter
     * @param par_changes: changes in par degree. It has to be greater than zero
     * @param num_classes number of
     * @param scheduling_table
     */
    ReconfEmitter(int par_changes, int num_classes,char *scheduling_table)
    {
        par_degree_changes=par_changes;
        _num_classes=num_classes;
        this->scheduling_table=new char[num_classes]();
        //copy the passed scheduling table
        memcpy(this->scheduling_table,scheduling_table,num_classes*sizeof(char));
        if(par_changes>0) //we are going to increase the parallelism degree. We need other info
        {
            tag=ReconfTag::INCREASE_PAR_DEGREE;
            //declare the space for queueus
            wqueues=new ff::SWSR_Ptr_Buffer*[par_changes]();
        }
        else
        {
            wqueues=nullptr;
            if(par_changes<0)
                tag=ReconfTag::DECREASE_PAR_DEGREE;
            else
                tag=ReconfTag::REBALANCE;
         }
    }


    ~ReconfEmitter()
    {

        delete[] scheduling_table;
        //NOTE: i want to delete only the arrays of pointers not the queues themselves: the delete[] does not know how to delete them
        if(wqueues!=nullptr)
            delete[] wqueues;
    }

    /**
     * @brief  copy constructor not defined
     */
    ReconfEmitter ( const ReconfEmitter & ){
        throw std::logic_error("Copy Constructor for ReconfEmitter not defined!\n");
    }




private:
    int _num_classes;
}__attribute__((__aligned__(64)));


/**
 * @brief The ReconfCollector class represent a reconfiguration message sent to the Collector by Controller
 * Its member are public
 */
class ReconfCollector{
public:
    ReconfTag tag;                  //identifies the type of message
    int par_degree_changes;         //identifies how many worker have been added (if >0) or removed (if <0). It is equal to zero if there are no changes
    ff::SWSR_Ptr_Buffer **wqueues;      //queues from the newly spawned workers (if any)


    /**
     * @brief ReconfCollector Constructor
     * @param par_changes changes in parallelism degree: it can be either negative or positive
     */
    ReconfCollector(int par_changes)
    {
        par_degree_changes=par_changes;
        if(par_changes>0) //we are going to increase the parallelism degree. We need other info
        {
            tag=ReconfTag::INCREASE_PAR_DEGREE;
            //declare the space for queueus
            wqueues=new ff::SWSR_Ptr_Buffer*[par_changes]();
        }
        else
        {
            wqueues=nullptr;
            if(par_changes<0)
                tag=ReconfTag::DECREASE_PAR_DEGREE;

         }
    }

    ~ReconfCollector()
    {
        if(wqueues!=nullptr)
            delete[] wqueues;
    }

    /**
     * @brief  copy constructor not defined
     */
    ReconfCollector ( const ReconfCollector & ){
        throw std::logic_error("Copy Constructor for ReconfCollector not defined!\n");
    }

}__attribute__((__aligned__(64)));

}
#endif
