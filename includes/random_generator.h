/****************************************************************************
 *
 *  All the rights are due to the proper author
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  As a special exception, you may use this file as part of a free software
 *  library without restriction.  Specifically, if other files instantiate
 *  templates or use macros or inline functions from this file, or you compile
 *  this file and link it with other files to produce an executable, this
 *  file does not by itself cause the resulting executable to be covered by
 *  the GNU General Public License.  This exception does not however
 *  invalidate any other reasons why the executable file might be covered by
 *  the GNU General Public License.
 *
 ****************************************************************************
 */

#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
using namespace std;

//Exception class thrown by some RandomGenerator methods:
class RandomGeneratorError { 
public:
	RandomGeneratorError(string _msg) {
		msg = "RandomGenerator Error: ";
		if (_msg.length() < 1) msg += "Undefined error.";
		else msg += _msg;
	}
	string msg;
};

class RandomGenerator {
public:
	RandomGenerator(long _seed=0) {
		if (_seed == 0) seed = (long)time(0);
		else seed = _seed;
	}

	//UNIFORM (0.0, 1.0) RANDOM REAL NUMBER GENERATOR
	//returns a psuedo-random variate from a uniform    
	//distribution in range (0.0, 1.0):
	double randf(void) {
		const long  a =      16807;  // Multiplier
		const long  m = 2147483647;  // Modulus
		const long  q =     127773;  // m div a
		const long  r =       2836;  // m mod a
		long        x_div_q;         // x divided by q
		long        x_mod_q;         // x modulo q
		long        x_new;           // New x value
		//RNG using integer arithmetic:
		x_div_q = seed / q;
		x_mod_q = seed % q;
		x_new = (a * x_mod_q) - (r * x_div_q);
		if (x_new > 0)
			seed = x_new;
		else
			seed = x_new + m;
		//Return a random value between 0.0 and 1.0:
		return ((double)seed/m);
	}

	//UNIFORM [a, b] RANDOM VARIATE GENERATOR
	//returns a psuedo-random variate from a uniform
	//distribution with lower bound a and upper bound b:
	double uniform(double a, double b) {         
		double res;
		do {
			if (a>b) throw RandomGeneratorError("uniform Argument Error: a > b"); 
			res = (a+(b-a)*randf());
		}
		while(res<=0);
		return res;
	}

	//RANDOM INTEGER GENERATOR
	//returns an integer equiprobably selected from the 
	//set of integers i, i+1, i+2, . . , n:                     
	int random(int i, int n) { 
		if (i>n) throw RandomGeneratorError("random Argument Error: i > n"); 	
		n-=i; 
		n=int((n+1.0)*randf());
		return(i+n);
	}

	//EXPONENTIAL RANDOM VARIATE GENERATOR
	//returns a psuedo-random variate from a negative     
	//exponential distribution with mean x:
	double expntl(double x) {  
		double res;      
		do {
			res = -x*log(randf());
		}           
		while(res <= 0);
		return res;
		//return(-x*log(randf()));
	}

	//ERLANG RANDOM VARIATE GENERATOR
	//returns a psuedo-random variate from an erlang 
	//distribution with mean x and standard deviation s:
	double erlang(double x, double s) { 
		if (s>x) throw RandomGeneratorError("erlang Argument Error: s > x"); 	
		int i,k; double z;
		z=x/s; 
		k=int(z*z);
		z=1.0; for (i=0; i<k; i++) z *= randf();
		return(-(x/k)*log(z));
	}

	//HYPEREXPONENTIAL RANDOM VARIATE GENERATOR
	//returns a psuedo-random variate from Morse's two-stage 
	//hyperexponential distribution with mean x and standard 
	//deviation s, s>x: 
	double hyperx(double x, double s) { 
		if (s<=x) throw RandomGeneratorError("hyperx Argument Error: s not > x"); 	
		double cv,z,p; 
		cv=s/x; z=cv*cv; p=0.5*(1.0-sqrt((z-1.0)/(z+1.0)));
		z=(randf()>p)? (x/(1.0-p)):(x/p);
		return(-0.5*z*log(randf()));
	}

	//TRIANGULAR RANDOM VARIATE GENERATOR
	//returns a psuedo-random variate from a triangular distribution 
	//with left and right being [a,b] and the mode being at point c:
	double triang(double a, double c, double b) {
		double sample,point;
		point = (c-a)/(b-a);
		sample = uniform(0.0,1.0);
		if (sample <= point)
			return(sqrt(sample*(b-a)*(c-a)) + a);
		else 
			return(b - sqrt((1.0-sample)*(b-a)*(b-c)));
	}

	//NORMAL RANDOM VARIATE GENERATOR
	//returns a psuedo-random variate from a normal distribution 
	//with mean x and standard deviation s:
	double normal(double x, double s) {     
		double v1,v2,w,z1;
		double res;
		do {
			static double z2=0.0;
			if (z2!=0.0) {
				z1=z2; //Use value from previous call.
				z2=0.0;
			}  
			else {
				do {
					v1 = 2.0*randf()-1.0; 
					v2 = 2.0*randf()-1.0; 
					w = v1*v1+v2*v2;
				}
				while (w>=1.0);
				w = sqrt((-2.0*log(w))/w);
				z1 = v1*w;
				z2 = v2*w;
			}
			res = x+z1*s;
		} 
		while(res<=0);
		return res;
	}

private:
	long seed;
};
