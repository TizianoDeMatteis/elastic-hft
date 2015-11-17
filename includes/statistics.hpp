/*
 * ---------------------------------------------------------------------

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
  Execution Statistics that are collected by the controllorer
  and collector and dumped to file.

*/
#ifndef STATISTICS_HPP
#define STATISTICS_HPP

#include <vector>
#include <mammut/cpufreq/cpufreq.hpp>
#include <mammut/energy/energy.hpp>
#include "general.h"
#include <string>
using namespace std;

namespace stats{
/**
 * @brief The ReconfigurationStatistics class contains a set of statistics (such as current par degree, energy consumption and so on)
 *        taken by the Controller at each monitoring step
 */
class ReconfigurationStatistics{

public:
    ReconfigurationStatistics()
    {
        _times=new vector<double>();
        _par_degrees=new vector<char>();
        _frequencies=new vector<mammut::cpufreq::Frequency>();
        _joules_core=new vector<double>();
        _joules_cpu=new vector<double>();
        _tot_reconf=0;
        _reconf_par_degree=0;
        _reconf_freq=0;
        _tot_joules_core=0;
        _tot_joules_cpu=0;
        _num_class_rebalancing=0;

        //reserve some space
        _times->reserve(_reserved_space);
        _par_degrees->reserve(_reserved_space);
        _frequencies->reserve(_reserved_space);
        _joules_core->reserve(_reserved_space);
        _joules_cpu->reserve(_reserved_space);

    }

    void addStats(double time, char par_degree,mammut::cpufreq::Frequency freq)
    {
        _times->push_back(time);
        _par_degrees->push_back(par_degree);
        _frequencies->push_back(freq);

        if(_times->size()>1)
        {
            //count the number of reconfigurations
            if(_par_degrees->at(_par_degrees->size()-2)!=par_degree)
            {
                _reconf_par_degree++;
                _tot_reconf++;
                if(_frequencies->at(_frequencies->size()-2)!=freq)
                    _reconf_freq++;
            }
            else
            {
                if(_frequencies->at(_frequencies->size()-2)!=freq)
                {
                    _tot_reconf++;
                    _reconf_freq++;
                }

            }
        }
    }

    void setNumClassRebalancing(int num)
    {
        _num_class_rebalancing=num;

    }

    //this have to be called after addStats
    void addEnergyStats( double joule_core,double joule_cpu)
    {
        _joules_core->push_back(joule_core);
        _joules_cpu->push_back(joule_cpu);
        _tot_joules_core+=joule_core;
        _tot_joules_cpu+=joule_cpu;
    }

    void writeToFile(char *name)
    {
        FILE *fpar=fopen(name,"w");
        fprintf(fpar,"#Time\tPar_degree\tFrequency\n");
        for(int i=0;i<_times->size();i++)
        {

            fprintf(fpar,"%6.3f\t%d\t%d\n",_times->at(i),_par_degrees->at(i),(int)_frequencies->at(i));
        }
        fclose(fpar);
    }

    //List of get methods for all the stats
    int getStatsNumber()
    {
        return _times->size();
    }

    double getTime(int i)
    {
        return _times->at(i);
    }

    char getParDegree(int i)
    {
        return _par_degrees->at(i);
    }

    int getNumClassRebalances()
    {
        return _num_class_rebalancing;
    }

    mammut::cpufreq::Frequency getFrequency(int i)
    {
        return _frequencies->at(i);
    }

    double getJouleCore(int i)
    {
        return _joules_core->at(i);
    }

    double getJouleCpu(int i)
    {
        return _joules_cpu->at(i);
    }

    //get total counts

    int getTotReconf()
    {
        return _tot_reconf;
    }

    int getParDegreeReconf()
    {
        return _reconf_par_degree;
    }

    int getFreqReconf()
    {
        return _reconf_freq;
    }

    double getTotJoulesCore()
    {
        return _tot_joules_core;
    }

    double getTotJoulesCpu()
    {
        return _tot_joules_cpu;
    }

    ~ReconfigurationStatistics()
    {
        delete _times;
        delete _par_degrees;
        delete _frequencies;
    }

private:
    const int _reserved_space=300;
    vector<double> *_times;                                 //the time (in ticks) to which the statiscs refer
    vector<char> *_par_degrees;                             //for each time (in _times) it contains the par degree
    vector<mammut::cpufreq::Frequency> *_frequencies;       //the frequencies
    vector<double> *_joules_core;                           //the joules consumed by incore components
    vector<double> *_joules_cpu;                            //the joules consumed by the whole cpu
    int _tot_reconf;                                        //total number of reconfiguration
    int _reconf_par_degree;                                 //number of reconfiguration that regards the par degree
    int _reconf_freq;                                       //number of reconfiguration that regads the frequency only
    double _tot_joules_cpu;                                 //the sum for each step
    double _tot_joules_core;                                //the sum for each step
    int _num_class_rebalancing;                             //the number of control step that require a class rebalancing (i.e. the configuration is the same but we rebalance between workers)
};


/**
 * @brief The ComputationStatistics class contains stats about the number of computation per class, their average computing time and so on...
 * Note: for the moment being they are taken by the Controller. In this way the could be approximated if some monitoring message is lost
 */
class ComputationStatistics
{
public:
    ComputationStatistics(int num_classes)
    {
        _elements_per_class=new int[num_classes]();
        _tot_calc_times_per_class=new double[num_classes]();
        _results_per_class=new int[num_classes]();
        _num_classes=num_classes;


    }

    /**
     * @brief addElementsToClass adds the number of elements passed to the count of a certain class
     * @param i class
     * @param el number of elements
     */
    void addElementsToClass(int i, int el)
    {
        _elements_per_class[i]+=el;
    }

    /**
     * @brief addCompTimeToClass add a monitored computation time to the count of a certain class
     * @param i class
     * @param ct computation time
     */
    void addCompTimeToClass(int i, double ct)
    {
        _tot_calc_times_per_class[i]+=ct;
    }

    /**
     * @brief addResultsToClass add the number of results to the count of a certain class
     * @param i class
     * @param nr computation time
     */
    void addResultsToClass(int i, int nr)
    {
        _results_per_class[i]+=nr;
    }

    void writeToFile(char *name)
    {
        FILE* fclass=fopen(name,"w");
        fprintf(fclass,"#Number of classes: %d\n#",_num_classes);
        fprintf(fclass, "#CLASS_ID\tFREQ\tASSIGNED_WORKER\tCOMP_TIME\n");
        for(int i=0;i<_num_classes;i++)
            fprintf(fclass,"%d\t%d\t%d\t%6.3f\n",i,_elements_per_class[i],_results_per_class[i],_tot_calc_times_per_class[i]/_results_per_class[i]);

        fclose(fclass);
    }

    ~ComputationStatistics(){
        delete[] _elements_per_class;
        delete[] _results_per_class;
        delete[] _tot_calc_times_per_class;
    }



private:
    int _num_classes;
    int *_elements_per_class;                //number of elements received from the input stream
    double *_tot_calc_times_per_class;       //total computation time throughout the execution
    int *_results_per_class;                //number of results produced per class

};

/**
 * @brief The ExecutionStatistics class is used by the Collector, to keep track of various execution stats
 * during the execution (e.g. throughput, latency, ...)
 */
class ExecutionStatistics{
public:

    ExecutionStatistics()
    {
        _times=new vector<double>();
        _recvd_results=new vector<int>();
        _latencies=new vector<double>();
        _lat_percentiles95=new vector<double>();
        _lat_percentiles99=new vector<double>();
        _lat_top=new vector<double>();
        _std_devs=new vector<double>();

        _times->reserve(_reserved_space);
        _recvd_results->reserve(_reserved_space);
        _latencies->reserve(_reserved_space);
        _lat_percentiles95->reserve(_reserved_space);
        _lat_percentiles99->reserve(_reserved_space);
        _lat_top->reserve(_reserved_space);
        _std_devs->reserve(_reserved_space);


    }

    /**
     * @brief addStat add a stat
     * @param seconds the time at which the measurements refer
     * @param recvd_results the number of received results from the last measurement
     * @param latencies the avg latency from the last measurement
     * @param lat_percentile95 a 95-percentile on latencies
     * @param lat_percentile99 99-percentile on latencies
     * @param lat_top the highest latency recorded
     * @param std_dev the standard deviation
     */
    void addStat(double seconds, int recvd_results, double latencies, double lat_percentile95, double lat_percentile99, double lat_top,double std_dev)
    {
        _times->push_back(seconds);
        _recvd_results->push_back(recvd_results);
        _latencies->push_back(latencies);
        _lat_percentiles95->push_back(lat_percentile95);
        _lat_percentiles99->push_back(lat_percentile99);
        _lat_top->push_back(lat_top);
        _std_devs->push_back(std_dev);


    }

    int getStatsNumber()
    {
        return _times->size();
    }

    double getTime(int i)
    {
        return _times->at(i);
    }

    int getRecvResults(int i)
    {
        return _recvd_results->at(i);
    }

    double getLatency(int i)
    {
        return _latencies->at(i);
    }

    double getLatencyPercentile95(int i)
    {
        return _lat_percentiles95->at(i);
    }
    double getLatencyPercentile99(int i)
    {
        return _lat_percentiles99->at(i);
    }
    double getLatencyTop(int i)
    {
        return _lat_top->at(i);
    }

    double getStdDev(int i)
    {
        return _std_devs->at(i);
    }

    /**
     * @brief writeToFile write the statistics to file
     * @param name name of the file
     */
    void writeToFile(char *name)
    {
        //we use FILE* since it is more easy
        FILE* flat=fopen(name,"w");
        fprintf(flat,"#Second\tNum_results\tLatency\tPercentile\n");
        for(int i=0;i<_times->size();i++)
        {
            fprintf(flat,"%3.3f\t%Ld\t%6.3f\t%6.3f\n",_times->at(i),_recvd_results->at(i),_latencies->at(i),_lat_percentiles95->at(i));
        }
        fclose(flat);

    }

    ~ExecutionStatistics()
    {
        delete _times;
        delete _recvd_results;
        delete _latencies;
        delete _lat_percentiles95;
        delete _lat_percentiles99;
        delete _lat_top;
        delete _std_devs;
    }


private:
    const int _reserved_space=300;
    vector<double> *_times;                     //the time (second) at which we take the metrics
    vector<int> *_recvd_results;                //#received results
    vector<double> *_latencies;                 //avg latency
    vector<double> *_lat_percentiles95;          //95 percentile on latency
    vector<double> *_lat_percentiles99;         //99 percentile
    vector<double> *_lat_top;                   //top latency
    vector<double> *_std_devs;                  //standard deviations on measured latencies of each step

};


/**
    The copyright of the code of RunningStat is due to John D. Cook
    source http://www.johndcook.com/blog/standard_deviation/
 */
class RunningStat
{

    //ATTENTION: do not use with big number (that is clock counter for example)
    public:
        RunningStat() : m_n(0) {}

        void Clear()
        {
            m_n = 0;
        }

        void Push(double x)
        {
            m_n++;

            // See Knuth TAOCP vol 2, 3rd edition, page 232
            if (m_n == 1)
            {
                m_oldM = m_newM = x;
                m_oldS = 0.0;
            }
            else
            {
                m_newM = m_oldM + (x - m_oldM)/m_n;
                m_newS = m_oldS + (x - m_oldM)*(x - m_newM);

                // set up for next iteration
                m_oldM = m_newM;
                m_oldS = m_newS;
            }
        }

        int NumDataValues() const
        { return m_n;  }

        double Mean() const
        {  return (m_n > 0) ? m_newM : 0.0; }

        double Variance() const
        {  return ( (m_n > 1) ? m_newS/(m_n - 1) : 0.0 ); }

        double StandardDeviation() const
        {
            return sqrt( Variance() );
        }

    private:
        int m_n;
        double m_oldM, m_newM, m_oldS, m_newS;
    };
}
#endif // STATISTICS_HPP

