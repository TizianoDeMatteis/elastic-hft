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
    This class it is a simpler container/updater for all the metrics that are derived by the controller
    starting from the ones reported by Emitter, Workers and Collector

    NOTE: for the moment being all fields are public
 */
#ifndef DERIVEDMETRICS_HPP
#define DERIVEDMETRICS_HPP
#include <math.h>
#include "messages.hpp"

/**
 * @brief The DerivedMetrics class contains all the performance metrics
 * derived by the monitoring data
 */
class DerivedMetrics{
public:
    //this will contain the calculation time for each class, updated with the values coming from workers
    double* tcalc_per_class;
    // weighted tcalc_per_class it will containt the product freq(i)*tcalc_per_class(i) (where i is a class)
    double* weighted_tcalc_per_class;
    //current load factor for the various workers (it will consider only the triggering tuples)
    double *rho;
    //average tcalc per worker (considering only the slide computations)
    double *weighted_tcalc_per_worker;
    //frequencies of the classes assigned to a particular worker
    double *freq_to_worker;
    //ideal rho of the whole module  (not considering load balancing problems)
    double module_rho;

    //the various performance metrics computed upon monitored data
    double trigger_per_second;//number of tuple that trigger a slide per second
    //double additional_elements; //the number of additional elements received by the emitter and not yet counted by it
    double tta_msec; //trigger interarrival time (in msec)
    double module_tcalc; //the tcalc of the whole parallel module (in seconds)

    double c_arr; //coefficient of variation of arrival
    double c_serv; //coefficient of variation of services

    /**
     * @brief DerivedMetrics constructor
     * @param num_classes
     * @param max_workers
     */
    DerivedMetrics(int num_classes, int max_workers)
    {
        tcalc_per_class=new double[num_classes]();
        weighted_tcalc_per_class=new double[num_classes]();
        rho=new double[max_workers]();
        weighted_tcalc_per_worker=new double[max_workers]();
        freq_to_worker=new double[max_workers]();
        _num_classes=num_classes;
        _max_workers=max_workers;

    }

    /**
     * @brief updateMetrics update the value of the various metrics
     * @param num_workers current number of workers
     * @param em emitter monitored data (message)
     * @param wm workers monitored data (message)
     * @param cm collector monitored data (message)
     */
    void updateMetrics(int num_workers,msg::EmitterMonitoring *em, msg::WorkerMonitoring **wm,msg::CollectorMonitoring *cm)
    {
        //reset values
        module_tcalc=0;
        for(int i=0;i<num_workers;i++)
        {
            rho[i]=0;
            freq_to_worker[i]=0;
            weighted_tcalc_per_worker[i]=0;
        }
        tta_msec=em->ta_timestamp;
        trigger_per_second=(1000.0/tta_msec);

        //compute the various calculation times metrics
        for(int i=0;i<_num_classes;i++)
        {
            //for each class, take the info from the worker to which was assigned
            char assw=em->scheduling_table[i]-1;

            if(wm[assw]->tcalc_per_class[i]>0) //there is some data (otherwise we will keep the one of the previous mon. step)
            {
                double class_freq=((double)em->elements_per_class[i])/em->elements;
                //average tcalc (we compute it here, since in the last mon. step we could have experienced state migration and the same class has been computed may have been processed by various workers)
                tcalc_per_class[i]=(wm[assw]->tcalc_per_class[i]/wm[assw]->computations_per_class[i])/1000; //msec
                //weigth it: we are considered the frequencies measured by the emitter (that is up to the max load sustainable with this configuration)
                weighted_tcalc_per_class[i]=class_freq*tcalc_per_class[i];
                // wtcalc_per_class[i]=class_freq*(1000/MONITORING_STEP)*tcalc_per_class[i];
                freq_to_worker[assw]+=class_freq;
                //compute the averaged tcalc considering only the elements received by the worker
                weighted_tcalc_per_worker[assw]+=tcalc_per_class[i]*((double)wm[assw]->elements_per_class[i]/wm[assw]->elements_rcvd); //in realta dovremmo dividere entrambi per 500 ma siamo li
                //comp_per_class+=wm[assw]->computations_per_class[i];
                //count it in the whole weighted tcalc
                module_tcalc+=weighted_tcalc_per_class[i];

            }
            //Network calculus metrics, not needed for the moment
            //tcalc_sum+=tcalc_per_class[i];
            //max_tcalc_sum+=wm[assw]->max_tcalc_per_class[i]/1000;
        }
        //compute the utilization factor of the various workers
        for(int i=0;i<num_workers;i++)
        {
            rho[i]=weighted_tcalc_per_worker[i]/1000.0; //in second
            rho[i]/=1/((1000.0/MONITORING_STEP)*(wm[i]->computations));//normalize it to a second basis
        }


        //Compute the utilization factor of the whole module (under the assumption of a perfectly balanced load)
        module_rho=(module_tcalc/num_workers)/(tta_msec);

        //Compute the coefficient of variation of arrivals and services
        //for the services we will consider the calculation time of the workers
        //and compute the standard deviation considering the module_tcalc
        //This is actually the coefficient of variation of services for
        //a module with par degree equal to one.
        //WE ARE ASSUMING THAT THIS IS THE SAME INDEPEND. OF THE NUM OF WORKERS
        c_arr=em->std_dev_timestamp/tta_msec;

        double std_dev=0;
        int samples=0;
        //compute with the worker data
        for(int i=0;i<num_workers;i++)
        {
            for(int j=0;j<(wm[i]->calc_times)->size();j++)
            {

                std_dev+=((wm[i]->calc_times)->at(j)-module_tcalc)*((wm[i]->calc_times)->at(j)-module_tcalc);
                samples++;
            }
        }
        std_dev=sqrt(std_dev/((double)samples))/1000.0;
        //c_serv=std_dev/module_tcalc;
        c_serv=cm->c_serv;

    }


    ~DerivedMetrics()
    {
        delete[] tcalc_per_class;
        delete[] weighted_tcalc_per_class;
        delete[] rho;
        delete[] weighted_tcalc_per_worker;
        delete[] freq_to_worker;
    }

private:
    int _num_classes;
    int _max_workers;


};

#endif // DERIVEDMETRICS_HPP

