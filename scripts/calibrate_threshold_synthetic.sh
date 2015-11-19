#!/bin/bash
#Calibration Script for the threshold value. It assumes that the files are located like in the repository


#fill the conf file
CONFIG_FILE="strategy=latency\nalpha=2\nbeta= 0.5\ngamma=0.4\nhorizon=1\ncontrol_step=1000"
THRESHOLD=1.5  #starting value
DECREASE=0.9 #at each step we decrease of the 10%
INCREASE=1.1 #increase of the 10%
NUM_IT=10
#the number of violation with a given threshold must be in the interval
# [VIOL_LOW_THRESHOLD,VIOL_UP_THRESHOLD] otherwise the threshold is adjusted and the program runned again
VIOL_LOW_THRESHOLD=4
VIOL_UP_THRESHOLD=7
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
    ./elastic-hft 2836 4 8080 1000 25 tmp_config &
    PID=$!
    sleep 1;

	#run the generator
	./synthetic-generator localhost 8080 2836 distr_and_rates/probability_distribution 300000 54000000 distr_and_rates/random_walk_rates
	wait $PID	# we have to wait for the termination of the program


    #now take the number of violation

    VIOL=$(grep Violations stats.dat | cut -f 2 -d ":" | xargs)
    echo "Founded $VIOL violations"
    echo $TRESHOLD $VIOL > calibration

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
