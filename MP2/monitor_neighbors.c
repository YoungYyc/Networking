#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define NULL_TER '\0'
#define INIFINITY 8388608	//2^23

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];
////////////////////////////////
struct nodeinfo
{
	int distance;
	int next_hop;	
};
extern struct nodeinfo DVtable[256];
extern int initcost[256];
// extern int realcost[256];
extern char logfile[100];
extern int PVtable[256][256];

void inittable(const char* costfile)
{
	int i, j;
	for(i = 0; i < 256; i++) {
		initcost[i] = 1;
		DVtable[i].distance = 0;
		DVtable[i].next_hop = -1;
		for(j = 0; j < 256; j++) {
			PVtable[i][j] = -1;
		}
	}
	// printf("PVtable = %d\n", PVtable[0][0]);
	FILE *fp;
	char buf[255];
	fp = fopen(costfile, "r");
	while(1) {
		if(fgets(buf, 255, fp) == NULL) break;
		int node;
		int distance;
		sscanf(buf, "%d %d", &node, &distance);
		initcost[node] = distance;
		// DVtable[node].distance = distance;
		// DVtable[node].next_hop = node;
	}

}
//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];

	int bytesRecvd;
	FILE *tolog;
	//clear content in the file
	tolog = fopen(logfile, "w+");
	// fclose(tolog);
	while(1)
	{
		// tolog = fopen(logfile, "a+");
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);

		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			// printf("%.*s\n",bytesRecvd, recvBuf);
			
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
			if(strstr(recvBuf, "HEREIAM")) {
				if(DVtable[heardFrom].distance == 0) {
					DVtable[heardFrom].distance = initcost[heardFrom];
					DVtable[heardFrom].next_hop = heardFrom;
					PVtable[heardFrom][0] = heardFrom;
				}
				// realcost[heardFrom] = initcost[heardFrom];
				//newCost
				if(DVtable[heardFrom].next_hop == heardFrom && DVtable[heardFrom].distance != initcost[heardFrom]) {
					DVtable[heardFrom].distance = initcost[heardFrom];

				}
				// if(DVtable[heardFrom].distance != 0 && DVtable[heardFrom].distance < realcost[heardFrom] ) {
				// 	realcost[heardFrom] = DVtable[heardFrom].distance;

				// }
				// realcost[heardFrom] = DVtable[heardFrom].distance;
				// printf("%d\n", DVtable[heardFrom].distance);
				char realcostbuf[1000];
				int i, j;
				memset(realcostbuf, 0, sizeof(realcostbuf));
				strcat(realcostbuf, "realcost:");
				for(i = 0; i < 256; i++) {
					//split horizon
					if(DVtable[i].distance != 0){
						char temp[10];
						char PVtemp[256];
						memset(PVtemp, NULL_TER, sizeof(PVtemp));
						for(j = 0; j < 256; j++) {
							char IDtemp[15];
							if(PVtable[i][j] > -1) {
								if(j > 0) {
									if(PVtemp[j-1] != NULL_TER) {
										strcat(PVtemp, "|");
									}
								}
								sprintf(IDtemp, "%d", PVtable[i][j]);
								strcat(PVtemp, IDtemp);
							}
							else 
								break;
						}
						// printf("%d -> %s\n", i, PVtemp);
						// printf("%d = %d\n", i, DVtable[i].distance);
						sprintf(temp, "%d %d %s,", i, DVtable[i].distance, PVtemp);
						strcat(realcostbuf, temp);
					}
				}
				// printf("%s\n", realcostbuf);
				realcostbuf[strlen(realcostbuf)-1] = NULL_TER;
				// printf("%s\n", realcostbuf);
				sendto(globalSocketUDP, realcostbuf, strlen(realcostbuf), 0,
				  (struct sockaddr*)&globalNodeAddrs[heardFrom], sizeof(globalNodeAddrs[heardFrom]));

			}
			// printf("%s\n", initcost);
			//record that we heard from heardFrom just now.
			// printf("%d: %ld\n",heardFrom, globalLastHeartbeat[heardFrom].tv_sec);
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
			int i, j, k;
			//check if node is dead
			// printf("heardFrom = %d\n", (int)globalLastHeartbeat[heardFrom].tv_sec);
			// printf("i = %d\n", (int)globalLastHeartbeat[0].tv_sec);
			// printf("next_hop = %d\n", DVtable[]);
			for(i = 0; i < 256; i++) {
				//deal with dead link
				if(globalLastHeartbeat[heardFrom].tv_sec - globalLastHeartbeat[i].tv_sec > 2 && DVtable[i].next_hop == i) {

					DVtable[i].distance = 0;
					DVtable[i].next_hop = -1;
					for(j = 0; j < 256; j++) {
						PVtable[i][j] = -1;
					}
					// printf("check1\n");
					// realcost[i] = DVtable[i].distance;
					for(j = 0; j < 256; j++) {
						if(DVtable[j].next_hop == i) {
							DVtable[j].distance = 0;
							DVtable[j].next_hop = -1;
							// realcost[j] = 0;
							for(k = 0; k < 256; k++) {
								PVtable[j][k] = -1;
							}
						}
					}
				}
					// printf("check2\n");
					//clear remaining dead link
				if(DVtable[DVtable[i].next_hop].next_hop == -1) {
					DVtable[i].distance = 0;
					DVtable[i].next_hop = -1;
					for(j = 0; j < 256; j++) {
						PVtable[i][j] = -1;
					}
					// printf("check1\n");
					// realcost[i] = DVtable[i].distance;
					for(j = 0; j < 256; j++) {
						if(DVtable[j].next_hop == i) {
							DVtable[j].distance = 0;
							DVtable[j].next_hop = -1;
							// realcost[j] = 0;
							for(k = 0; k < 256; k++) {
								PVtable[j][k] = -1;
							}
						}
					}

				}
				
				// printf(" time of %d = %d\n", i, (int)globalLastHeartbeat[i].tv_sec);
			}
			// printf("%d: %ld\n",heardFrom, globalLastHeartbeat[heardFrom].tv_sec);
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp(recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			// ...
			short int no_destID;
			short int destID;
			memcpy(&no_destID, recvBuf+4, sizeof(short int));
			destID = ntohs(no_destID);
			char text[bytesRecvd-6+1];
			memcpy(text, recvBuf+6, bytesRecvd-6);
			text[bytesRecvd-6] = NULL_TER;
			char logline[200];
			// printf("text = %s\n", text);
			// printf("recvBuf = %.*s ",bytesRecvd-6, recvBuf+6);
			// printf("destID = %d ", destID);
			// printf("recvfrom = %s\n", fromAddr);
			// printf("logfile = %s\n", logfile);
			if(destID == globalMyID) {
				sprintf(logline, "receive packet message %s\n", text);
			}
			else if(DVtable[destID].distance != 0){	
				// char sendbuf[bytesRecvd];
				// strncpy(sendbuf, "send", 4);

				// printf("next_hop = %d\n", DVtable[destID].next_hop);
				if(strstr(fromAddr, "10.1.1.") == NULL) {
					sprintf(logline, "sending packet dest %d nexthop %d message %s\n", destID, DVtable[destID].next_hop, text);
				}
				else {
					sprintf(logline, "forward packet dest %d nexthop %d message %s\n", destID, DVtable[destID].next_hop, text);
				}
				sendto(globalSocketUDP, recvBuf, bytesRecvd, 0, 
					(struct sockaddr*)&globalNodeAddrs[DVtable[destID].next_hop], sizeof(globalNodeAddrs[DVtable[destID].next_hop]));
				// printf("send to %d\n", DVtable[destID].next_hop);
			}
			else {
				sprintf(logline, "unreachable dest %d\n", destID);
			}
			printf("%s\n", logline);
			fwrite(logline, 1, strlen(logline), tolog);
			fflush(tolog);
			// fclose(tolog);

			// printf("%.*s\n", bytesRecvd-6, recvBuf+6);
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
			short int no_neiID;
			short int neiID;
			int no_newCost;
			int newCost;
			memcpy(&no_neiID,recvBuf+4, sizeof(short int));
			neiID = ntohs(no_neiID);
			// printf("neiID = %d\n", neiID);
			memcpy(&no_newCost, recvBuf+6, sizeof(int));
			newCost = ntohl(no_newCost);
			// printf("newCost = %d\n", newCost);
			int i;
			for(i = 0; i < 256; i++) {
				if(DVtable[i].next_hop == neiID) {
					// printf("DVtable[%d].distance = %d\n", i, DVtable[i].distance);
					DVtable[i].distance = DVtable[i].distance - initcost[neiID] + newCost;
					// realcost[i] = DVtable[i].distance;
					// printf("DVtable[%d].distance = %d\n", i, DVtable[i].distance);
				}
			}
			initcost[neiID] = newCost;
			// printf("initcost[%d] = %d\n", no_neiID, );

		}
		
		//TODO now check for the various types of packets you use in your own protocol
		else if(!strncmp(recvBuf, "realcost", 8))
		{
			// printf("recv = %.*s\n", bytesRecvd, recvBuf);
			int tempcosttable[256];
			int tempPVtable[256][256];
			char tempPVstr[256][256];
			int tempnode;
			int tempcost;
			char tempbuf[bytesRecvd+1];
			char PVtemp[256];
			char *elem, *PVstr;

			memset(tempcosttable, 0, sizeof(tempcosttable));
			memset(tempPVtable, -1, sizeof(tempPVtable));
			memset(tempPVstr, NULL_TER, sizeof(tempPVstr));
			memset(tempbuf, '\0', sizeof(tempbuf));
			strncpy(tempbuf, recvBuf, bytesRecvd);
			tempbuf[bytesRecvd] = NULL_TER;
			elem = NULL;
			printf("%d -> %.*s\n", heardFrom, bytesRecvd, recvBuf);
			//parse the distance
			if(bytesRecvd > 8) {
				elem = strtok(tempbuf+9, ",");
			}

			while(elem != NULL) {
				// printf("%s\n", elem);
				sscanf(elem,"%d %d %s", &tempnode, &tempcost, PVtemp);
				// printf("%d\n", tempnode);
				// printf("%d\n", tempcost);
				// printf("%s\n", PVtemp);
				// printf("tempnode = %d tempcost = %d\n", tempnode, tempcost);
				tempcosttable[tempnode] = tempcost;
				strncpy(tempPVstr[tempnode], PVtemp, strlen(PVtemp));

				elem = strtok(NULL, ",");
			}

			//parse the PVtable
			PVstr = NULL;
			int i, j;
			// printf("heardFrom = %d\n", heardFrom);
			for(i = 0; i < 256; i++) {
				if(strlen(tempPVstr[i]) > 0) {
					// printf("tempPVstr[%d] = %s\n", i, tempPVstr[i]);
					PVstr = strtok(tempPVstr[i], "|");
					while(PVstr != NULL) {
						for(j = 0; j < 256; j++) {
							if(tempPVtable[i][j] == -1) {
								tempPVtable[i][j] = atoi(PVstr);
								break;
							}
						}
						PVstr = strtok(NULL, "|");
					}
				}
			}

			// printf("check\n");

			// init node index heardfrom
			if(DVtable[heardFrom].distance == 0 || initcost[heardFrom] < DVtable[heardFrom].distance) {
				DVtable[heardFrom].distance = initcost[heardFrom];
				DVtable[heardFrom].next_hop = heardFrom;

				for(i = 0; i < 256; i++) {
					PVtable[heardFrom][i] = -1;
				}
				PVtable[heardFrom][0] = heardFrom;

			}
			//deal with lower ID case
			if(DVtable[heardFrom].distance == initcost[heardFrom] && DVtable[heardFrom].next_hop > heardFrom) {
				DVtable[heardFrom].next_hop = heardFrom;

				for(i = 0; i < 256; i++) {
					PVtable[heardFrom][i] = -1;
				}
				PVtable[heardFrom][0] = heardFrom;
			}

			// printf("check2\n");
			for(i = 0; i < 256; i++) {
				if(tempcosttable[i] != 0 && i != globalMyID) {
					//update short path
					if(DVtable[i].distance == 0 || DVtable[i].distance > tempcosttable[i] + DVtable[heardFrom].distance){

						DVtable[i].distance = tempcosttable[i] + DVtable[heardFrom].distance;

						DVtable[i].next_hop = heardFrom;
						for(j = 0; j < 256; j++) {
							PVtable[i][j] = -1;
						}
						PVtable[i][0] = heardFrom;
						for(j = 0; j < 256; j++) {
							if(tempPVtable[i][j] > -1) {
								PVtable[i][j+1] = tempPVtable[i][j];
							}
							else
								break;
						}

					}
					// deal with newCost
					else if(DVtable[i].distance < tempcosttable[i] + DVtable[heardFrom].distance && DVtable[i].next_hop == heardFrom) {
						DVtable[i].distance = tempcosttable[i] + DVtable[heardFrom].distance;
					}
					//deal with lower ID case
					int pathcount = 0;
					if(DVtable[i].distance == tempcosttable[i] + DVtable[heardFrom].distance) {
						if(DVtable[i].next_hop > heardFrom) {
							DVtable[i].next_hop = DVtable[heardFrom].next_hop;
							//clear previous path 
							for(j = 0; j < 256; j++) {
								if(PVtable[i][j] > -1) {
									PVtable[i][j] = -1;
								}
								else
									break;
							}
							//update path
							for(j = 0; j < 256; j++) {
								if(PVtable[heardFrom][j] > -1) {
									PVtable[i][j] = PVtable[heardFrom][j];
									pathcount++;
								}
								else
									break;
							}
							for(j = 0; j < 256; j++) {
								if(tempPVtable[i][j] > -1) {
									PVtable[i][j+pathcount] = tempPVtable[i][j];
								}
								else 
									break;
							}
						}
					}
				}
				// printf("check3\n");
				//fix when link is down
				if(tempcosttable[i] == 0 && DVtable[i].distance != 0 && DVtable[i].next_hop == heardFrom && i != heardFrom) {
					DVtable[i].distance = 0;
					DVtable[i].next_hop = -1;
					for(j = 0; j < 256; j++) {
						PVtable[i][j] = -1;
					}
				}
			}
			// printf("check4\n");
			//check for loop 
			// printf("check2\n");
			int loopflag = 0;
			for(i = 0; i < 256; i++) {
				for(j = 0; j < 256; j++) {
					if(PVtable[i][j] == globalMyID) {
						loopflag = 1;
						break;
					}
				}
				if(loopflag == 1) {
					//clear loop
					for(j = 0; j < 256; j++) {
						PVtable[i][j] = -1;
					}
					DVtable[i].distance = 0;
					DVtable[i].next_hop = -1;
				}
			}
			//clear data in tempcostable
			// memset(tempcosttable, 0, sizeof(tempcosttable));
			// printf("heardFrom = %d\n", heardFrom);
			// for(i = 0; i < 256; i++) {
			// 	if(tempcosttable[i] != 0) {
			// 		printf("i = %d ", i);
			// 		printf("distance = %d\n", tempcosttable[i]);
			// 	}
			// }
			printf("globalMyID = %d\n", globalMyID);
			for(i = 0; i <256; i++) {
				if(DVtable[i].distance != 0) {
					printf("i = %d ", i);
					printf("distance = %d ", DVtable[i].distance);
					printf("next_hop = %d ", DVtable[i].next_hop);
					printf("PV =  ");
					for(j = 0; j < 256; j++) {
						if(PVtable[i][j] > -1) {
							printf("%d-", PVtable[i][j]);
						}
						else 
							break;
					}
					printf("\n");
				}
			}
			// printf("check\n");

		}
		// fclose(tolog);

		// ... 
	}
	//(should never reach here)
	close(globalSocketUDP);
}

