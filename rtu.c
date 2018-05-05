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
int sock, myID, cdev_id, counter = 0;
Event* Head = NULL;
pthread_t t1, cdev_thread, et;
float currentreading;

//Function Prototypes 
void segmentDisplay(int number);
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
void print_list(Event *node);
void send_list(Event *node);

int main(){

	//Variables
	int i, b, var;
	int signalindex = 0; //For checking signal is active
	float pastreadings[10] = {0};
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
		//checkSignal(currentreading, signalindex, pastreadings);	
	}	

	pthread_join(t1, NULL);
	pthread_join(cdev_thread, NULL);
	pthread_join(et, NULL);

	return 0;
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
	newevent->voltage = currentreading;
	newevent->nextevent = NULL;
	clock_gettime(CLOCK_REALTIME, &(newevent->timestamp));

	//Show on the display what event just happened
        if ((int)t < 10){
                segmentDisplay((int)t);
        }


	//Add new event to the linked list 
	counter++;
	add_node(newevent);

	//	print_list(Head);
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

	//open character device 
	if((cdev_id = open(CHAR_DEV, O_RDWR)) == -1){
		printf("\nCannot open device %s", CHAR_DEV);
	}

	//Create the socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("\nError creating socket...\n");
	}

	//Initalize server struct
	bzero(&me, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port = htons(PORT);
	inet_aton("128.206.19.255", &me.sin_addr);	

	//Bind Socket 
	if(bind(sock, (const struct sockaddr *)&me, sizeof(me)) < 0){
		printf("\nError binding socket..");
	}

	//Set socket to Broadcast
	var = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &b, sizeof(b));
	if(var < 0){
		printf("\nError in setsockopt()");
	}
	length = sizeof(struct sockaddr_in);

	//Broadcast message to find any other RTU's
	memset(msg, '\0', MSG_SIZE);
	sprintf(msg, "WHOIS");
	var = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&me, length);
	printf("\nSent the WHOIS\n");

	while(1){
		memset(msg, '\0', MSG_SIZE);
		var = recvfrom(sock, msg, MSG_SIZE, 0, (struct sockaddr *)&server, &length);
		printf("Message: %s\n", msg);			//Deleteme
		if (msg[0] == '~'){
			myID = msg[1] - '0';
			break;
		}
	}	

	printf("\nMy ID number is %d", myID);

	//call thread 1 to constantly check for messages from other RTU's
	pthread_create(&cdev_thread, NULL, (void *)readKM, (void *)&myID);
	pthread_create(&t1, NULL, (void *)thread1, (void *)&myID);	
	pthread_create(&et, NULL, (void *)eventthread, (void *)&myID);
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

	digitalWrite(SegENABLE, 1);

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
		case 10:
			digitalWrite(SegENABLE, 0);
			break;	
	}	

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
		var = recvfrom(sock, message, MSG_SIZE, 0, (struct sockaddr *)&server, &length);
		printf("\nMessage Recieved: %s", message);

		//check if message was recieved properly 	
		;		if (var < 0){					
			printf("\nError Recieving Message..");
		} else {
			//WHOIS received 
			if (strncmp(message, "WHOIS", 5) == 0){
				sprintf(message, "RTU #%d is active", ID);
				var = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&me, length);
			}

			//Command received
			if (strncmp(message, "#", 1) == 0){
				//Sent to me
				if ((message[1] - '0') == ID){	
					switch(message[2] - '0'){
						case 0:
							setLED(REDLED, 1);
							break;
						case 1:
							setLED(REDLED, 0);
							break;
						case 2:
							setLED(YELLOWLED, 1);
							break;
						case 3: 
							setLED(YELLOWLED, 0);
							break;
						case 4:
							setLED(GREENLED, 1);
							break;
						case 5:
							setLED(GREENLED, 0);
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

	timer_value.it_interval.tv_sec = 5;
	timer_value.it_interval.tv_nsec = 0;
	timer_value.it_value.tv_sec = 0;
	timer_value.it_value.tv_nsec = 100;

	timerfd_settime(timer, 0, &timer_value, NULL);
	read(timer, &periods, sizeof(periods));

	while(1){
		printf("\nSending List..");
		send_list(Head);
		free_list(Head);
		read(timer, &periods, sizeof(periods));
		usleep(100);	
	}

	pthread_exit(0);
}

//fucntion that will read in events from the character device and greate an event 
void *readKM(void *ptr){

	char readin[MSG_SIZE], prev;
	int dummy;
	enum eventtypes x;
	printf("\nReading in from Kernel Module...");

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
				//printf("\nButton four event detected");
				event(pb4);
				break;
				//button 5 event detected 
			case '2':
				//printf("\nButton 5 event detected");
				event(pb5);
				break;
				//switch 1 event detected 
			case '3':
				//printf("\nSwitch 1 event detected");
				event(sw1);
				break;
				//switch 2 event detected 
			case '4':
				//printf("\nSwitch 2 event detected");
				event(sw2);
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

	if(Head == NULL){
		Head = add;

	} else {
		//create pointer to the head 
		Event *ptr = Head;

		//while loop to find the end of the linked list 
		while(ptr->nextevent != NULL){
			ptr = ptr->nextevent;
		}

		//when loop breaks we want to add the new node 
		ptr->nextevent = add;
		add->nextevent = NULL;
	}
}

//function to call when we want to free the list 
void free_list(Event *node){

	Event *oldnode, *head = node, *cnode;

	//Base Case 
	//if (node == NULL){
	//	return;
	//}

	//if(node->nextevent == NULL){
	//	free(node);	
	//} else{
	//	free_list(node->nextevent);
	//	free(node);
	//}

	if (head == NULL){
		return;
		printf("\nHead is NULL, exiting\n\n");
	} else {
		while (head->nextevent != NULL){
			cnode = head;
			oldnode = cnode;
			while (cnode->nextevent != NULL){
				oldnode = cnode;
				cnode = cnode->nextevent;
			}

			if (oldnode != cnode){
				oldnode->nextevent = NULL;
			} else {
				head->nextevent = NULL;
			}
			free(cnode);
		}
	}		
}

void print_list(Event *node){
	Event *ptr = node;

	while(ptr->nextevent != NULL){
		printf("\nEvent #%d on RTU #%d at time %ld.%ld", ptr->type, ptr->rtuID, ptr->timestamp.tv_sec, ptr->timestamp.tv_nsec);
		printf("Pin Status:\nRED LED: %d\nYELLOW LED: %d\nGREEN LED: %d\nSWITCH 1:%d\n", ptr->led1, ptr->led2, ptr->led3, ptr->sw1);
		printf("SWITCH 2: %d\nPB4: %d\nPB5: %d\n", ptr->sw2, ptr->pb4, ptr->pb5);
		ptr = ptr->nextevent;
	}
}

//this function will send the structure through the socket to the historian
void send_list(Event *node){

	Event *cnode = node;
	int var;
	char emsg[MSG_SIZE];
	struct timespec ctime;

	if(node != NULL){
		sprintf(emsg, "$%d|%d%d%d%d%d%d%d|%02d|%.04f|%ld.%ld ", cnode->rtuID, cnode->led1, cnode->led2, cnode->led3,
				cnode->sw1, cnode->sw2, cnode->pb4, cnode->pb5, cnode->type, cnode->voltage, cnode->timestamp.tv_sec,
				cnode->timestamp.tv_nsec);
		printf("\n%s", emsg);
		while (cnode->nextevent != NULL){
			var = sendto(sock, emsg, MSG_SIZE, 0, (struct sockaddr *)&me, length); 
			cnode = cnode->nextevent;
			sprintf(emsg, "$%d|%d%d%d%d%d%d%d|%02d|%.04f|%ld.%ld ", cnode->rtuID, cnode->led1, cnode->led2, cnode->led3,
					cnode->sw1, cnode->sw2, cnode->pb4, cnode->pb5, cnode->type, cnode->voltage, cnode->timestamp.tv_sec,
					cnode->timestamp.tv_nsec);		
		}
		var = sendto(sock, emsg, MSG_SIZE, 0, (struct sockaddr *)&me, length);
	} else {
		clock_gettime(CLOCK_REALTIME, &ctime);
		sprintf(emsg, "$%d|%d%d%d%d%d%d%d|%02d|%.04f|%ld.%ld ", myID, getPinStatus(REDLED), getPinStatus(YELLOWLED), getPinStatus(GREENLED),
                                getPinStatus(SW1), getPinStatus(SW2), getPinStatus(PB4), getPinStatus(PB5), 10, getADC(), ctime.tv_sec, ctime.tv_nsec);
		printf("\nNo events - %s\n", emsg);
		var = sendto(sock, emsg, MSG_SIZE, 0, (struct sockaddr *)&me, length);
		segmentDisplay(10);
	}
}


