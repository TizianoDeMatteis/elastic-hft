#---------------------------------------------------------------------
#
#    Copyright (C) 2015- by Tiziano De Matteis (dematteis <at> di.unipi.it)
#
#    This file is part of elastic-hft.
#
#    elastic-hft is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#    ---------------------------------------------------------------------
#!/bin/bash
#Calibration Script for the threshold value. It requires the REAL_DATASET env variable to be defined

if [ -z "$1" ]; then
    echo "Specify as command line argument the dataset path"
    exit 1
fi

#fill the conf file
CONFIG_FILE="strategy=latency\nalpha=3\nbeta= 0.5\ngamma=0.4\nhorizon=1\ncontrol_step=1000"
THRESHOLD=7  #starting value
DECREASE=0.9 #at each step we decrease of the 10%
INCREASE=1.1 #increase of the 10%
NUM_IT=10
INITIAL_NUM_REPLICAS=4
#the number of violation with a given threshold must be in the interval
# [VIOL_LOW_THRESHOLD,VIOL_UP_THRESHOLD] otherwise the threshold is adjusted and the program runned again
#in this case a little bit more violations are allowed
VIOL_LOW_THRESHOLD=10
VIOL_UP_THRESHOLD=20
echo -e $CONFIG_FILE
echo "threshold=$THRESHOLD"
rm stats.dat
for ((i=0;i<$NUM_IT;i++)); do
    #prepare the configuration file
    rm tmp_config;

    echo -e $CONFIG_FILE > tmp_config
    echo "threshold=$THRESHOLD">> tmp_config
    echo "Trying with threshold set to " $THRESHOLD

    #run program
    ./elastic-hft 2836 $INITIAL_NUM_REPLICAS 8080 1000 25 tmp_config &
    PID=$!
    sleep 1;

	#run the generator
	./real-generator localhost 8080 $1 49544800 -s 100
	wait $PID	# we have to wait for the termination of the program


    #now take the number of violation

    VIOL=$(grep Violations stats.dat | cut -f 2 -d ":" | xargs)
    echo "Founded $VIOL violations"
    echo "$TRESHOLD  $VIOL" > calibration

    if (( VIOL<$VIOL_LOW_THRESHOLD )); then
        #too few violations, lower the threshold
        THRESHOLD=$(echo "scale=2; $THRESHOLD * $DECREASE" | bc -l )

    else
        if (( VIOL > $VIOL_UP_THRESHOLD )); then
        #too many, increase it
        THRESHOLD=$(echo "scale=2; $THRESHOLD * $INCREASE" | bc -l )
        else
            #it's ok
            break
        fi
    fi

done

echo "A good threshold value might be: " $THRESHOLD
