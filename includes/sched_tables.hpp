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
 * Contains various heuristic for computing a scheduling table
 */
//Utility function and definition that will be used in constructing the scheduling table

typedef struct{
        double l;
        int idx;
}pair_t;
static int cmppair(const void *p1, const void *p2)
{
    if (((pair_t *)p1)->l > ((pair_t *)p2)->l)
        return 1;
    if (((pair_t *)p1)->l == ((pair_t *)p2)->l)
        return 0;
    else
        return -1;
}
/**
    Compute an ex-novo fully balanced scheduling table
    some argument are modified since they are helpful
    @param num_workers the number of workers to which tuples have to be routed
    @param num_classes
    @param wtcalc_per_class: it contains for each class the product  class_frequency*tcalc_class, computed using the monitored data
    @param scheduling_table: the past scheduling table that will be modified
*/
void compute_fb_st(int num_workers, int num_classes,double* wtcalc_per_class, char *scheduling_table)
{
    double *load_w=new double[num_workers]();
    pair_t *v=new pair_t[num_classes]();
    //fill the array with the weight for each class
    for(int i=0;i<num_classes;i++)
    {
        v[i].l=wtcalc_per_class[i];
        v[i].idx=i;
    }

    //order it by weight
    qsort(v, num_classes, sizeof(pair_t), cmppair);

    //starting from the heavier one...
    for(int i=num_classes-1;i>=0;i--)
    {
        //assign to the less load worker (ASSUMPTION: NONE OF THEM WILL BE BOTTLENECK)
        double min_w=load_w[0];
        int work_index=0;
        for(int j=0;j<num_workers;j++)
        {
            if(load_w[j]<min_w)
            {
                work_index=j;
                min_w=load_w[j];
            }
        }

        scheduling_table[v[i].idx]=work_index+1;
        load_w[work_index]+=v[i].l; //that is wtcalc_per_class
    }
    /*for(int i=0;i<num_workers;i++)
    {
        printf("%d %.8f\n",i,load_w[i]);
    }*/
}

void compute_st_flux(int num_workers, int prev_workers, int num_classes,double* wtcalc_per_class, char *scheduling_table)
{
    const double imb_thr=1.1; //imbalance of 10%
    pair_t *v=new pair_t[num_classes]();
    //take into account the load of the worker (if we are decreasing we have to consider only the existing ones)
    double *load_w=new double[num_workers]();
    //fill the array with the weight for each class

    for(int i=0;i<num_classes;i++)
    {
        v[i].l=wtcalc_per_class[i];
        v[i].idx=i;
        if(scheduling_table[i]-1<num_workers)
            load_w[scheduling_table[i]-1]+=wtcalc_per_class[i]; //compute the load per worker (only the one that exists)
    }

    //order classes by weight
    qsort(v, num_classes, sizeof(pair_t), cmppair);
    //compute average load per worker, max load and min load
    double avg_load=0;
    double max_load=load_w[0], min_load=load_w[0];
    int max_w=0,min_w=0;
    for(int i=0;i<num_workers;i++)
    {
//        printf("%d %f\n",i,load_w[i]);
        avg_load+=load_w[i];
        if(max_load<load_w[i])
        {
            max_load=load_w[i];
            max_w=i;
        }
        if(min_load>load_w[i])
        {
            min_load=load_w[i];
            min_w=i;
        }

    }
    avg_load/=num_workers;
    //first of all: if we are going to decrease the par degree, move out the classes
    //from workers that have to be destroyed and then (if needed) rebalance the load
    if(prev_workers>num_workers)
    {
        for(int i=num_classes-1;i>=0;i--)
        {
            if(scheduling_table[v[i].idx]-1>=num_workers) //previously assigned to a worker that has to be removed
            {
                //assign to the currently less load worker and update its load
                scheduling_table[v[i].idx]=min_w+1;
                load_w[min_w]+=v[i].l;
                //recompute the minimum
                min_load=load_w[0];
                min_w=0;
                for(int i=0;i<num_workers;i++)
                {
                    if(min_load>load_w[i])
                    {
                        min_load=load_w[i];
                        min_w=i;
                    }

                }
            }
        }
    }
    //now: rebalance using the flux method. To be used if the number of worker
    //has been increased or if we need to rebalance the current load


    //the donor site is the one with highest load, the receiver the one with lowest
    //we continue to pair them if the donor's load is above average and the
    //imbalace between them is over a given threshold (imb_thr)
    //note: the newly created worker has load equal to zero
    while(min_load==0 || (max_load/min_load>imb_thr && max_load>avg_load))
    {
        //now we walk down the sorted list of classes that belong to the donor
        //for choosing one that will be moved to the receiver reducing the imbalance
        //starting from the heavier one...
//        printf("Spostare da %d a %d %f-%f\n",max_w,min_w,max_load,min_load);
        for(int i=num_classes-1;i>=0;i--)
        {
            if(scheduling_table[v[i].idx]-1==max_w) //it is currently assigned tot the donor
            {
                if((v[i].l+min_load)/(max_load-v[i].l)<max_load/min_load) //it could be assigned since it will not worsen the imbalance
                {
                    scheduling_table[v[i].idx]=min_w+1;
                    //recompute load, donor and receiver
                    load_w[min_w]+=v[i].l;
                    load_w[max_w]-=v[i].l;
                }

                max_load=load_w[0], min_load=load_w[0];
                max_w=0,min_w=0;
                for(int i=0;i<num_workers;i++)
                {

                    if(max_load<load_w[i])
                    {
                        max_load=load_w[i];
                        max_w=i;
                    }
                    if(min_load>load_w[i])
                    {
                        min_load=load_w[i];
                        min_w=i;
                    }

                }

                break;
            }

        }
    }

//    for(int i=0;i<num_workers;i++)
//        {
//            printf("%d %.8f\n",i,load_w[i]);
//        }


}
