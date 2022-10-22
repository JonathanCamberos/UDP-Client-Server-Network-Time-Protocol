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

void DieWithError(char *errorMessage); //error handling function

void print_bytes(void *ptr, int size);

struct client_arguments {
	struct sockaddr_in servInfo; /* You can store this as a string, but I probably wouldn't */
	int numTimeRequests;
    time_t timeout;
};

typedef struct timeReq {
	int timeToLive;
	int state;
	double theta;
	double delta;
} timeReq;

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

error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;
	int len;
	switch (key) {
	//IP case "-a"
    case 'a':
        memset(&args->servInfo, 0, sizeof(args->servInfo)); // double check - zero out (textbook)
		
		// int inet_pton(int af, const char *restrict src, void *restrict dst);
		//convert IPv4 or IPv6 address into necessary format, (if error then end)
		if (!inet_pton(AF_INET, arg, &args->servInfo.sin_addr.s_addr)) { 
			argp_error(state, "Dotted-Decimal required, given formatting is Invalid");
		}
		args->servInfo.sin_family = AF_INET; // Use constant for Address family (textbook)
		break;

	//Port case "-p"
	case 'p':
		//converts string to integer (returns 0 if string of chars)
		args->servInfo.sin_port = atoi(arg);
		//if invalid port is given
		if (args->servInfo.sin_port <= 0) {
			argp_error(state, "Port given is less than or equal to 0, invalid");
		}
		//translate short integer from host byte order to network byte order (little/big endian)
		args->servInfo.sin_port = htons(args->servInfo.sin_port); // Server port
		break;

	//Hash Request case "-n"
	case 'n':
		//converts string to integer
		args->numTimeRequests = atoi(arg);
		//if hashNum request is less than 0
		if (args->numTimeRequests < 0) {
			argp_error(state, "HashNum request given in less than 0, invalid");
		}
		break;
	
	//File Case "-t"
	case 't':
		//Check if file can be opened/found, returns file pointer
		//args->fileName = fopen(arg, "r");  ******
        
        args->timeout = atoi(arg);
        if (args->timeout < 0) {
			argp_error(state, "Could not find/open given file, error");
		}
      	break;

	//No Known Options Chosen, error
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

void client_parseopt(struct client_arguments *args, int argc, char *argv[]) {
	struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the server", 0},
		{ "timereq", 'n', "timereq", 0, "The number of time requests to send to the server", 0},
		{ "timeout", 't', "timeout", 0, "The time in seconds to wait after sending or receiving a response before terminating", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0 };
	
	//printf("hello 1\n");

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, args) != 0) {
		printf("Got error in parse\n");
		exit(1);
	}	
	
	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	if (!args->servInfo.sin_addr.s_addr) {
		printf("Got error in IP parse\n");
		exit(1);
	}
	if (!args->servInfo.sin_port) {
		printf("Got error in Port parse\n");
		exit(1);
	}
	if (args->numTimeRequests < 0) {
		printf("Got error in time request\n");
		exit(1);
	}
	if (!args->timeout) {
		printf("Got error in time out\n");
		exit(1);
	}

}

int main(int argc, char *argv[]){

	struct client_arguments args;
    memset(&args, 0, sizeof(args));
    client_parseopt(&args, argc, argv);
    srand(time(NULL));

	//printf("time Requests: %d\n", args.numTimeRequests);
	
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
	sock.events = POLLIN | POLLOUT;
	//short revents; ---> filled by the kernel with the events that actually occurred.
	// ----> The bits returned in revents can include any of those specified in events

    //manditory for non blocking connection
	//fcntl(sock.fd, F_SETFL, O_NONBLOCK);

    //structure
    struct timeReq *allTimeRequests = malloc(args.numTimeRequests * sizeof(struct timeReq));

	//struct timeReq {
	//  int timeToLive;
	//  int state;
	//  double theta;
	//  double delta;
    //};

    //typedef struct accTimeReq{
    //  int seqNum;
    //  int versionNum;
    //  uint64_t seconds;
    //  uint64_t nanoSeconds;
    //}accTimeReq;

	//typedef struct accTimeResp{
    //	int seqNum;
    //	int versionNum;
    //	uint64_t clientSeconds;
    //	uint64_t clientNanoSeconds;
	//	uint64_t serverSeconds;
    //	uint64_t serverNanoSeconds;

	//} accTimeResp;

	for(int i = 0; i < args.numTimeRequests; i++){
        allTimeRequests[i].state = 0; //means not sent
        allTimeRequests[i].timeToLive = -1; //means not sent or dead
    }

	struct timespec currTime;
    clock_gettime(CLOCK_REALTIME, &currTime);

	accTimeReq currTimeRequest; //where recvfrom and sendto will write from
	accTimeResp currTimeResponse; //where recvfrom and sendto will write from

	int expectedSend = sizeof(currTimeRequest);

	int alive = 1;
	int timeout = -1;
	int pollRet = 0;
	time_t timePassed = 0;
	time_t lastTimeCheck = time(NULL); //current time
	int bytesSent;
	int bytesRecv;
	int allSent = 0;

	while(alive){

		pollRet = poll(&sock, 1, timeout);

		if(pollRet == -1){
			timePassed = time(NULL) - lastTimeCheck;
			lastTimeCheck += timePassed;
			printf("poll() failed\n");

		}else if(pollRet == 0){

			//printf("poll RET 0\n");

			timePassed = time(NULL) - lastTimeCheck;
			lastTimeCheck += timePassed;
			//check status / update each timeRequest
			for(int i = 0; i < args.numTimeRequests; i++){

				if(allTimeRequests[i].timeToLive < 0){
					//ignore
				}else if (allTimeRequests[i].timeToLive - timePassed <= 0){
					//time has run out waiting for call
					allTimeRequests[i].timeToLive = -1; //dead
					allTimeRequests[i].state = 2; //died waiting on lover
					
				}else{

					allTimeRequests[i].timeToLive = allTimeRequests[i].timeToLive - timePassed;
					if(timeout < allTimeRequests[i].timeToLive){ //find a higher timeout
						timeout = allTimeRequests[i].timeToLive;
					}
				}
			}
			alive = 0;
			for(int k = 0; k < args.numTimeRequests; k++){

				//printf("Another for %d, time is %d\n", k, allTimeRequests[k].timeToLive);
				if(allTimeRequests[k].timeToLive > 0){
					alive = 1; //someone is still alive
				}
			}
			


		}else if(pollRet == 1){

			//printf("Connection Made\n");

			//connection made
			timePassed = time(NULL) - lastTimeCheck;
			lastTimeCheck += timePassed;

			//check status / update each timeRequest
			for(int i = 0; i < args.numTimeRequests; i++){

				if(allTimeRequests[i].timeToLive < 0){
					//ignore
				}else if (allTimeRequests[i].timeToLive - timePassed <= 0){
					//time has run out waiting for call
					allTimeRequests[i].timeToLive = -1; //dead
					allTimeRequests[i].state = 2; //died waiting on lover
					
				}else{

					allTimeRequests[i].timeToLive = allTimeRequests[i].timeToLive - timePassed;
					if(timeout < allTimeRequests[i].timeToLive){
						timeout = allTimeRequests[i].timeToLive;
					}
				}
			}

			if (sock.revents & POLLOUT) {

				//printf("Writing!!\n");
					
				//printf("Sending All Messages ****************************\n");

				for(int i = 0; i < args.numTimeRequests; i++){
                    printf("Sending Packet %d\n", i);
					//havent sent response yet
					if(allTimeRequests[i].state == 0){  

						currTimeRequest.seqNum = htonl(i+1);
						currTimeRequest.versionNum = htonl(7);

                        //printf("Original\n");
						//printf("Seconds: ");
						//print_bytes(&currTime.tv_sec, sizeof(currTime.tv_sec));
						//printf("\nNANO Seconds: ");
						//print_bytes(&currTime.tv_nsec, sizeof(currTime.tv_nsec));

						if(littleEndian == 1){ //checked earlier, computer is little, need to translate to big!!
							//currTimeRequest.seconds = le64toh((uint64_t)currTime.tv_sec);  //double check tmr!!!
							//currTimeRequest.nanoSeconds = le64toh((uint64_t)currTime.tv_nsec);
							 	
							currTimeRequest.seconds = htobe64((uint64_t)currTime.tv_sec);  //double check tmr!!!
							currTimeRequest.nanoSeconds = htobe64((uint64_t)currTime.tv_nsec);


						}else{
							currTimeRequest.seconds = (uint64_t)currTime.tv_sec;
							currTimeRequest.nanoSeconds = (uint64_t)currTime.tv_nsec;
						}
					
						bytesSent = sendto(sock.fd, &currTimeRequest, sizeof(currTimeRequest), 0, (struct sockaddr *)&args.servInfo, sizeof(args.servInfo));

						//if more than 1 byte was sent
						if (bytesSent < 0) {
							DieWithError("sendto() failed");
						}

						
						//printf("Sending Request %d ******************************\n", i+1);
						//printf("Seconds: ");
						//print_bytes((uint64_t *) &currTime.tv_sec, sizeof((uint64_t) currTime.tv_sec));
						//printf("\nNANO Seconds: ");
						//print_bytes((uint64_t *) &currTime.tv_nsec, sizeof((uint64_t) currTime.tv_nsec));
						//printf("\ncurrTime %d Seconds, %ld Nano Seconds\n", currTime.tv_sec, currTime.tv_nsec);
						
						//printf("Translated ###\n");

						//printf("Seconds: ");
						//print_bytes(&currTimeRequest.seconds, sizeof(currTimeRequest.seconds));
						//printf("\nNANO Seconds: ");
						//print_bytes(&currTimeRequest.nanoSeconds, sizeof(currTimeRequest.nanoSeconds));
						//printf("\ncurrTime %d Seconds, %ld Nano Seconds\n", currTimeRequest.seconds, currTimeRequest.nanoSeconds);

						//printf("Sent packet for timeRequest: %d, with expected size of %d, actual size of %d\n", i, expectedSend, bytesSent);
						
						
						//printf("only once 80808080808080 \n");
						allTimeRequests[i].timeToLive = args.timeout;
						allTimeRequests[i].state = 1; //request sent out, waiting for response

						
					}
					

				
				}	

				allSent = 1;
				for(int i = 0; i < args.numTimeRequests; i++){
					if(allTimeRequests[i].state == 0){
						allSent = 0; //forgot to send one!
					}
				}
				if(allSent == 1){
					//printf("\nEVERYTHING SENT OUT )@)))@)@)@)@)@)))@)@)@)@@)))@)@)@)@@)))@)@)@)@@)))@)@)@)@\n");
					//all requests have been sent;
					sock.events &= ~POLLOUT; //removing sending option from POLL
				}
				//printf("After\n");
				
				
			}

            
			if(sock.revents & POLLIN){

				//printf("Reading!!\n");

				bytesRecv = recvfrom(sock.fd, &currTimeResponse, sizeof(currTimeResponse), 0, NULL, 0);

				if(bytesRecv < 0){
					printf("recvfrom() failed");
				}
				currTimeResponse.seqNum = ntohl(currTimeResponse.seqNum);
				currTimeResponse.versionNum = ntohl(currTimeResponse.versionNum);

				if(currTimeResponse.versionNum == 7){

					allTimeRequests[currTimeResponse.seqNum-1].state = 3;
					allTimeRequests[currTimeResponse.seqNum-1].timeToLive = -1; //to ignore recieved packages

				

					if(littleEndian == 1){ //checked earlier
						//currTimeResponse.clientSeconds = le64toh(currTimeResponse.clientSeconds);
						//currTimeResponse.clientNanoSeconds = le64toh(currTimeResponse.clientNanoSeconds);
						//currTimeResponse.serverSeconds = be64toh(currTimeResponse.serverSeconds);
						//currTimeResponse.serverNanoSeconds = be64toh(currTimeResponse.serverNanoSeconds);
						
						currTimeResponse.clientSeconds = be64toh(currTimeResponse.clientSeconds);
						currTimeResponse.clientNanoSeconds = be64toh(currTimeResponse.clientNanoSeconds);
						
						currTimeResponse.serverSeconds = be64toh(currTimeResponse.serverSeconds);
						currTimeResponse.serverNanoSeconds = be64toh(currTimeResponse.serverNanoSeconds);
					}

					
					printf("Information from response Packet: %d\n", currTimeResponse.seqNum);
					//print_bytes()

					//printf("Sending Request %d\n", i+1);
					//printf("Seconds: ");
					//print_bytes((uint64_t *) &currTime.tv_sec, sizeof((uint64_t) currTime.tv_sec));
					//printf("\nNANO Seconds: ");
					//print_bytes((uint64_t *) &currTime.tv_nsec, sizeof((uint64_t) currTime.tv_nsec));
					//printf("\ncurrTime %d Seconds, %ld Nano Seconds\n", currTime.tv_sec, currTime.tv_nsec);
						
					//printf("Translated ###\n");

					//printf("Seconds: ");
					//print_bytes(&currTimeResponse.clientSeconds, sizeof(currTimeResponse.clientSeconds));
					//printf("\nNANO Seconds: ");
					//print_bytes(&currTimeResponse.clientNanoSeconds, sizeof(currTimeResponse.clientNanoSeconds));
					//printf("\ncurrTime %d Seconds, %ld Nano Seconds\n", currTimeResponse.clientSeconds, currTimeResponse.clientSeconds);

					//printf("Sent packet for timeRequest: %d, with expected size of %d, actual size of %d\n", i, expectedSend, bytesSent);
					
					
					clock_gettime(CLOCK_REALTIME, &currTime);
					time_t recvClientSec = currTimeResponse.clientSeconds;
					time_t recvServerSec = currTimeResponse.serverSeconds;
					time_t recvCurrentSec = currTime.tv_sec;

					time_t thetaSecHolder = (recvServerSec - recvClientSec) + (recvServerSec - recvCurrentSec);
					time_t deltaSecHolder = (recvCurrentSec - recvClientSec);

					long recvClientNano = currTimeResponse.clientNanoSeconds;
					long recvServerNano = currTimeResponse.serverNanoSeconds;
					long recvCurrentNano = currTime.tv_nsec;

					long thetaNanoHolder = (recvServerNano - recvClientNano) + (recvServerNano - recvCurrentNano);
					long deltaNanoHolder = recvCurrentNano - recvClientNano;

					allTimeRequests[currTimeResponse.seqNum-1].theta = (double)thetaSecHolder / 2.0 + (double)thetaNanoHolder / 2000000000.0;
					allTimeRequests[currTimeResponse.seqNum-1].delta = (double)deltaSecHolder + (double)deltaNanoHolder / 1000000000.0;

				}else{
					printf("Invalid Version Provided");
				}
			}

			


			alive = 0;
			for(int k = 0; k < args.numTimeRequests; k++){
				//printf("Another for %d, time is %d\n", k, allTimeRequests[k].timeToLive);
				if(allTimeRequests[k].timeToLive > 0){
					alive = 1; //someone is still alive
				}
			}
			//printf("$$$$\n");
			
			

			
		}
		

		
	}

	//printf("DeadXD\n");

	for(int i = 0; i < args.numTimeRequests; i++){

		if(allTimeRequests[i].state == 2){
			printf("%d: Dropped\n", i+1);
		}else{
			printf("%d: %f %f\n", i+1, allTimeRequests[i].theta, allTimeRequests[i].delta);
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
