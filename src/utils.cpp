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

  Definitions of various utility functions
*/
#include <stdio.h>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <mammut/mammut.hpp>
using namespace mammut;
using namespace mammut::topology;


std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems){
    std::stringstream ss(s);

    std::string item;
    while(std::getline(ss, item, delim)){
        elems.push_back(item);
    }
    return elems;
}



std::map<std::pair<int,int>,double>  *loadVoltageTable(std::string fileName){
    std::ifstream file;
    file.open(fileName.c_str());
    if(!file.is_open()){
        throw std::runtime_error("Impossible to open the specified voltage table file.");
    }
    std::map<std::pair<int,int>,double> *voltages=new std::map<std::pair<int,int>,double>();
    std::string line;
    std::vector<std::string> fields;

    while(std::getline(file, line)){
        /** Skips lines starting with #. **/
        if(line.at(0) == '#'){
            continue;
        }
        fields.clear();
        split(line, ';',fields);

        int n= std::stoi(fields.at(0));

        int freq= std::stoi(fields.at(1));
        double v=std::stof(fields.at(2));
        voltages->insert(std::make_pair(std::make_pair(n,freq),v));
    }

    // For read them
   /*   std::map<std::pair<int,int>,double>::iterator iter;
    for(iter=voltages->begin(); iter!=voltages->end();++iter)
    {
        std::cout << "["<<(iter->first).first<<","<<(iter->first).second<<"]: "<<iter->second<<std::endl;
    }*/
    file.close();
    return voltages;

}


std::vector<int>* getCoreIDs()
{
    Mammut m;
    Topology* topology = m.getInstanceTopology();
    int num_phys_cores=0;
    int num_virt_cores=0;

    std::vector<Cpu*> cpus = topology->getCpus();
    //cout << "The machine has [" << cpus.size() << " CPUs]" << endl;
    for(size_t i = 0; i < cpus.size(); i++)
    {
        Cpu* cpu = cpus.at(i);
        num_phys_cores+= cpu->getPhysicalCores().size();
        num_virt_cores+= cpu->getVirtualCores().size();
     }
     std::vector<int> *core_ids=new std::vector<int>();

     std::vector<VirtualCore*> virtualCores = topology->getVirtualCores();

     //now take the id of one virtual core per physical core;
     //assuming that physical core are number starting from zero
     int curr_phys_core=0;
     bool constantTSC=true;
     for(size_t i = 0; i < virtualCores.size(); i++)
     {
       VirtualCore* vc = virtualCores.at(i);
       if(vc->getPhysicalCoreId()==curr_phys_core)
       {
           core_ids->push_back(vc->getVirtualCoreId());
           curr_phys_core++;
       }
       if(!vc->areTicksConstant())
           constantTSC=false;
     }
     if(!constantTSC)
         std::cout<< "ATTENTION: the machine has not constant TSC. The program may not work since relies on this to get accurate timings" <<std::endl;
     return core_ids;

}

std::string intToString(int x){
    std::string s;
    std::stringstream out;
    out << x;
    return out.str();
}

float getMaximumFrequency()
{
    //get maximum frequency of the machine. Mammut report it in Khz
    mammut::cpufreq::CpuFreq* frequency=mammut::cpufreq::CpuFreq::local();
    mammut::cpufreq::Domain* domain = frequency->getDomains().at(0);
    std::vector<mammut::cpufreq::Frequency> available_frequencies = domain->getAvailableFrequencies();

    //do not consider eventual 'turboboost' frequency
    if(intToString(available_frequencies.back()).at(3) == '1')
    {
        available_frequencies.pop_back();
    }
    return available_frequencies.back()/1000.0;
}

float getMinimumFrequency()
{
    //get maximum frequency of the machine. Mammut report it in Khz
    mammut::cpufreq::CpuFreq* frequency=mammut::cpufreq::CpuFreq::local();
    mammut::cpufreq::Domain* domain = frequency->getDomains().at(0);
    std::vector<mammut::cpufreq::Frequency> available_frequencies = domain->getAvailableFrequencies();


    return available_frequencies.front()/1000.0;
}

