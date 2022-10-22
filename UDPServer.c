#include <arpa/inet.h> //for sockaddr_in, and inet_addr()
#include <stdio.h> 
#include <stdlib.h> //for atoi()
#include <string.h> //for memset()
#include <strings.h>
#include <argp.h>
#include <sys/socket.h>
#include <sys/poll.h> //for poll stuff??
#include <unistd.h> //for close()
#include <time.h> //for timespec
#include <fcntl.h> // for fcntl(sock.fd, F_SETFL, O_NONBLOCK);
#include <endian.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void DieWithError(char *errorMessage); //error handling function

void print_bytes(void *ptr, int size);

struct server_arguments {
	int port;
	int dropPercent;
};

/** TA:
 * I'm not sure the sizes of these integer types
 * are guaranteed to be the same across hosts. So, it
 * would likely be safer to be more specific: e.g. uint32_t 
 * 
 * Fine because we're all using the same environment
*/
typedef struct clientInfo{

    unsigned long ipAddress;  //32 bits
	unsigned short port;  //16 bits
	int highestCurr; //4 bytes
	int timeToUpdate; //4 bytes
} clientInfo;

typedef struct accTimeReq{ //total of 24 bytes
    int seqNum; //4 bytes
    int versionNum; //4 bytes
    uint64_t seconds; //8 bytes
    uint64_t nanoSeconds; //8 bytes
} accTimeReq;

typedef struct accTimeResp{ //tota
    int seqNum; //4 bytes
    int versionNum; //4 bytes
    uint64_t clientSeconds; //8 bytes
    uint64_t clientNanoSeconds; //8 bytes
	uint64_t serverSeconds; //8 bytes
    uint64_t serverNanoSeconds; //8 bytes

} accTimeResp;



error_t server_parser(int key, char *arg, struct argp_state *state) {
	struct server_arguments *args = state->input;
	int num;
	error_t ret = 0;
	switch (key) {
	case 'p':
		args->port = atoi(arg);
		if (args->port == 0) {
			argp_error(state, "Port given is less than or equal to 0, invalid");
		} else if (args->port <= 1024) {
			argp_error(state, "Port must be greater than 1024, invalid");
		}
		break;
	case 'd':
		num = atoi(arg);
		if (num < 0 ||  100 < num) {
			argp_error(state, "Invalid, drop case must be inbetween 0-100");
		}
		args->dropPercent = num;
		break;

    //unknown option
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

void *server_parseopt(struct server_arguments *args, int argc, char *argv[]) {
	memset(args, 0, sizeof(*args));

	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" , 0 },
		{ "drop", 'd', "drop", 0, "The percent chance a given packet will be dropped. Zero by default", 0 },
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, args) != 0) {
		printf("Got error in parse\n");
		
	}
	if (!args->port) {
		printf("Got error in Port parse\n");
		exit(1);
	}

	return args;
}


int main(int argc, char *argv[]){

    struct server_arguments args;
    memset(&args, 0, sizeof(args));
    server_parseopt(&args, argc, argv);
    srand(time(NULL));

	/** TA:
	 * You may want to look at Bobby's code from a0
	 * for the `LOG` function. That way you don't need to
	 * bother commenting out all these printf statements.
	 * 
	 * And since most of your prints seem to be 'debugging prints',
	 * maybe just try out the gdb debugger?
	*/
	

    //function from poll()
    struct pollfd sock;
    //wait for some event on a file descriptor

	//struct pollfd {
    //    int fd;
    //    short events;
    //    short revents;
    //};

    int test = 1;
	int littleEndian = 0;

	/** TA:
	 * What is the point of this test?
	 * This is taken care of by the `hton` type functions, where
	 * `h` is known already.
	 * You should ALWAYS convert to host byte ordering when receiving integer data.
	 * It costs you nothing if it is already in the correct order.
	 * 
	 * Typically the network will send with big-endian (though not always)
	*/
	if(*(char *)&test == 1){
		//printf("Little Endian\n");
		littleEndian = 1;
	}

    //setting file descriptor
	sock.fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock.fd < 0) {
		perror("socket() failed");
		exit(1);
	}

    //manditory for poll(), explaining I/O, 
	sock.events = POLLIN;
	//short revents; ---> filled by the kernel with the events that actually occurred.
	// ----> The bits returned in revents can include any of those specified in events

    //manditory for non blocking connection
	//fcntl(sock.fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in servAddr; // Local address
	memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
	servAddr.sin_family = AF_INET; // IPv4 address family
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
	servAddr.sin_port = htons(args.port); // Local port

    if (bind(sock.fd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
		DieWithError("bind() failed");
	}

    //All Client Info
    struct clientInfo *allClientInfo = malloc(50 * sizeof(struct clientInfo));

    for(int i = 0; i < 50; i++){
        //allClientInfo[i].ipAddress; 
		allClientInfo[i].port = 1; //means not set
		//allClientInfo[i].mostRec;
    }

    int alive = 1;
    int timeout = -1;
    int pollRet;
    int bytesRecv;
	int bytesSent;
    int randNum = 0;

    accTimeReq currTimeReq;
    accTimeResp currTimeResp;

	struct in_addr temp;

	int foundClient = -1;
	int foundClientIndex = -1;

	int holder1;
	int holder2;

	int droppedRes;

	struct timespec currTime;
    clock_gettime(CLOCK_REALTIME, &currTime);

    struct sockaddr_in currAddr;
	socklen_t currAddr_len = sizeof(currAddr);
	memset(&currAddr, 0, currAddr_len);

    //printf("Before Alive!\n");

    while(alive){

        //printf("before\n");
        pollRet = poll(&sock, 1, timeout);
        
		//printf("After\n");

        if(pollRet == -1){
            printf("poll() failed\n");
        
        }else if(pollRet == 0){
            //printf("idle\n");
        
        }else if(pollRet == 1){

			//printf("Connection made!\n");

            if(sock.revents & POLLIN){

				//printf("POLLIN made!\n");

                
                //use all packets
                printf("HERE!!!\n"); 

                bytesRecv = recvfrom(sock.fd, &currTimeReq, sizeof(currTimeReq), 0, (struct sockaddr *)&currAddr, &currAddr_len);

                if(args.dropPercent == 0){
					droppedRes = 1;
				}else{

                    randNum = rand() % (100 + 1 - 1) + 1;

					//rintf("Random Num: --------------> %d\n", randNum);
                    if(randNum <= args.dropPercent){
						droppedRes = 0;
					}else{
						droppedRes = 1;
					}
				}

				if(droppedRes == 1){
			

                    if(bytesRecv <= 0){
                       printf("##################!\n");
                    }else{

						//printf("Curr Port Size: %d,    :", sizeof(currAddr.sin_port));
						//printf("Curr Port Size: %d,    :", sizeof(currAddr.sin_port));
						//print_bytes(&currAddr.sin_port, sizeof(currAddr.sin_port));
                        //printf("\n");


                        //typedef struct accTimeResp{ //tota
                        //   int seqNum; //4 bytes
                        //   int versionNum; //4 bytes
                        //   uint64_t clientSeconds; //8 bytes
                        //   uint64_t clientNanoSeconds; //8 bytes
	                    //   uint64_t serverSeconds; //8 bytes
                        //   uint64_t serverNanoSeconds; //8 bytes
                        //} accTimeResp;
                        clock_gettime(CLOCK_REALTIME, &currTime);
 
                        currTimeResp.seqNum = currTimeReq.seqNum;
						currTimeResp.versionNum = currTimeReq.versionNum;
						currTimeResp.clientSeconds = currTimeReq.seconds;
						currTimeResp.clientNanoSeconds = currTimeReq.nanoSeconds;

						//RESPONSE IS READY!!!

						if(littleEndian == 1){ //checked earlier, computer is little, need to translate to big!!
							currTimeResp.serverSeconds = htobe64((uint64_t)currTime.tv_sec);  //double check tmr!!!
							currTimeResp.serverNanoSeconds = htobe64((uint64_t)currTime.tv_nsec);	
						    //currTimeResp.serverSeconds = (uint64_t)currTime.tv_sec;
							//currTimeResp.serverNanoSeconds = (uint64_t)currTime.tv_nsec;
						}else{
							currTimeResp.serverSeconds = (uint64_t)currTime.tv_sec;
							currTimeResp.serverNanoSeconds = (uint64_t)currTime.tv_nsec;
						}

                        //for testing
						holder1 = ntohl(currTimeReq.seqNum);
						holder2 = ntohl(currTimeReq.versionNum);


						printf("Reciving Request %d ****************************** Version %d\n", holder1, holder2);
						//printf("Original ###\n");

						//printf("Seconds: ");
						//print_bytes(&currTimeReq.seconds, sizeof(currTimeReq.seconds));
						//printf("\nNANO Seconds: ");
						//print_bytes(&currTimeReq.nanoSeconds, sizeof(currTimeReq.nanoSeconds));
						//printf("\n");

						currTimeReq.seconds = ntohl(currTimeReq.seconds);
						currTimeReq.nanoSeconds = ntohl(currTimeReq.nanoSeconds);


						
						
						//typedef struct accTimeReq{ //total of 24 bytes
                        //   int seqNum; //4 bytes
                        //   int versionNum; //4 bytes
                        //   uint64_t seconds; //8 bytes
                        //   uint64_t nanoSeconds; //8 bytes
                        //} accTimeReq;

                        foundClient = -1;
						foundClientIndex = -1;
						for(int i = 0; i < 50; i++){
                           //allClientInfo[i].ipAddress;
						   if(allClientInfo[i].port == 1){ //not set
							 foundClient = -1;
							 foundClientIndex = i;
							 //printf("FinalINDEX: %d\n", foundClientIndex);
							 break;
							
							
							}else if(allClientInfo[i].port == currAddr.sin_port && allClientInfo[i].ipAddress == currAddr.sin_addr.s_addr){

							foundClient = 1;
							foundClientIndex = i;
							//printf("Final 2: %d\n", foundClientIndex);
							break;
						   } 
						}	

						if(foundClient == 1){
                           //we have already used this client
						   //printf("Hello There! Stored in index: %d\n", foundClientIndex);




						}else if(foundClient == -1){
							//new client!!
                            //printf("A new Friend at index: %d!\n", foundClientIndex);


                            allClientInfo[foundClientIndex].port = currAddr.sin_port;
							allClientInfo[foundClientIndex].ipAddress = currAddr.sin_addr.s_addr;
                            allClientInfo[foundClientIndex].highestCurr = 1;
							allClientInfo[foundClientIndex].timeToUpdate = 120;

							//typedef struct clientInfo{
                            //   unsigned long ipAddress;  //32 bits
	                        //   unsigned short port;  //16 bits
	                        //   int highestCurr; //4 bytes
	                        //   int timeToUpdate; //4 bytes
                            //   } clientInfo;
					    }
                        
                        
                        temp.s_addr = allClientInfo[foundClientIndex].ipAddress;
						//We have a Response ready, we have found where the client is in our addresses, now the fun begins
                        holder1 = ntohl(currTimeReq.seqNum);
						if(holder1 < allClientInfo[foundClientIndex].highestCurr){
							/** TA:
							 * I think you forgot something about the port and its endianness. Whoops
							*/
							printf("<%s>:<%lu><%d><%d>\n", inet_ntoa(temp), currAddr.sin_port, holder1, allClientInfo[foundClientIndex].highestCurr);
							//printf("")
						}else{
							//printf("Other Ip Address %s\n", inet_ntoa(temp));
							//printf("")
							//print_bytes(&currAddr.sin_port, sizeof(currAddr.sin_port));
							//printf("Port: %lu\n", currAddr.sin_port);
							//printf("Other: %d\n", currAddr.sin_port);
							//printf("")
							//printf("<%s>:<%lu><%d><%d>\n", inet_ntoa(temp), currAddr.sin_port, holder1, allClientInfo[foundClientIndex].highestCurr);
							//printf("Less than max!\n");

							allClientInfo[foundClientIndex].highestCurr = holder1;
							allClientInfo[foundClientIndex].timeToUpdate = 120;
						}


						//printf("Curr Address Size %d", sizeof(currAddr.sin_addr.s_addr));
                        //print_bytes(&currAddr.sin_addr.s_addr, sizeof(currAddr.sin_addr.s_addr));
						//printf("\n");

                        //ssize_t sendto(int socket, const void *message, size_t length,
                        //int flags, const struct sockaddr *dest_addr,
                        // socklen_t dest_len);
						//bytesRecv = recvfrom(sock.fd, &currTimeReq, sizeof(currTimeReq), 0, (struct sockaddr *)&currAddr, &currAddr_len);

						bytesSent = sendto(sock.fd, &currTimeResp, sizeof(currTimeResp), 0, (struct sockaddr *)&currAddr, sizeof(currAddr));
                        
						if(bytesSent <= 0){
							//printf("Nothing Sent for %d", holder1);
						}else{
							//printf("Sent for %d", holder1);
						}
					}
		                
	                
                    //sendto()
                    
				}
                
              
            }

            //alive = 0;

        }
    }
}


void DieWithError(char *errorMessage){
    perror(errorMessage);
    exit(1);
}

void print_bytes(void *ptr, int size) 
{
    unsigned char *p = ptr;
    int i;
    for (i=0; i<size; i++) {
        printf("%02hhX ", p[i]);
    }
    //printf("\n");
}
