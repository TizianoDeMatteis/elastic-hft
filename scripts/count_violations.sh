#!/bin/bash

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
# Count the number of violations taking in input a directory containing the stats files
# e.g. generated with conf_interval_real or conf_interval_synthetic scripts

if [ -z "$1" ]; then
	echo "Specify the directory containing the stat files as first command line argument"
	exit 1
fi

if [ -z "$2" ]; then
	echo "Specify the latency threshold (in usecs) as second command line argument"
	exit 1
fi

if [ ! -d "$1" ]; then
  echo "The directory $1 does not exists!"
  exit 1
fi


for stat_file in $1/stat*dat
do
	tail -n +2 $stat_file | head -n -2 | cut -f 3 | awk -v thr=$2 'BEGIN {count=0} {if($1>thr){count++}} END{print count;}' >> tmp_violations
done


cat tmp_violations | awk 'BEGIN{sum=0; count=0;} {sum+=$1; count++;} END {print "Average #violations: "sum/count;}'
rm tmp_violations;
