#
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

#This script take all the configuration file in a given directory and for all of them
#execute the program multiple times, saving statistics and computing the conf interval for
#latencies and energy consumption

#/bin/bash
#CONDIFENCE INTERVAL AT 95


#directory that contains all the configuration file
CONFIG_DIR=configs
#directory that will contains the results
RES_DIR=results
#checks
if [ ! -d "$CONFIG_DIR" ]; then
  echo "The directory $CONFIG_DIR does not exists! Create it and put the desired configuration files or edit the script"
  exit 1
fi

if [  -d "$RES_DIR" ]; then
  echo "The directory $RES_DIR exists! Results could be overwritten"
fi

if [ -z "$1" ]; then
    echo "Specify as command line argument the dataset path"
    exit 1
fi

#the path of the real dataset if used
#number of replicas at startup. Please, it should be equal to the number of replicas
#used in the calibration scripts (by default 4)
INITIAL_NUM_REPLICAS=4
NUM_RUN=5			#number of execution per config
rm *.dat

for conf_file in $CONFIG_DIR/*
do
	#create a directory for the results
	basen=$(basename $conf_file);
	dirname=${basen%.*}
	mkdir -p $RES_DIR/$dirname
	touch all_latencies.dat
	touch all_joules_core.dat

	OK_RUN=0;
	for ((i=0;i<NUM_RUN;i++));
	do

        #run the program: adjust the following row to meet your needs
        ./elastic-hft 2836 $INITIAL_NUM_REPLICAS 8080 1000 25 $conf_file &
        PID=$!
        sleep 1;

		#run the generator: please comment/uncomment and adjust the following row according to your needs
		echo "REAL GENERATOR"
		./real-generator localhost 8080 $1 49544800 -s 100
		#echo "SYNTHETIC GENERATOR"
		#./synthetic-generator localhost 8080 2836 distr_and_rates/probability_distribution 300000 54000000 distr_and_rates/random_walk_rates

		wait $PID	# we have to wait for the termination of the program



		#take rows that contains values, and paste all the data in a file
		#each column of the file will contain the data of a certain run

		grep "^[^#;]" stats.dat | cut -f 3 | paste - all_latencies.dat > tmp.dat
		mv tmp.dat all_latencies.dat

		#save joules core (==watt if print rate is on a second basis)
		grep "^[^#;]" stats.dat | cut -f 7 | paste - all_joules_core.dat > tmp.dat
		mv tmp.dat all_joules_core.dat

		# save reconf data
		echo "Run: $i" >> metrics.dat
		tail -n +2 stats.dat | grep "#" >> metrics.dat

		#save stats
		tag=$[NUM_RUN-i]
		cp stats.dat stats_$tag.dat


		OK_RUN=$[OK_RUN+1]
	done
	#compute all the info row by row
	echo -e "#TIME\tAVG\tMIN\tMAX\tCONF_INT" > conf_interval_latency.dat
	cat all_latencies.dat | awk '{
		A=0; V=0;
		for(N=1; N<=NF; N++)
			 A+=$N ;
		A/=NF ;
		for(N=1; N<=NF; N++)
			V+=(($N-A)*($N-A));
		V/=NF;
		# A contains the average, V the std. dev
		STDERR=sqrt(V)/sqrt(NF);
		# the confidence interval is given by zed*STDERR
		print A,"\t"A-1.96*STDERR,"\t"A+1.96*STDERR,"\t"1.96*STDERR;
	}'> tmp.dat
	#attach times
	grep "^[^#;]" stats.dat | cut -f 1| paste - tmp.dat >> conf_interval_latency.dat
	rm tmp.dat


	#compute joules
	echo -e "#TIME\tAVG\tMIN\tMAX\tCONF_INT" > conf_interval_watt.dat
	cat all_joules_core.dat | awk '{
			A=0; V=0;
			for(N=1; N<=NF; N++)
					 A+=$N ;
			A/=NF ;
			for(N=1; N<=NF; N++)
					V+=(($N-A)*($N-A));
			V/=NF;
			# A contains the average, V the std. dev
			STDERR=sqrt(V)/sqrt(NF);
			# the confidence interval is given by zed*STDERR
			print A,"\t"A-1.96*STDERR,"\t"A+1.96*STDERR,"\t"1.96*STDERR;
	}'> tmp.dat
	#attach times
	grep "^[^#;]" stats.dat | cut -f 1| paste - tmp.dat >> conf_interval_watt.dat
	rm tmp.dat
	rm all*

    SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
    bash $SCRIPT_DIR/compute_avg_metrics.sh metrics.dat > avg_metrics.dat

	#save all .dat
	rm stats.dat
	mv *dat $RES_DIR/$dirname/
done


