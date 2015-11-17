//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "../includes/HoltWinters.h"

//Forecast the next value
double HWFilter::forecast() {
    lastS = forecast_mean(); //Get smooth component.
    lastT = forecast_trend(); //Get trend component.
    predicted = lastS + lastT;
    //Parameters update:
    lastPrediction = predicted;
    lastlastS = lastS;
    lastS = lastPrediction;
    lastlastT = lastT;
    return predicted;
}

//Forecast h values beyond t
//and put the result in result
void HWFilter::forecast(double * result, int h ) {
    lastS = forecast_mean(); //Get smooth component.
    lastT = forecast_trend(); //Get trend component.
    for(int i = 1; i <= h; i++) result[i - 1] = lastS + ( i * lastT );
    //Parameters update:
    lastPrediction = result[0];
    lastlastS = lastS;
    lastS = lastPrediction;
    lastlastT = lastT;
}
// Forecast s(t)
double HWFilter::forecast_mean() {
    double value = (alfa * lastObs) + ((1 - alfa) * lastPrediction);
    return value;
}
//forecast b(t)
double HWFilter::forecast_trend() {
    double value = (beta * (lastS - lastlastS)) + (( 1 - beta) * lastlastT );
    return value;
}

void HWFilter::reset_trend() {
	lastT = lastlastT = 0;
}
