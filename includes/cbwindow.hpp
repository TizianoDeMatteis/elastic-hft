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

	Type and functions definitions for count based window
	In this case it is specifically tailored for receiving tasks corresponding to quotes for saving their values

	Author: Tiziano De Matteis <dematteis <at> di.unipi.it>

*/

#ifndef _CB_WINDOW_H
#define _CB_WINDOW_H
#include <math.h>
#include <stdio.h>
#include "general.h" 
#include "lmcurve.h"
#include "window.h"

/**
 * The CBWindow class represent a count based window of elements Tuple.
 * It extends the abstract class Window.
 *
 * It is characterized by a window_size and a window_slide. The computation
 * may be triggered after the insertion of window_slide elements and regards the last
 * window_size elements.
 *
 */
class CBWindow : public Window<tuple_t,winresult_t>{
	
public:

    /**
     * Constructor of the Count Based Window
     * @param window_size
     * @param window_slide
     */
	CBWindow(int window_size, int window_slide)
	{
		this->window_size=window_size;
		this->window_slide=window_slide;
        elements=new tuple_t[window_size];


        x_bid=new double[window_size];
        x_ask=new double[window_size];
        y_bid=new double[window_size];
        y_ask=new double[window_size];
		ins_pointer=0; // DA RIPULIRE
		eflc=0;
        total_elements=0;

		
	}

	~CBWindow()
	{
        delete[] elements;
        delete[] x_bid;
        delete[] x_ask;
        delete[] y_bid;
        delete[] y_ask;

	}

	int getSize()
	{
		return window_size;
	}
	int64_t getTotalElements()
	{
		return total_elements;
	}



    /**
     * Insert the tuple passed as argument into the window (by copying it)
     * @param t tuple to insert
     */
    void insert(const tuple_t& t)
	{


        elements[ins_pointer]=t;
        ins_pointer++;
		if(ins_pointer==window_size) //Reset the insertion pointer
			ins_pointer=0;
		eflc++;
		total_elements++;
      //  std::cout<< "Inserted element with id: "<<t.id<<std::endl;
     }


    /**
      * isComputable return a boolean indicating if the computation can be triggered
      * i.e. window_slide tuples have been inserted since the last computation
      * @return true if the window content is valid for computation, false otherwise
      */
     bool isComputable()
     {
        //assuming that window size is a multiplier of window slide
        return (eflc==window_slide);
     }

    /**
     * Compute on the last window_size received elements
     * @param res the reference in which save the result
     */
    void compute(winresult_t &res)
    {
        if(eflc!=window_slide) //we compute only if the window is computable
            return;

        lm_control_struct control = lm_control_double;
        control.verbosity = 0;

        eflc=0;
        //Now we have to compute considering the last window_size element received
        //that is starting from ins_pointer (and threating the elements array as a circular buffer)
        //For the interpolation part we have to consider the element from zero up to the actual insertion pointer.
        //We will perform an interpolation for the bid quotes and one for the ask quotes (in many cases a quote have both terms).
        //Therefore we buil the x and y vectors for the two cases. The elements for x (that is time) have to start from zero.
        //(in a real context they should be tranformed into seconds, but since we will throttle the input stream is not necessary)


        //TODO: improve the efficiency of this method
        //Since the market precision is at the millisecond level, it may happen that more than one trade
        //occur with the same timestamp. In this case, for the fitting phase, it is kept a single point whose
        //y-cord correspond to the average price value

        int npoints=0;
        int j=0;
        int start_idx, end_idx;
        if(total_elements>=window_size) //there is a full window
        {
            start_idx=ins_pointer;
            end_idx=ins_pointer+window_size;
        }
        else //we have only a partial window (we have to start from zero)
        {
            start_idx=0;
            end_idx=ins_pointer;

        }
        //if(elements[0].type==212)
        //	printf("Partiamo da %d per un numero di punti pari a: %d\n",start_idx,end_idx-start_idx);

        //Building the vectors for bid quotes
        for(int i=start_idx;i<end_idx;)
        {
            if(elements[i%window_size].bid_size>0)
            {
                x_bid[npoints]=(elements[i%window_size].original_timestamp-elements[start_idx].original_timestamp); //ins_pointer is the older element
                y_bid[npoints]=elements[i%window_size].bid_price;
                j=(i+1);

                //check if subsequent point have the same x-value
                while(j<end_idx && (elements[i%window_size].original_timestamp)==(elements[j%window_size].original_timestamp)  && elements[j%window_size].bid_size>0)
                {
                    y_bid[npoints]+=elements[j%window_size].bid_price;
                    j++;
                }
                y_bid[npoints]/=(j-i);

                //go ahead
                i=j; //j point to the next value with different timestamp
                npoints++; //next point to be derived
            }
            else //go to the next one
                i++;



        }
        //now we can perform the fitting

        lm_status_struct status;
        //just a guess
        if(par_bid[0]==0)
            par_bid[0]=y_bid[0];
//        printf("Lmcurve: eseguo con %d punti\n",npoints);
        lmcurve(3, par_bid, npoints, x_bid, y_bid, parabola, &control, &status );
        res.p0_bid=par_bid[0];
        res.p1_bid=par_bid[1];
        res.p2_bid=par_bid[2];
        //Building the vectors for ask quotes
        npoints=0;
        for(int i=start_idx;i<end_idx;)
        {
            if(elements[i%window_size].ask_size>0)
            {
                x_ask[npoints]=(elements[i%window_size].original_timestamp-elements[start_idx].original_timestamp); //ins_pointer is the older element
                y_ask[npoints]=elements[i%window_size].ask_price;
                j=(i+1);

                //check if subsequent point have the same x-value
                while( j<end_idx && (elements[i%window_size].original_timestamp)==(elements[j%window_size].original_timestamp) && elements[j%window_size].ask_size>0)
                {
                    y_ask[npoints]+=elements[j%window_size].ask_price;
                    j++;
                }
                y_ask[npoints]/=(j-i);

                //go ahead
                i=j; //j point to the next value with different timestamp
                npoints++; //next point to be derived
            }
            else //go to the next one
                i++;



        }
        //now we can perform the fitting
        //printf("Lmcurve: eseguo con %d punti\n",npoints);
        //just a guess
        if(par_ask[0]==0)
            par_ask[0]=y_ask[0];
        lmcurve(3, par_ask, npoints, x_ask, y_ask, parabola, &control, &status );
        res.p0_ask=par_ask[0];
        res.p1_ask=par_ask[1];
        res.p2_ask=par_ask[2];
        //printf("Scattato slide per classe: %d, tot elementi ricevuti: %Ld Elementi distinti:%d\n",t->type,total_elements,npoints);
        /*printf("X: ");
        for(int i=0;i<ins_pointer;i++)
            printf("%Ld ",elements[i].timestamp);
        printf("\n");
        */
//     printf("\nParametri: %g %g %g\n",par_ask[0],par_ask[1],par_ask[2]);
        /*if(elements[0].type==3)
        {
            printf("X: (start timestamp: %d) for stock: %d\n",elements[0].original_timestamp,elements[0].type);
            for(int i=0;i<npoints;i++)
                printf("%f %f\n",x_ask[i],y_ask[i]);
            printf("\nParametri: %g %g %g\n",par_ask[0],par_ask[1],par_ask[2]);
        }*/

        //Candle stick: for the candle stick graph we have to consider the last window_slide element received and compute
        //the various values
        if(ins_pointer==0)
        {
            end_idx=window_size;
            start_idx=window_size-window_slide;
        }
        else //there is always the assumption that the window slide is a divisor of window size
        {
            end_idx=ins_pointer;
            start_idx=end_idx-window_slide;
        }
        res.low_bid=10000;
        res.low_ask=10000;
        res.high_bid=0;
        res.high_ask=0;
        res.open_bid=0;
        res.open_ask=0;
        int last_valid_bid=0, last_valid_ask=0;
        for(int i=start_idx;i<end_idx;i++)
        {
            //compute for both ask and bid (taking into account only vaules !=0)
            if(elements[i].bid_size>0)
            {
                if(res.open_bid==0)
                    res.open_bid=elements[i].bid_price;
                if(elements[i].bid_price>res.high_bid)
                    res.high_bid=elements[i].bid_price;
                if(elements[i].bid_price<res.low_bid)
                    res.low_bid=elements[i].bid_price;
                last_valid_bid=i;
            }
            if(elements[i].ask_size>0)
            {
                if(res.open_ask==0)
                    res.open_ask=elements[i].ask_price;
                if(elements[i].ask_price>res.high_ask)
                    res.high_ask=elements[i].ask_price;
                if(elements[i].ask_price<res.low_ask)
                    res.low_ask=elements[i].ask_price;
                last_valid_ask=i;
            }

        }
        res.close_bid=elements[last_valid_bid].bid_price;
        res.close_ask=elements[last_valid_ask].ask_price;

//        if(elements[0].type==3)
//        {
//            printf("Open: %g Close: %g Low: %g High: %g\n",res.open_ask,res.close_ask,res.low_ask,res.high_ask);
//        }

	}

    /**
     * Reset the window content
     */
	void reset()
	{
		ins_pointer=0;
		total_elements=0;
		eflc=0;
	}

    /*
     * Just for coding purposes
     */
	int64_t getLastInserted()
	{
        return elements[(ins_pointer-1)].internal_id;
	}
	int getInsertionPointer()
	{
		return ins_pointer;
	}
	

	/**
		Print the id of all the task contained in the elements array, starting from the oldest one
	*/
	void printAll()
	{
		for(int i=0;i<window_slide;i++)
		{
            std::cout << i <<"-th element is: "<<elements[i].id <<std::endl;
		}
	}



private:
    int window_size;
    int window_slide;
    int64_t total_elements; //total elements that were contained in the window
	double *x_bid, *x_ask;
	double *y_bid, *y_ask;
	//for the candlestick computation
	double open,close, high, low;
	int ins_pointer; //always points to the insertion point in elements
	int eflc; //elements received from the last computation
//	ticks computation_time; //monitoring (if specified) of the computation time
//	ticks computation_time_empty; //monitoring (if specified) of the computation time
	int type;

	int iteration_multiplier;
	//starting parameter for the fitting
	double par_ask[3]={0,0,0};
	double par_bid[3]={0,0,0};
	

};

#endif
