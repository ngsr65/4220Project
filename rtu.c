/*Historian to RTU Command reference
	Format
	#[ID][COMMAND]

	ID is ID of RTU. Single int between 0 and 9
	COMMAND is the command to execute. Single int between 0 and 9

	COMMAND table
	0 - Turn off Red LED
	1 - Turn on Red LED
	2 - Turn off Yellow LED
	3 - Turn on Yellow LED
	4 - Turn off Green LED
	5 - TUrn on Green LED
	5-9 - Reserved

	Example
	#11
	Tells RTU with ID 1 to turn on it's Red LED
*/

//Included Libraries
#include <wiringPi.h>
#include <wiringPiSPI.h>
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
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sched.h>


//Definitions
#define REDLED 	  2
#define YELLOWLED 3
#define GREENLED  4
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
#define PORT 4000
#define CHAR_DEV "/dev/buffer"

//Event enum
enum eventtypes{sigoff, sighigh, siglow, led1, led2, led3, sw1, sw2, pb4, pb5};

//Event struct
struct eventvar{
	int rtuID, led1, led2, led3, sw1, sw2, pb4, pb5;
	enum eventtypes type;
	float voltage;
	struct timespec timestamp;
	struct eventvar *nextevent;
}typedef Event;

//Global Variables
struct sockaddr_in me, server;
socklen_t length;
int sock, myID, cdev_id;
Event* Head;
pthread_t t1, cdev_thread;

//Function Prototypes 
void segementDisplay(int number);
float getADC();
void setLED(int number, int onoff);
int getPinStatus(int number);
void checkSignal(float currentreading, int currentreadingindex, float *pastreadings);
void setup();
void *readKM(void *ptr);
void *thread1(void *ptr);
void *eventthread(void *ptr);
void event(enum eventtypes t);
void add_node(Event *add);
void free_list(Event *node);

int main(){
	
	//Variables
	int i, b, var, otherRTU = -1;
	int signalindex = 0; //For checking signal is active
	float pastreadings[10] = {0}, currentreading;
	struct timespec start, current;
	char msg[MSG_SIZE];
	
	//Run setup operations
	setup();
	
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
		checkSignal(currentreading, signalindex, pastreadings);
	}	
	
	pthread_join(t1, NULL);
	pthread_join(cdev_thread, NULL);

}

void event(enum eventtypes t){
	Event* newevent;
	newevent = (Event*)malloc(sizeof(Event));

	newevent->rtuID = myID; 
	newevent->type = t;
	newevent->led1 = getPinStatus(REDLED);
	newevent->led2 = getPinStatus(YELLOWLED);
	newevent->led3 = getPinStatus(GREENLED);
	newevent->sw1 = getPinStatus(SW1);
	newevent->sw2 = getPinStatus(SW2);
	newevent->pb4 = getPinStatus(PB4);
	newevent->pb5 = getPinStatus(PB5);
	newevent->nextevent = NULL;
	clock_gettime(CLOCK_MONOTONIC, &(newevent->timestamp));

	//Add new event to the linked list 
//	add_node(newevent);

}

void setup(){
	//Variables
	char msg[MSG_SIZE];
	int b, var, otherRTU;

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

	//Create the socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("\nError creating socket...\n");
	}

	//open character device 
	if((cdev_id = open(CHAR_DEV, O_RDWR)) == -1){
		printf("\nCannot open device %s", CHAR_DEV);
	}
	//Initalize server struct
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);

	//Bind Socket 
	if(bind(sock, (const struct sockaddr *)&server, sizeof(server)) < 0){
		printf("\nError binding socket..");
	}

	//Set socket to Broadcast
	var = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &b, sizeof(b));
	if(var < 0){
		printf("\nError in setsockopt()");
	}
	length = sizeof(struct sockaddr_in);

	//Broadcast message to find any other RTU's
	sprintf(msg, "WHOIS");
	inet_aton("128.206.19.255", &me.sin_addr);
	var = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&me, length);
	
	while(1){
		msg[0] = '~';
		msg[1] = '0';
	//	memset(msg, '\0', MSG_SIZE);
	//	var = recvfrom(sock, msg, MSG_SIZE, 0, (struct sockaddr *)&me, &length);
		if (msg[0] = '~'){
			myID = msg[1] - 48;
			break;
		}
	}	

	printf("\nMy ID number is %d", myID);
	

	//call thread 1 to constantly check for messages from other RTU's
//	pthread_t t1, cdev_thread;
	pthread_create(&t1, NULL, (void *)thread1, (void *)&myID);
	pthread_create(&cdev_thread, NULL, (void *)readKM, (void *)&myID);
	
}

void checkSignal(float currentreading, int currentreadingindex, float* pastreadings){
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
        if (((pastreadings[currentreadingindex]) > 0.99 * (pastreadings[i])) && 
	    ((pastreadings[currentreadingindex]) < 1.01 * (pastreadings[i]))){
                event(sigoff);
        }

	//Check for out of bounds
	if (currentreading > 2.0){
		event(sighigh);
	}

	if (currentreading < 1.0){
		event(siglow);
	}
}



void setLED(int number, int onoff){

	digitalWrite(number, onoff);
	if (number == 2){
		event(led1);
	}
	if (number == 3){
		event(led2);
	}
	if (number == 4){
		event(led3);
	}
}

int getPinStatus(int number){
	return digitalRead(number);
}

float getADC(){

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

	return ((float)(received * 3.3) / 1023);
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
	int ID = *((int *)ptr), var;
	
	while(1){
		memset(message,'\0', 40);			
		var = recvfrom(sock, message, MSG_SIZE, 0, (struct sockaddr *)&me, &length);
		printf("\nMessage Recieved: %s", message);

	   	//check if message was recieved properly 	
;		if (var < 0){					
			printf("\nError Recieving Message..");
		} else {
			//WHOIS received 
			if (strncmp(message, "WHOIS", 5) == 0){
				sprintf(message, "RTU #%d is active", ID);
				inet_aton("128.206.19.255", &me.sin_addr);
				var = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&me, length);
			}
	
			//Command received
			if (strncmp(message, "#", 1) == 0){
				//Sent to me
				if (atoi(&(message[1])) == ID){	
					switch(atoi(&(message[2]))){
						case 0:
							setLED(REDLED, 0);
							break;
						case 1:
							setLED(REDLED, 1);
							break;
						case 2:
							setLED(YELLOWLED, 0);
							break;
						case 3: 
							setLED(YELLOWLED, 1);
							break;
						case 4:
							setLED(GREENLED, 0);
							break;
						case 5:
							setLED(GREENLED, 1);
							break;
						case 6:
							
							break;
					}
				}
			}
		}
	}
	pthread_exit(0);
}

void *eventthread(void *ptr){

	struct sched_param param;
	struct itimerspec timer_value;
	int timer = timerfd_create(CLOCK_MONOTONIC, 0);
	uint64_t periods = 0;

	param.sched_priority = 55;
	sched_setscheduler(0, SCHED_FIFO, &param);

	timer_value.it_interval.tv_sec = 1;
	timer_value.it_interval.tv_nsec = 0;
	timer_value.it_value.tv_sec = 0;
	timer_value.it_value.tv_nsec = 100;

	timerfd_settime(timer, 0, &timer_value, NULL);
	read(timer, &periods, sizeof(periods));

	while(1){
		
			

	
		read(timer, &periods, sizeof(periods));
		
	}
	
	pthread_exit(0);
}
//fucntion that will read in events from the character device and greate an event 
void *readKM(void *ptr){
	
	char readin[MSG_SIZE], prev;
	int dummy;
	enum eventtypes x;

	while(1){
		memset(readin, '\0', MSG_SIZE);
		//read in from character device 
		dummy = read(cdev_id, readin, sizeof(readin));	
		//check message was recieved properly 
		if(dummy != sizeof(readin)){
			printf("\nReading in From character Device Failed...");
		}
		switch(readin[0]){
			//button 4 event detected 
			case '1':
				printf("\nButton four event detected");
				x = pb4;
				event(x);
				break;
			//button 5 event detected 
			case '2':
				printf("\nButton 5 event detected");
				x = pb5;
				event(x);
				break;
			//switch 1 event detected 
			case '3':
				printf("\nSwitch 1 event detected");
				x = sw1;
				event(x);
				break;
			//switch 2 event detected 
			case '4':
				printf("\nSwitch 2 event detected");
				x = sw2;
				event(x);
				break;
			case '\0':
				//ignore this case 
				break;
			default:
			//	printf("\nDefault case...");
				break;
		}
	}

	pthread_exit(0);
}
//This function will add an event to the end of our linked list 
void add_node(Event *add){
	//create pointer to the head 
	Event *ptr = Head;
	
	//while loop to find the end of the linked list 
	while(ptr->nextevent != NULL){
		ptr = ptr->nextevent;
	}

	//when loop breaks we want to add the new node 
	ptr->nextevent = add;
}
//function to call when we want to free the list 
void free_list(Event *node){
	
	//Base Case 
	if(node->nextevent == NULL){
		free(node);	
	}

	else{
		free_list(node->nextevent);
		free(node);
	}		
}

void print_list(Event *node){
	Event *ptr = node;

	while(node != NULL){
		printf("\nEvent #%d on RTU #%d at time %ld.%ld", node->type, node->rtuID, node->timestamp.tv_sec, node->timestamp.tv_nsec);
		printf("Pin Status:\nRED LED: %d\nYELLOW LED: %d\nGREEN LED: %d\nSWITCH 1:%d\n", node->led1, node->led2, node->led3, node->sw1);
		printf("SWITCH 2: %d\nPB4: %d\nPB5: %d\n", node->sw2, node->pb4, node->pb5);
		ptr = ptr->nextevent;
	}

}
