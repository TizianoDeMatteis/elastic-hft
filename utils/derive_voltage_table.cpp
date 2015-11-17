
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
*/
#include <iostream>
#include <vector>
#include <mammut/cpufreq/cpufreq.hpp>
#include <mammut/energy/energy.hpp>
#include <mammut/cpufreq/cpufreq-linux.hpp>


using namespace std;

//This is used to derive an aproximate voltage table of a machine

int main()
{
    mammut::topology::Topology* topology=mammut::topology::Topology::local();
    vector<mammut::topology::Cpu*> cpus=topology->getCpus();
    int n_physical_core=0;

    for(size_t i = 0; i < cpus.size(); i++){
           mammut::topology::Cpu* cpu = cpus.at(i);
           n_physical_core+=cpu->getPhysicalCores().size();
       }
    cout << "This machine has "<<cpus.size()<< " CPUs and "<<n_physical_core<<" physical cores"<<endl;

    mammut::cpufreq::CpuFreq* frequency=mammut::cpufreq::CpuFreq::local();
    mammut::cpufreq::Domain* domain = frequency->getDomains().at(0);
    vector<mammut::cpufreq::Frequency> available_frequencies = domain->getAvailableFrequencies();
    std::cout << "Starting computing the voltage table: it should take approximately " <<5*3*available_frequencies.size()<<" seconds" << std::endl;
    //we compute it only for half the core of a cpu
    int ncore=((cpus.at(0))->getPhysicalCores().size())/2;
    std::cout<< "Computing with "<<ncore<<std::endl;
    mammut::cpufreq::VoltageTable vt = domain->getVoltageTable(ncore,true);
    //replicate it
    mammut::cpufreq::VoltageTable r;

    for(size_t i = 1; i <= n_physical_core; i++){
        for(mammut::cpufreq::VoltageTableIterator iterator = vt.begin(); iterator != vt.end(); iterator++){
					//replicate the info for all number of cores
					//tha table is a map of pair<NUM_COR,FREQ>->Voltage
					auto key=make_pair(i,get<1>(std::get<0>(*iterator)));
                    // "volt: "<<std::get<1>(*iterator);
					r.insert(make_pair(key,get<1>(*iterator)));
                    //r.insert(*iterator);
					


                }
    }
    mammut::cpufreq::dumpVoltageTable(r,"voltages.txt");

       std::cout << "Voltage table computed and dumped on file" << std::endl;
//    //the value of the frequency returned by the strategy
//    mammut::cpufreq::Frequency freq_opt=0,freq_pred=0;
//    mammut::cpufreq::CpuFreq* frequency= mammut::cpufreq::CpuFreq::local();
//    //get domains
//    vector<mammut::cpufreq::Domain*> domains = frequency->getDomains();
//    //get frequency (NOTE: for the moment being we assume that all the domain have the same set of frequencies)
//    vector<mammut::cpufreq::Frequency> available_frequencies = domains.at(0)->getAvailableFrequencies();
//    //set Governor to UserSpace to all the domains

}
