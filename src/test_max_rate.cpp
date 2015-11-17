//just for testing the max rate supported by the generator
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <sched.h>
#include "../includes/cycle.h"
#include "../includes/iaperf.h"
#include "../includes/general.h"
#include "../includes/functional_partitioning_cb.h"
#if defined(USE_ZMQ)
#include <zmq.h>
#endif
using namespace std;
int main(int argc, char *argv[])
{
	printf("Listen on port 8080\n");
	int socket;
	int ret;
	socket=*(int *)receive_connection(1,8080);
	tuple_t tmp;
	int num_classes=atoi(argv[1]);
	int64_t *recvd=(int64_t*)calloc(num_classes,sizeof(int64_t));
	int64_t rcvd=0;
	while((ret=socket_receive(socket, &tmp, sizeof(tuple_t)))==sizeof(tuple_t))
	{
		if(tmp.type==-1)
			break;
		else
			if(tmp.type>=0)
			{
				rcvd++;
				//printf("Ricevuto: %d\n",tmp.type);
				recvd[tmp.type]++;
			}
	}
	//for(int i=0;i<num_classes;i++)
	//	printf("%2.5f\n",((double)recvd[i])/rcvd);


}