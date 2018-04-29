//Included Libraries
#include <wiringPi.h>
#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb,h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>

//Definitions
#define REDLED 	  2
#define YELLOWLED 3
#define SW1 	  12
#define SW2 	  13
#define PB4 	  19
#define PB5 	  20
#define SegENABLE 21
#define SegDP     23
#define SegA      24
#define SegB      25
#define SegC      26
#define SegD      27
#define MSG_SIZE 40


//Function Prototypes
void segmentDisplay(int number);
uint16_t readADC();
void setLED(int number, int onoff);
int getPinStatus(int number);
void checkSignal(int currentreading, int currentreadingindex, int* pastreadings);
void setup();
void *thread1(void *ptr);


//Global Variables
struct sockaddr_in me, server;
socklen_t length;
int sock;

int nopowerflag = 0;



int main(){
	
	//Variables
	int i, b, var, otherRTU = -1, myID;
	int pastreadings[10] = {0}, signalindex = 0; //For checking signal is active
	int currentreading;
	char timespec start, current;
	char msg[MSG_SIZE];
	
	//Run setup operations
	setup();

	//Create the socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("\nError creating socket...\n");
		return -1;
	}

	//Initalize server struct
	bzero(&server, sizeof(server);
	server.sin_family = AF_INET);
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(atoi(argv[1]));

	//Bind Socket 
	if(bind(sock, (const struct sockaddr *)&server, sizeof(server)) < 0){
		printf("\nError binding socket..");
		return -1;
	}

	//Set socket to Broadcast
	var = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &b, sizeof(b));
	if(var < 0){
		printf("\nError in setsockopt()");
		return -1;
	}
	length = sizeof(struct sockaddr_in);

	//Broadcast message to find any other RTU's
	msg = "WHOIS";
	inet_aton("128.206.19.255", &me.sin_addr);
	var = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&me, length);
	
	clock_gettime(CLOCK_MONOTONIC, &start)
	while(1){
		clock_gettime(CLOCK_MONOTINIC, &current);
		memset(msg, '\0', MSG_SIZE);
		if((current.tv_sec - start.tv_sec) <= 1){
			var = recvfrom(sock, message, MSG_SIZE, 0, (struct sockaddr *)&me, &length);
			otherRTU++;
		} else {
			break;
		}
	}	
	myID = otherRTU + 1;

	//call thread 1 to constantly check for messages from other RTU's
	pthread_t t1;
	pthread_create(&t1, NULL, (void *)thread1, (void *)&myID);


	//Main program loop
	while (1){

		currentreading = getADC();

		//Load current signal value into signalcheck array
		pastreadings[signalindex] = currentreading;
		if (signalindex == 9){
			signalindex = 0;
		} else {
			signalindex++;
		}

		//Check the signal
		checkSignal(currentreading, signalindex, &pastreadings);
	}	
	
	
	
}



void setup(){
	//Initialize Wiring Pi and the SPI Bus
        wiringPiSetupGpio();
        wiringPiSPISetup(0, 1000000);


        //7 Segment Display Setup
        pinMode(SegENABLE, OUTPUT);
        pinMode(SegDP, OUTPUT);
        pinMode(SegA, OUTPUT);
        pinMode(SegB, OUTPUT);
        pinMode(SegC, OUTPUT);
        pinMode(SegD, OUTPUT);
}

void checkSignal(int currentreading, currentreadingindex, pastreadings){
	int i;

	//Check for no power

	//Make sure the compare value is 5 readings away
        i = currentreadingindex;
        if (i < 5){
                i = 4 + i;
        } else {
                i = currentreadingindex - 5;
        }

        //Check to see if an older signal is within 2% of new signal
        if ((pastreadings[currentreadingindex] > 0.99 * (float)pastreadings[i]) && 
	    (pastreadings[currentreadingindex] < 1.01 * (float)pastreadings[i])){
                nopowerflag = 1;
        }

	//Check for out of bounds
}



void setLED(int number, int onoff){
	digitalWrite(number, onoff);
}

int getPinStatus(int number){
	return digitalRead(number);
}

uint16_t getADC(){

	uint16_t received = 0;
	uint8_t data[3] = {
		0x01,	//Start Byte
		0xA0,	//Channel 2
		0x00    //Empty Byte
	};

	//Send and recieve data over the SPI bus
	wiringPiSPIDataRW(0, data, 3);

	//00000000 000000XX
	received = data[1];

	//000000XX 00000000
	received = received << 8;

	//000000XX XXXXXXXX
	received = received | data[2];

	return received;
}

void segmentDisplay(int number){
	int a = 0, b = 0, c = 0, d = 0;

	switch (number){
		case 1:
			a = 1;
			break;
		case 2:
			b = 1;
			break;
		case 3:
			a = 1;
			b = 1;
			break;
		case 4:
			c = 1;
			break;
		case 5:
			a = 1;
			c = 1;
			break;
		case 6:
			b = 1;
			c = 1;
			break;
		case 7:
			a = 1;
			b = 1;
			c = 1;
			break;
		case 8:
			d = 1;
			break;
		case 9:
			a = 1;
			d = 1;
			break;		
	}	

	digitalWrite(SegENABLE, 1);
        digitalWrite(SegA, 0);
        digitalWrite(SegB, 0);
        digitalWrite(SegC, 1);
        digitalWrite(SegD, 0);
}

void *thread1(void *ptr){
	char message[MSG_SIZE];
	int ID = *((int *)ptr);

	while(1){
		memset(message,'\0', 40);			
		var = recvfrom(sock, message, MSG_SIZE, 0, (struct sockaddr *)&me, &length);
		printf("\nMessage Recieved: %s", message);
	   //check if message was recieved properly 	
		if(var < 0){					
			printf("\nError Recieving Message..");
			return -1;
		}
		//WHOIS recieved 
		else if(strncmp(message, "WHOIS", 5) == 0){
			sprintf("RTU #%d is active", ID);
			inet_aton("128.206.19.255", &me.sin_addr);
			var = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&me, length);
		}
	}
	pthread_exit(0);
}
