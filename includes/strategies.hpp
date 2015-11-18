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
	Headers contains a series of adaptation strategies definitions (and relative data structures)
	for the funcional partitioning case

*/
#ifndef _STRATEGIES
#define _STRATEGIES
#include <math.h>
#include <map>
#include <mammut/cpufreq/cpufreq.hpp>
#include <limits>
#include "strategy_descriptor.hpp"
#include <climits>

#define MAX_RHO_MODULE 0.95 //the maximum rho that the module can have before being considered bottleneck
using namespace std;
/**
	Reconfiguration choice for energy aware strategies
*/
typedef struct{
	int nw;
	mammut::cpufreq::Frequency freq;
}reconf_choice_energy_t;

/**
	Auxiliary function declaration: they perform the actual strategy resolution.
	The predict_* functions are simpler wrapper to them.
*/
void resolve_strategy_rt(int h, int step, int max_par_degree, int *reconf_vector, double* forecasted, double tcalc,double rt_threshold, double c_arr, double c_serv, double ksf,double alpha, double beta,double gamma, int curr_par_degree,int *result, double *min,double *exp_rt, double *kingman);
void resolve_strategy_energy_rt(int h, int step, int max_par_degree, vector<mammut::cpufreq::Frequency> available_frequencies, map<pair<int,int>,double> *voltages, reconf_choice_energy_t *reconf_vector, double* forecasted, double tcalc,double rt_threshold, double c_arr, double c_serv, double ksf,double alpha, double beta,double gamma, int curr_par_degree,mammut::cpufreq::Frequency curr_frequency,reconf_choice_energy_t *result, double *min,double *exp_rt, double *kingman);
void resolve_strategy_energy_rt_bb(int h, int step, double part_obj_funct, double first_rt, double first_king, int max_par_degree, vector<mammut::cpufreq::Frequency> available_frequencies, map<pair<int,int>,double> *voltages, reconf_choice_energy_t *reconf_vector, double* forecasted, double tcalc, double rt_threshold, double c_arr, double c_serv, double ksf, double alpha, double beta, double gamma, int curr_par_degree, mammut::cpufreq::Frequency curr_frequency, reconf_choice_energy_t *result, double *min, double *exp_rt, double *kingman/*, int &solutions_explored*/);

// TODO: sistemare qua dentro prima di fare i test definitivi(credo si tratti semplicemente di commentare per bene, il core è identico)

/*----------------------------------------------------
	Strategies that do  take into account energy
-------------------------------------------------------*/



/**
	Strategy prediction  that take into account the average response time. This has to be kept
	below a given threshold. 

    This implements the Lat-Power strategy of the paper
    The average expected response time will be computed using the Kingman Formula, that consider the
	coefficient of variation of inter arrival time and service time
	As a consequence it will assure that the number of workers 
	found for the next steps is sufficient to ingest all the incoming data.


    @param sd Strategy descriptor
	@param max_par_degree maximum parallelism degree allowed
	@param available_frequencies list of the available frequencies (computed with the Mammut lib)
	@param voltages map that contains the voltage for any combination <num_processor_used,frequency>
	@param curr_par_degree current parallelism degree
	@param curr_frequency the current frequency (expressed in KHz, as returned by the Mammut lib)
	@param forecasted the forescasted disturbance vector. It is an array with h position. In this case we will forecast the arrival rate
	@param tcalc current tcalc of the module
	@param c_arr coefficient of variation of arrivals
	@param c_serv coefficient of variation of services
	@param ksf parameter for scaling the expected response time computed with the kingman formula
	@param result pointer to a vector in which will be stored the h reconfiguration choices computed (i.e. the following par degree and frequencies)
	@param exp_rt pointer to a double in which will be stored the expected response time for the next step (after reconfigurations have been applied)
	@param kingman pointer to a double in which will be stored the response time forecasted with kingman (therefore not scaled)

*/

void predict_reconf_energy_rt(StrategyDescriptor *sd, int max_par_degree,vector<mammut::cpufreq::Frequency> available_frequencies,map<pair<int,int>,double> *voltages,int curr_par_degree,mammut::cpufreq::Frequency curr_frequency, double *forecasted,double tcalc, double c_arr, double c_serv, double ksf, reconf_choice_energy_t *result, double *exp_rt, double *kingman)
{

	double min=INT_MAX;
	//needed for calculation purposes (it describes the various combination of the reconfiguration vector)
    reconf_choice_energy_t *reconf_vector=new reconf_choice_energy_t[sd->horizon]();
    //This was used for testing the space of solution exploration
    //int solutions_explored=0;
    // resolve_strategy_energy_rt(h,0,max_par_degree,available_frequencies,voltages, reconf_vector,forecasted,tcalc, rt_threshold, c_arr, c_serv, ksf, alpha,beta,gamma,curr_par_degree,curr_frequency,result,&min,exp_rt,kingman);
    resolve_strategy_energy_rt_bb(sd->horizon,0,0,0,0,max_par_degree,available_frequencies,voltages, reconf_vector,forecasted,tcalc, sd->threshold, c_arr, c_serv, ksf, sd->alpha,sd->beta,sd->gamma,curr_par_degree,curr_frequency,result,&min,exp_rt,kingman/*,solutions_explored*/);
	//printf("Current Frequency: %d Minimum of the objective function: %f\n",curr_frequency,min);
    /*printf("RT-EN: Punto di partenza [%d,%d]. Parameters: %.2f, %.2f, %.2f. Reconf_vector: ", curr_par_degree, (int)curr_frequency,sd->alpha,sd->beta,sd->gamma);
    for(int j=0;j<sd->horizon;j++)
		printf("[%d %d]",result[j].nw,(int)result[j].freq);
    printf("\n");*/
   // fprintf(stderr,"%d\n",solutions_explored);
    delete[] reconf_vector;

}



/**
    Strategy resolution for response time. Version with branch and bound
	In addition to the predict_reconf procedure, there are other additional parameters:
	@param step the current step in the horizon: it will go from 0 to h-1 and it is used to stop the recursion
	@param reconf_vector the reconfiguration trajectory built up to now
	@param alpha, beta, gamma parameters of the objective function
	@param result the best trajectory found till now
	@param min the value of the obj function correspoonding to the best trajectory found till now
	@param part_objc_funct: if we are at step S it is the value of the objective function computed for step 0,...,S-1	
	@param first_rt: for the choosen trajectory, we have to return the expected response time. This is used for keep track of the exp. resp time of the first reconfiguration of the trajectory
	@param first_king: the same as above but for the kingman value of waiting time

    Note: the parameter solutions_explored was used in testing for measuring the number of solutions explored and
    compare it with the same number for the case of a greedy search.
*/
void resolve_strategy_energy_rt_bb(int h, int step, double part_obj_funct, double first_rt, double first_king,int max_par_degree, vector<mammut::cpufreq::Frequency> available_frequencies, map<pair<int,int>,double> *voltages, reconf_choice_energy_t *reconf_vector, double* forecasted, double tcalc,double rt_threshold, double c_arr, double c_serv, double ksf,double alpha, double beta,double gamma, int curr_par_degree,mammut::cpufreq::Frequency curr_frequency,reconf_choice_energy_t *result, double *min,double *exp_rt, double *kingman/*, int &solutions_explored*/)
{


	double resp_time,rho,kingman_prev;
    double voltage;
    double minFreqGHZ=available_frequencies.front()/1000000.0;
	for(int i=1;i<=max_par_degree;i++)
	{
		
		for(int fj=available_frequencies.size()-1; fj>=0;fj--)
		{
            //solutions_explored++;
			reconf_vector[step].nw=i;
			reconf_vector[step].freq=available_frequencies[fj];
			//evaluate the object function, if the cost is less than the previous optima, update the result
			double obj_funct=part_obj_funct;
			//we have to save the value of resp_time and kingma_prev for the first prevision step
			rho=((tcalc*curr_frequency/(double)reconf_vector[step].freq)/(double)reconf_vector[step].nw)/(MAX_RHO_MODULE/(double)forecasted[step]);
			if(rho<1)
			{
				
				// kingman_prev=(tcalc*curr_frequency)/(double)reconf_vector[j].freq+((rho/(1-rho))*((c_arr*c_arr+c_serv*c_serv)/2)*(tcalc*curr_frequency/(double)reconf_vector[j].freq)/(double)reconf_vector[j].nw);//tserv is equal to tcal/n
				//the expected waiting time according to kingman
				kingman_prev=((rho/(1-rho))*((c_arr*c_arr+c_serv*c_serv)/2)*(tcalc*curr_frequency/(double)reconf_vector[step].freq)/(double)reconf_vector[step].nw);//tserv is equal to tcal/n
				//then we obtain the expected response time as the scaled waiting time + the tcalc
				resp_time=(tcalc*curr_frequency)/(double)reconf_vector[step].freq+kingman_prev*ksf;


                double ratio=((double)resp_time)/rt_threshold;
                if(ratio>100)
                    obj_funct+=INT_MAX;
                else
                    obj_funct+=alpha*exp(ratio);
                //obj_funct+=alpha*exp(((double)resp_time)/rt_threshold);

				//beta factor of the objective function
                voltage=(*voltages)[make_pair(reconf_vector[step].nw+4,reconf_vector[step].freq)]; //+4 since there are 4 additional thread but the enumeration start from zero
				obj_funct+=beta*(voltage*voltage*reconf_vector[step].freq/1000000.0*(reconf_vector[step].nw+3));
                //As switching cost we consider the two reconfiguration choice as a vector of size 2
                //The switching cost is given by the squared norm-2 of the difference vector
                //Notice that: the frequency part is converted in GHz and its value reported in a range 0-8 with unitary step
                // in order to be comparable with the number of workers
				double norm2;
				if(step==0)
				{	
					norm2=(curr_par_degree-reconf_vector[0].nw)*(curr_par_degree-reconf_vector[0].nw);
                    //norm2+=100*(curr_frequency/1000000.0-reconf_vector[0].freq/1000000.0)*(curr_frequency/1000000.0-reconf_vector[0].freq/1000000.0);
                    double curr_f=(curr_frequency/1000000.0-minFreqGHZ)*10;
                    double past_f=(reconf_vector[0].freq/1000000.0-minFreqGHZ)*10;
                     norm2+=(curr_f-past_f)*(curr_f-past_f);
				}
				else
				{
					norm2=(reconf_vector[step].nw-reconf_vector[step-1].nw)*(reconf_vector[step].nw-reconf_vector[step-1].nw);
//                    norm2+=100*(reconf_vector[step].freq/1000000.0-reconf_vector[step-1].freq/1000000.0)*(reconf_vector[step].freq/1000000.0-reconf_vector[step-1].freq/1000000.0);
                    double freq_step=(reconf_vector[step].freq/1000000.0-minFreqGHZ)*10;
                    double freq_step_min_one=(reconf_vector[step-1].freq/1000000.0-minFreqGHZ)*10;
                    norm2+=(freq_step-freq_step_min_one)*(freq_step-freq_step_min_one);
				}
//				obj_funct+=gamma*sqrt(norm2);
                obj_funct+=gamma*(norm2);
				if(step==0) //save the value of the first reconf. step
				{
					first_rt=resp_time;
					first_king=kingman_prev;
				}
				if(step==h-1)//let's see if we have a new minimum
				{
					if(obj_funct<*min)
					{

						// printf("[%d,%d]  exp rt: %f Min: %f (rho: %f ksf: %f)\n", reconf_vector[0].nw ,(int)reconf_vector[0].freq, resp_time,obj_funct, rho,ksf);
						//printf("New minima: ");
						for(int j=0;j<h;j++)
						{
							//save the values
							result[j].nw=reconf_vector[j].nw;
							result[j].freq=reconf_vector[j].freq;
							//printf("%d ",reconf_vector[j]);
						}
						//printf(" Obj function value: %f\n",obj_funct);
						*min=obj_funct;
						//save the expect response time and the kingman value for the first step
						// *exp_rt=rt_first;
						// *kingman=king_first;
						*exp_rt= first_rt;
						*kingman=first_king;
					}
				}
				else
				{
					//let's see if we can continue with the recursion of if we can stop here
					//since the onj_funct has a value that is already greater than the current minimum
					if(obj_funct<=*min)
					{
						//continue recursion
                        resolve_strategy_energy_rt_bb(h,step+1,obj_funct,first_rt,first_king,max_par_degree,available_frequencies,voltages,reconf_vector,forecasted,tcalc,rt_threshold, c_arr, c_serv, ksf,alpha,beta,gamma,curr_par_degree,curr_frequency,result,min,exp_rt,kingman/*,solutions_explored*/);


					}
					//otherwise stop here
				}
			}
			else
			 	break; //since we start from the highest frequency, we can break here
		}
	}
}





/**
	Strategy prediction  that take into account the average response time. This has to be kept
	below a given threshold. 
    THIS IS named Lat-Node in the paper
	The average expected response time will be computed using the Kingman Formula, that consider the
	coefficient of variation of inter arrival time and service time
	As a consequence it will assure that the number of workers 
	found for the next steps is sufficient to ingest all the incoming data.
    @param sd Strategy Descriptor
	@param max_par_degree maximum parallelism degree allowed
	@param curr_par_degree current parallelism degree
	@param forecasted the forescasted disturbance vector. It is an array with h position. In this case we will forecast the arrival rate
	@param tcalc current tcalc of the module
	@param c_arr coefficient of variation of arrivals
	@param c_serv coefficient of variation of services
	@param ksf parameter for scaling the expected response time computed with the kingman formula
	@param result pointer to a vector in which will be stored the h reconfiguration choices computed (i.e. the following par degree)
	@param exp_rt pointer to a double in which will be stored the expected response time for the next step (after reconfigurations have been applied)
	@param kingam pointer to a double in which will be stored the response time forecasted with kingman (therefore not scaled)

*/
//consider also the expect response time computed considering the first step of reconfiguration
void predict_reconf_rt(StrategyDescriptor *sd, int max_par_degree,int curr_par_degree,double *forecasted,double tcalc, double c_arr, double c_serv, double ksf, int *result, double *exp_rt, double *kingman)
{

    double min=INT_MAX;
    //needed for calculation purposes (it describes the various combination of the reconfiguration vector)
    int *reconf_vector=new int[sd->horizon]();
	//eventualmente lo si pesa
    resolve_strategy_rt(sd->horizon,0,max_par_degree,reconf_vector,forecasted,tcalc, sd->threshold, c_arr, c_serv, ksf, sd->alpha,sd->beta,sd->gamma,curr_par_degree,result,&min,exp_rt,kingman);
    /*printf("RT threshold=%.1f: Parameters: %.2f, %.2f, %.2f. Minimum of the objective function: %f Forecasted resp_time: %f (Expected according kingman %.3f)\n",sd->threshold,sd->alpha,sd->beta,sd->gamma,min,*exp_rt,*kingman);
	printf("reconf_vector: ");
    for(int j=0;j<sd->horizon;j++)
		printf("%d ",result[j]);
    printf("\n");*/
    delete[] reconf_vector;
}



/**
	Strategy resolution for response time
	In addition to the predict_reconf procedure, there are other additional parameters:
	@param step the current step in the horizon: it will go from 0 to h-1 and it is used to stop the recursion
	@param reconf_vector the reconfiguration trajectory built up to now
	@param alpha, beta, gamma parameters of the objective function
	@param result the best trajectory found till now
	@param min the value of the obj function correspoonding to the best trajectory found till now
*/
	
void resolve_strategy_rt(int h, int step, int max_par_degree, int *reconf_vector, double* forecasted, double tcalc,double rt_threshold, double c_arr, double c_serv, double ksf,double alpha, double beta,double gamma, int curr_par_degree,int *result, double *min,double *exp_rt, double *kingman)
{
	double resp_time,rho,kingman_prev;
    for(int i=1;i<=max_par_degree;i++)
	{
		reconf_vector[step]=i;
		if(step<h-1)
			resolve_strategy_rt(h,step+1,max_par_degree,reconf_vector,forecasted,tcalc,rt_threshold, c_arr, c_serv, ksf,alpha,beta,gamma,curr_par_degree,result,min,exp_rt,kingman);
		else //end of recursion, evaluate the cost
		{
			//evaluate the object function, if the cost is less than the previous optima, update the result
			double obj_funct=0;
			//we have to save the value of resp_time and kingma_prev for the first prevision step
			double rt_first,king_first;
			//start summing the first gamma factor
			obj_funct+=gamma*(reconf_vector[0]-curr_par_degree)*(reconf_vector[0]-curr_par_degree);
			// obj_funct+=gamma*abs(reconf_vector[0]-curr_par_degree); //abs
			//For the step greater than one we will use the same c_arr and c_serv
			//but different forecasted values for the arrival rate
			for(int j=0;j<h;j++)
			{
				//NOTE WE LOWER THE INTERARRIVAL TIME
				//compute the response time expected using the kingman formula and the
				//kingman scaling factor. All it is computed on a msec basis
				rho=(tcalc/(double)reconf_vector[j])/(MAX_RHO_MODULE/forecasted[j]); //forecast is the rate not the ta 
				if(rho<1)
				{

					// kingman_prev=tcalc+((rho/(1-rho))*((c_arr*c_arr+c_serv*c_serv)/2)*(tcalc/reconf_vector[j]));
					//the expected waiting time according to kingman
					kingman_prev=((rho/(1-rho))*((c_arr*c_arr+c_serv*c_serv)/2)*(tcalc/reconf_vector[j]));
					// resp_time=kingman_prev*ksf;
					//then we obtain the expected response time as the scaled waiting time + the tcalc
					resp_time=tcalc+kingman_prev*ksf;
					
				}
				else
				{
                    resp_time=INT_MAX; //bottleneck, in theory we can cut here (problema: bisogna farlo andare al massimo)
					obj_funct+=resp_time;
				}
				
				//The objective function has a term in which we take into account the expected response time
				//If it is below the threshold we add zero (since we are trying to minimize it), otherwise
				//we will add the difference between the expected response time and the threshold
				/*if(resp_time<(rt_threshold))
					obj_funct+=0;
				else
					obj_funct+=alpha*(resp_time-rt_threshold);*/
				
				//Nuova versione L
				//obj_funct+=alpha*MIN(resp_time,rt_threshold);
				//if(resp_time>(rt_threshold))
				//	obj_funct+=alpha*2*(resp_time-rt_threshold);
				
				//versione L in cui consideriamo la differenza percentuale rispetto al threshold. Se positiva la moltipl. per due
				// double diff=resp_time/rt_threshold;
				// if(diff<1)
				// 	obj_funct+=alpha*(1-diff);
				// else
				// 	obj_funct+=2*alpha*(diff-1);

                //Exp version
                //obj_funct+=alpha*pow(2.0,resp_time/rt_threshold);
                double ratio=((double)resp_time)/rt_threshold;
//                printf("%d %f %f %f\n",reconf_vector[j],ratio,resp_time,rt_threshold);
                if(ratio>100)
                    obj_funct+=INT_MAX;
                else
                    obj_funct+=alpha*exp(ratio);

				// obj_funct+=alpha*resp_time/rt_threshold;
				// obj_funct+=alpha*MAX(resp_time,rt_threshold);

				//beta factor of the objective function

				obj_funct+=beta*reconf_vector[j];
				//gamma factor
				if(j>0)
					obj_funct+=gamma*(reconf_vector[j]-reconf_vector[j-1])*(reconf_vector[j]-reconf_vector[j-1]);
				else
				{
					//save the values of the first horizon step: they will be useful for computing the next ksf
					rt_first=resp_time;
					king_first=kingman_prev;
				}

//                 printf("%d  exp rt: %f objfunct: %f (rho: %f ksf: %f)\n", reconf_vector[j], resp_time, obj_funct,rho,ksf);

			}

                if(obj_funct<=*min)
				{

					//printf("New minima: ");
					for(int j=0;j<h;j++)
					{
						//save the values
						result[j]=reconf_vector[j];
						//printf("%d ",reconf_vector[j]);
					}
					//printf(" Obj function value: %f\n",obj_funct);
					*min=obj_funct;
					//save the expect response time and the kingman value for the first step
					*exp_rt=rt_first;
					*kingman=king_first;
				}
		}
	}
}


/**
  TPDS functions
  - SPL in the paper
  */


typedef enum load_t{
	Unknown=0, //in order to be the default value
	MoreLoad=1,
	LessLoad=2
}load_t;

//utility function for computing the number of channel at a given level L
inline double n_at_level(int l)
{
	//return floor(0.5+pow(2,0.5*(l+1)));
	return l+1;
}

/**
	Detect Load Changes using the Congestion
    @param sd Strategy descriptor
	@param p current adaptation step
	@param l current level
	@param cong current congestion
	@param p_i array containing the info about levels
	@param c_i array containing the info about congestions
	@return LessLoad,  MoreLoad or Unknown
*/
load_t checkLoadChangeViaCongestion(StrategyDescriptor *sd,int p, int l, bool cong,int *p_i, bool *c_i)
{
	if(p_i[l]==p-1 && c_i[l]!=cong ) //current level and the last level are the same and congestion has changed (I'm at the level l, and the last time that I was at level l was the last step)
		return cong? MoreLoad:LessLoad;

    if(l<sd->max_level-1 && p_i[l+1]==p-1 && c_i[l+1] && !cong) //if the current level is lower than the last one and the congestion disappeared
		return LessLoad;
	if(l>0 && p_i[l-1]==p-1 && !c_i[l-1] && cong) //if the level is higher than the last one and congestion has appearred
		return MoreLoad;
    //unknown otherwise
	return Unknown;
}

/**
	Detect load changes using the trhoughput
    @param sd StrategyDescriptor
	@param p current adaptation step
	@param l current level
	@param thr current throughput
	@param p_i array containing the info about levels
	@param thr_last_i array containing info on the last throughput per level
	@param thr_first_i array containing info on the througphut of the first step of a stationary phase
	@param s significance level


	Note: probably there are some missing boundary condition on l
*/
load_t checkLoadChangeViaThroughput(StrategyDescriptor *sd,int p, int l, int thr, int *p_i, int *thr_last_i, int * thr_first_i, double s)
{
	if (p_i[l]==p-1) //current and the last level are the same
	{
		if(thr<thr_first_i[l]) // load decrease? check with significance level
		{
			if((thr_first_i[l]-thr)> s*(n_at_level(l)-n_at_level(l-1))*((double)thr_first_i[l]/n_at_level(l)))
			{
				// printf("Thr act: %d Thr first: %d Diff threshold: %f\n",thr,thr_first_i[l], s*(n_at_level(l)-n_at_level(l-1))*((double)thr_first_i[l]/n_at_level(l)));
				return LessLoad;
			}
		}
		else
		{
			if((thr-thr_first_i[l])>s*(n_at_level(l+1)-n_at_level(l))*((double)thr_first_i[l]/n_at_level(l)))
				return MoreLoad;
		}
	}
    if(l<sd->max_level-1 && p_i[l+1]==p-1 && thr>thr_last_i[l+1]) // the current level is lowe than the previous one and thereis a change in thr
		return MoreLoad;
	if(l>0 && p_i[l-1]==p-1 && thr < thr_last_i[l-1])//the current level is higher than the previous one
	{ //io la metterei qui una soglia
		// printf("Thr act: %d Thr last: %d Hyp threshold: %f \n",thr,thr_last_i[l-1],s*(n_at_level(l)-n_at_level(l-1))*((double)thr_last_i[l]/n_at_level(l)) ); //troppo stringente...se pesiamo con s?
		return LessLoad;
	}
	return Unknown;
}
/**
	Implements the strategy proposed in the article "Elastic scaling for data stream processing"
	This was referred in the article as getNumberOfChannel
    @param sd StrategyDesctipro containing the various parameter
	@param p current adaptation step
	@param l the pointer to the current level (it will be changed)
	@param thr current throughput
	@param cong current congestion
		@param p_i array containing the info about levels
	@param c_i array containing the info about congestions
	@param thr_last_i array containing info on the last throughput per level
	@param thr_first_i array containing info on the througphut of the first step of a stationary phase
	@param s significance level

	@return the number of workers that has to be used
*/

int predict_tpds(StrategyDescriptor *sd,int p, int *l, int thr, bool cong, int *p_i, bool *c_i,int *thr_last_i, int * thr_first_i, double s )
{
	int curr_level=*l;
	/* (P3) and (P4): congestion and throughput adapt.
	If there is a change in the load, the information kept about the level 
	(above or below depending on the chane) are reset
	*/
    load_t load_c=checkLoadChangeViaCongestion(sd,p,curr_level,cong,p_i,c_i);
    load_t load_thr=checkLoadChangeViaThroughput(sd,p,curr_level,thr,p_i,thr_last_i,thr_first_i,s);

	if(load_c==LessLoad || load_thr==LessLoad)
	{
//		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>Less load: %d %d\n",load_c,load_thr);
		//forget the info about levels below
		for(int i=0;i<curr_level;i++)
		{
			c_i[i]=false;
			thr_last_i[i]=0;
		}
	}

	if(load_c==MoreLoad || load_thr==MoreLoad)
	{
//		printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<More load: %d %d\n",load_c,load_thr);
		//forget the info about level above the current one
        for(int i=curr_level+1;i<sd->max_level-1;i++) //or max level?
		{
			c_i[i]=true;
			thr_last_i[i]=INT_MAX;
		}
	}

	//update info on current level
	p_i[curr_level]=p; //monitoring step is update outside this function
	thr_last_i[curr_level]=thr;
	c_i[curr_level]=cong;

	if(thr_first_i[curr_level]==-1) //nan
		thr_first_i[curr_level]=thr;//save the value

	bool r=(curr_level>0 && p_i[curr_level-1]==p_i[curr_level]-1) && c_i[curr_level-1] && c_i[curr_level] && thr_last_i[curr_level]<=thr_last_i[curr_level-1];
	if(r) //(P5) remote congestion
	{
//		printf("!!!!!!!!!Remote congestion !!!!!!!!!\n");
		thr_first_i[curr_level-1]=-1;
		*l=curr_level-1;
	}
	else
	{
		if(cong) //(P1) expand
		{
            if(curr_level<sd->max_level && thr_last_i[curr_level+1]>=thr) //I think that in the paper there is an error here. Curr_level has to be minor than max_level (which is max_workers-1) in order to scale up
			{
				thr_first_i[curr_level+1]=-1;
				*l=curr_level+1;
			}
		}
		else //(P2) contract
		{
			if(curr_level>0 && !c_i[curr_level-1])
			{
				thr_first_i[curr_level-1]=-1;
				*l=curr_level-1;
			}
		}
	}
	return n_at_level(*l); //(P6) rapid scaling

}



#endif
