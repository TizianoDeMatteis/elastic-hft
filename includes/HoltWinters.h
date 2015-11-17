/*---------------------------------------------------------------------

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

#ifndef HOLTWINTERS_H_
#define HOLTWINTERS_H_

#include <iostream>

class HWFilter {
  public:
    double lastPrediction;
    double initialValue;
    double lastObs;
    double lastS;
    double lastlastS;
    double lastT;
    double lastlastT;
    double predicted;
    double count;
    double alfa;
    double beta;

    HWFilter() {
        count = 0;
        lastPrediction = 0;
        lastObs = 0;
        lastS= 0;
        predicted = 0;
        initialValue = 0;
    }

    HWFilter(double alfa) {
        count = 0;
        lastPrediction = 0;
        lastObs = 0;
        lastS = 0;
        lastlastS = 0;
        lastT = 0;
        lastlastT = 0;
        predicted = 0;
        initialValue = 0;
    }

    HWFilter(int _initial, double alfa) {
        count = 0;
        lastPrediction = 0;
        lastObs = 0;
        lastlastS = 0;
        lastS = 0;
        lastT = 0;
        lastlastT = 0;
        predicted = 0;
        initialValue = _initial;
    }

    void initialize( double a, double b, double lo, double llt) {
        alfa = a;
        beta = b;
        lastObs = lo;
        lastPrediction = lo;
        lastlastT = llt;
        lastlastS = lo;
    }

    void updateSample(double value) {
        lastObs = value;
    }

    double forecast();
    void forecast(double * input, int h);
    double forecast_mean();
    double forecast_trend();
    void reset_trend();
};

#endif /* HOLTWINTERS_H_ */
