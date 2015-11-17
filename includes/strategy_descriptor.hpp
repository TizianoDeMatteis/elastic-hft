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

#ifndef STRATEGY_HPP
#define STRATEGY_HPP
#include "config.hpp"
enum class StrategyType{
    NONE,
    LATENCY,
    LATENCY_ENERGY,
    LATENCY_RULE,
    TPDS
};

/**
 * @brief The StrategyDescriptor class it is a descriptor for an adaptation strategy,
 * whose detail are read from file. The configuration file has to respect a proper sintax:
 * - lines that begin with a dash (#) are threated as comment
 * - lines that represent parameters are in the form <attribute>=<value> (also with spaces)
 *
 * To be a valid configuration file for a strategy, the file passed has to have the following parameters (all lower case)
 * - control_step= duration of the control step expressed in milliseconds
 * - strategy=
 *      - none: no adaptation strategy has to be used
 *      - bandwidth: the strategy assure that the operator is not a bottleneck
 *      - bandwidth_energy: as before but tries to minimize energy consumption
 *      - latency: assure that the average latency is belowe a given threshold
 *      - latency_energy: as before but tries to minimize the energy consumption
 *      - tpds: it is the strategy defined in the paper "Elastic scaling for data stream processing"
 *      - rule_based: strategy that applies a simple rule on utilization values
 *      - lantecy_rule: assure avergae latency by means of simple rules (no models nor predictive strategy)
 * - depending from the strategy type other parameters are required
 *      - alpha=<value>, beta=<value>, gamma=<value>: are the parameters of the
 *                  optimization problem (as described in the article ...). They are
 *                  positive float numbers. They are required for bandwidth, bandwidth_energy,
 *                  latency, latency_energy
 *      - threshold=<value>: required for latency, latency_energy and latency_rule. Describe the desired
 *                  latency threshold in millisecond (positive float number).
 *      - max_level=<value>, change_sensitivity=<value>, cong_threshold=<value> are required
 *                  by the tpds strategy. Max level is the maximum number of workers -1 that you want to use
 *      - rho_min=<value>, rho_max=<value>: positive float number in [0,1]. Required by rule_based
 *
 *
 * Please note that for this testing version, we require in any case to insert the control_step parameter, in order
 * to take all the interesting perfomance metrics.
 * It will be removed afterwards
 *
 * Other fields are ignored if present
 */

class StrategyDescriptor
{
public:
    StrategyType type;
    double alpha,beta,gamma;
    double threshold;
    int horizon;
    bool predictive=false; //states if the required strategy is predictive or not

    //parameters for TPDS
    int max_level; //that is referred as L* in the article (max_workers-1 in pianosau)
    double change_sensitivity; //the alpha parameter NON SI CAPISCE SE DEVE ESSERE PICCOLO O GRANDE
    double congestion_threshold;

    //parameters for rule_based
    double rho_min, rho_max;

    //Control step: interval between two strategy evaluations (in milliseconds)
    int control_step;
    StrategyDescriptor(std::string const& configFile)
    {
        //Read the configuration file
        Configuration c(configFile);
        //get the type of strategy and the various parameters
        std::string strategy_type=c.getValue("strategy");
        if(strategy_type.empty())
            throw std::runtime_error("Bad configuration file: strategy type missing");

        if(strategy_type.compare(strategy_none)==0)
        {
            type=StrategyType::NONE;
            predictive=false;
            control_step=1000;
            return;
        }
        std::string ctrl_step=c.getValue("control_step");
        if(ctrl_step.empty())
            throw std::runtime_error("Bad configuration file: control_step missing. In this testing version is needed for performance measurements");
        control_step=std::stoi(ctrl_step);

        if(strategy_type.compare(strategy_latency)==0)
        {
            //strategy for bandwidth_energy
            type=StrategyType::LATENCY;
            //get alfa beta and gamma
            getAlphaBetaGammaHorizon(c);
            //get threshold
            std::string thr=c.getValue("threshold");
            if(thr.empty())
                throw std::runtime_error("Bad configuration file: threshold parameter missing");
            threshold=std::stod(thr);
            predictive=true;
            return;
        }
        if(strategy_type.compare(strategy_latency_energy)==0)
        {
            //strategy for bandwidth_energy
            type=StrategyType::LATENCY_ENERGY;
            //get alfa beta and gamma
            getAlphaBetaGammaHorizon(c);
            //get threshold
            std::string thr=c.getValue("threshold");
            if(thr.empty())
                throw std::runtime_error("Bad configuration file: threshold parameter missing");
            threshold=std::stod(thr);
            predictive=true;
            return;
        }

        if(strategy_type.compare(strategy_tpds)==0)
        {
            //tpds
            type=StrategyType::TPDS;
            //get parameters
            std::string par=c.getValue("max_level");
            if(par.empty())
                throw std::runtime_error("Bad configuration file: max_level parameter missing");
            max_level=std::stoi(par);
            par=c.getValue("change_sensitivity");
            if(par.empty())
                throw std::runtime_error("Bad configuration file: change_sensitivity parameter missing");
            change_sensitivity=std::stod(par);
            par=c.getValue("cong_threshold");
            if(par.empty())
                throw std::runtime_error("Bad configuration file: cong_threshold parameter missing");
            congestion_threshold=std::stod(par);
            return;
        }

        if(strategy_type.compare(strategy_latency_rule)==0)
        {
            //latency contrained with rules
            type=StrategyType::LATENCY_RULE;
            //get threshold
            std::string thr=c.getValue("threshold");
            if(thr.empty())
                throw std::runtime_error("Bad configuration file: threshold parameter missing");
            threshold=std::stod(thr);
            predictive=false;
            return;


        }


        std::cerr<<"Strategy type not recognized, assume none as default"<<std::endl;
        type=StrategyType::NONE;


    }

    char *toString()
    {
        char *ret=new char[100];
        switch(type)
        {
            case StrategyType::NONE:
                sprintf(ret,"none");
            break;
            case StrategyType::LATENCY:
                sprintf(ret,"latency. Horizon=%d. Alpha=%.2f, Beta=%.2f, Gamma=%.2f. Threshold=%.1f. Control step=%d",horizon,alpha,beta,gamma,threshold, control_step);
            break;
            case StrategyType::LATENCY_ENERGY:
                sprintf(ret,"latency_energy. Horizon=%d. Alpha=%.2f, Beta=%.2f, Gamma=%.2f. Threshold=%.1f. Control step=%d",horizon,alpha,beta,gamma,threshold,control_step);
            break;
            case StrategyType::TPDS:
                sprintf(ret,"tpds. Change_sensitivity= %.2f, Congestion threshold=%.2f. Max_level: %d. Control step=%d",change_sensitivity,congestion_threshold,max_level,control_step);
            break;
            case StrategyType::LATENCY_RULE:
                sprintf(ret,"latency_rule. Theshold=%.1f. Control step=%d",threshold,control_step);
            break;
        }
        return ret;

    }

    void print()
    {
        std::cout<<"[Strategy type: ";
        switch(type)
        {
            case StrategyType::NONE:
                std::cout << "none";
            break;
            case StrategyType::LATENCY:
                std::cout<< "latency. Horizon="<<horizon<<". Alpha="<<alpha<<", Beta="<<beta<<", Gamma="<<gamma<<". Threshold="<<threshold;
            break;
            case StrategyType::LATENCY_ENERGY:
                std::cout<< "latency_energy. Horizon="<<horizon<<". Alpha="<<alpha<<", Beta="<<beta<<", Gamma="<<gamma<<". Threshold="<<threshold;
            break;
            case StrategyType::TPDS:
                std::cout<<"tpds. Change_sensitivity="<<change_sensitivity<<", Congestion threshold="<<congestion_threshold<<", Max_level="<<max_level;
            break;
            case StrategyType::LATENCY_RULE:
                std::cout<<"latency_rule. Threshold= "<<threshold;
            break;
        }

        std::cout<<"]"<<std::endl;
    }

private:
    const std::string strategy_none="none";
    const std::string strategy_latency="latency";
    const std::string strategy_latency_energy="latency_energy";
    const std::string strategy_tpds="tpds";
    const std::string strategy_rule="rule_based";
    const std::string strategy_latency_rule="latency_rule";

    /*
     * Support method, called for various type of strategies
     */
    void getAlphaBetaGammaHorizon(Configuration c)
    {
        std::string par=c.getValue("alpha");
        if(par.empty())
            throw std::runtime_error("Bad configuration file: alpha parameter missing");
        alpha=std::stod(par);
        par=c.getValue("beta");
        if(par.empty())
            throw std::runtime_error("Bad configuration file: beta parameter missing");
        beta=std::stod(par);
        par=c.getValue("gamma");
        if(par.empty())
            throw std::runtime_error("Bad configuration file: gamma parameter missing");
        gamma=std::stod(par);
        par=c.getValue("horizon");
        if(par.empty())
            throw std::runtime_error("Bad configuration file: horizon parameter missing");
        horizon=std::stoi(par);
    }


};

#endif // STRATEGY_HPP

