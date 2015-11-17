/*
 * 	affinities.h
 *
 *  Created by Daniele Buono
 *	Edited by Tiziano De Matteis
 */

#ifndef AFFINITIES_H_
#define AFFINITIES_H_

//Allocation arrays
#ifdef PARACOOL
	#ifdef SMT
	int affinities[]={	0 , 32, 1 , 33, 2 , 34, 3 , 35, 4 , 36,
						5 , 37, 6 , 38, 7 , 39, 8 , 40, 9 , 41,
						10, 42, 11, 43, 12, 44, 13, 45, 14, 46,
						15, 47, 16, 48, 17, 49, 18, 50, 19, 51,
						20,	52, 21, 53, 22, 54, 23, 55, 24, 56,
						25, 57, 26, 58, 27, 59, 28, 60, 29, 61,
						30, 62 };
	int emitter_affinity=31;
	int collector_affinity=63;
	#else
	int affinities[]={ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
			21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
	int emitter_affinity=62;
	int collector_affinity=63;
	#endif
#endif
#ifdef REPARA
    const float FREQ=2400;
    const int max_workers=20;
     int affinities[]={ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,14,15,16,17,18,19,20,21 };
    //int affinities[]={ 2, 8, 3, 9, 4, 10, 5, 11, 6, 12, 7,13,18,24,19,25,20,26,21,27,22,28,23,29};
    int emitter_affinity=1;
    int collector_affinity=23;
    int generator_affinity=0;
    int controller_affinity=22;

#endif
#ifdef PIANOSA
	const int FREQ=2000;
    const int max_workers=12;
	#ifdef SMT
	int affinities[]={ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
	int emitter_affinity=30;
	int collector_affinity=31;
	#else
	// int affinities[]={ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,14 };
    int affinities[]={ 2, 8, 3, 9, 4, 10, 5, 11, 6, 12, 7,13,18,24,19,25,20,26,21,27,22,28,23,29};
	int emitter_affinity=0;
	int collector_affinity=15;
	int generator_affinity=1;
	int controller_affinity=14;
	int generator_affinity_smt=17;
	
	#endif
#endif

#ifdef PHIASK
    //const float FREQ=2294.045;
    const float FREQ=2300;
    const int max_workers=8;
    int affinities[]={ 2, 3, 4, 5, 6, 7, 8, 9};

    int generator_affinity=0;
    int emitter_affinity=1;
    int collector_affinity=10;
    int controller_affinity=11;



#endif


#ifdef MIC
	const int FREQ=1060;
	const int FREQ_HOST=2300;
	#ifdef SMT
	int affinities[]={	1  , 2  , 3  , 4  , 5  , 6  , 7  , 8  , 9  , 10 ,
						11 , 12 , 13 , 14 , 15 , 16 , 17 , 18 , 19 , 20 ,
						21 , 22 , 23 , 24 , 25 , 26 , 27 , 28 , 29 , 30 ,
						31 , 32 , 33 , 34 , 35 , 36 , 37 , 38 , 39 , 40 ,
						41 , 42 , 43 , 44 , 45 , 46 , 47 , 48 , 49 , 50 ,
						51 , 52 , 53 , 54 , 55 , 56 , 57 , 58 , 59 , 60 ,
						61 , 62 , 63 , 64 , 65 , 66 , 67 , 68 , 69 , 70 ,
						71 , 72 , 73 , 74 , 75 , 76 , 77 , 78 , 79 , 80 ,
						81 , 82 , 83 , 84 , 85 , 86 , 87 , 88 , 89 , 90 ,
						91 , 92 , 93 , 94 , 95 , 96 , 97 , 98 , 99 , 100,
						101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
						111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
						121, 122, 123, 124, 125, 126, 127, 128, 129, 130,
						131, 132, 133, 134, 135, 136, 137, 138, 139, 140,
						141, 142, 143, 144, 145, 146, 147, 148, 149, 150,
						151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
						161, 162, 163, 164, 165, 166, 167, 168, 169, 170,
						171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
						181, 182, 183, 184, 185, 186, 187, 188, 189, 190,
						191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
						201, 202, 203, 204, 205, 206, 207, 208, 209, 210,
						211, 212, 213, 214, 215, 216, 217, 218, 219, 220,
						221, 222, 223, 224, 225, 226, 227, 228, 229, 230,
						231, 232, 233, 234, 235, 236, 237, 238, 239, 0		};
	int emitter_affinity=239;
	int collector_affinity=0;
	#else
	 int affinities[]={	9  , 13 , 17 , 21 , 25 , 29 , 33 , 37 ,
	 					41 , 45 , 49 , 53 , 57 , 61 , 65 , 69 , 73 , 77 ,
	 					81 , 85 , 89 , 93 , 97 , 101, 105, 109, 113, 117,
	 					121, 125, 129, 133, 137, 141, 145, 149, 153, 157,
	 					161, 165, 169, 173, 177, 181, 185, 189, 193, 197,
	 					201, 205, 209, 213, 217, 221, 225, 229, 233, 237};

	/*int affinities[]={	1  , 5  , 9  , 13 , 17 , 21 , 25 , 29 , 33 , 37 ,
						41 , 45 , 49 , 53 , 57 , 61 , 65 , 69 , 73 , 77 ,
						81 , 85 , 89 , 93 , 97 , 101, 105, 109, 113, 117,
						121, 125, 129, 133, 137, 141, 145, 149, 153, 157,
						161, 165, 169, 173, 177, 181, 185, 189, 193, 197,
						201, 205, 209, 213, 217, 221, 225, 229, 233, 237,
						2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,66,70,74,78,82,86,90,94,98,102,106,110,114,118,122,126,130,134,138,142,146,150,154,158,162,166,170,174,178,182,186,190,194,198,202,206,210,214,218,222,226,230,234,238,
						3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63,67,71,75,79,83,87,91,95,99,103,107,111,115,119,123,127,131,135,139,143,147,151,155,159,163,167,171,175,179,183,187,191,195,199,203,207,211,215,219,223,227,231,235,239,
						4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,84,88,92,96,100,104,108,112,116,120,124,128,132,136,140,144,148,152,156,160,164,168,172,176,180,184,188,192,196,200,204,208,212,216,220,224,228,232};*/
	int emitter_affinity=0;
	int collector_affinity=1;
	int controller_affinity=5;
	const int max_workers=57;
	#endif
#endif

#ifdef NOTEBOOK
	const int FREQ=1900;
	int emitter_affinity=0;
	int collector_affinity=1;
	int controller_affinity=1;
	int ncores=4;
	int affinities[]={2,3};
		int generator_affinity=0;
	int max_workers=2;
#endif

#ifdef ORIONE
	const long int FREQ=3000;
	const int max_workers=2;
	int emitter_affinity=0;
	int collector_affinity=1;
	int ncores=4;
	int affinities[]={2,3};
	int generator_affinity=0;
	int controller_affinity=1;
		int generator_affinity_smt=0;


#endif


#endif /* AFFINITIES_H_ */
