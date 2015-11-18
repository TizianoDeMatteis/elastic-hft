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
#Computes average metrics on the file passed as arguments

grep Violations $1 | cut -f 2 -d ":" | xargs | tr  " " "\n" > tmp_violations
grep reconfiguration $1 | cut -f 2 -d ":" | xargs | tr  " " "\n" > tmp_reconfigurations
grep Watt $1 | cut -f 2 -d ":" | xargs | tr  " " "\n" > tmp_watt
grep amplitude $1 | cut -f 2 -d ":" | xargs | tr  " " "\n" > tmp_amplitude
grep Number $1 | cut -f 2 -d ":" | xargs | tr  " " "\n" > tmp_num_replica

#compute average
cat tmp_violations | awk 'BEGIN{sum=0; count=0;} {sum+=$1; count++;} END {print "Average #violations: "sum/count;}'
cat tmp_reconfigurations | awk 'BEGIN{sum=0; count=0;} {sum+=$1; count++;} END {print "Average #reconfiguration: "sum/count;}'
cat tmp_watt | awk 'BEGIN{sum=0; count=0;} {sum+=$1; count++;} END {print "Average watt: "sum/count;}'
cat tmp_amplitude | awk 'BEGIN{sum=0; count=0;} {sum+=$1; count++;} END {print "Average reconfiguration amplitude: "sum/count;}'
if [ $(wc -c tmp_num_replica| cut -f 1 -d " ") -gt "1" ];then
    cat tmp_num_replica | awk 'BEGIN{sum=0; count=0;} {sum+=$1; count++;} END {print "Average #replicas: "sum/count;}'
fi
rm tmp_*
