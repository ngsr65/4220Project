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

/*Event type reference
00 - No signal
01 - Signal too high
02 - Signal too low
03 - LED1 state change
04 - LED2 state change
05 - LED3 state change
06 - Toggle switch 1 state change
07 - Toggle switch 2 state change
08 - Push button 4 state change
09 - Push button 5 state change
10 - No event
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
#include <errno.h>

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
enum eventtypes{sigoff, sighigh, siglow, led1, led2, led3, sw1, sw2, pb4, pb5, noevent};

//Global Variables
struct sockaddr_in me, server;
socklen_t length;
int sock, myID, cdev_id, counter = 0, sigoffflag = 0, sighighflag = 0, siglowflag = 0;
pthread_t t1, cdev_thread, et;
float currentreading;
char data[1600];


//Function Prototypes 
void segmentDisplay(int number);
float getADC();
void setLED(int number, int onoff);
int getPinStatus(int number);
void checkSignal(float currentreading);
void setup();
void *readKM(void *ptr);
void *thread1(void *ptr);
void *eventthread(void *ptr);
void event(enum eventtypes t);

int main(){

	//Run setup operations
	setup();

	//Main program loop
	while (1){

		//Get the current voltage value
		currentreading = getADC();

		//Check the signal
		checkSignal(currentreading);	
	}	

	pthread_join(t1, NULL);
	pthread_join(cdev_thread, NULL);
	pthread_join(et, NULL);

	return 0;
}

void event(enum eventtypes t){

	struct timespec ctime;
	clock_gettime(CLOCK_REALTIME, &ctime);


	printf("\nEvent %d just occured!\n", (int)t);

	//Display the event that just occured on the seven segment display
        if ((int)t < 10){
                segmentDisplay((int)t);
        }


	//Add new event to the array
	snprintf(data + (counter * 40), 40, "$%d|%d%d%d%d%d%d%d|%02d|%.04f|%ld.%ld", myID, getPinStatus(REDLED), getPinStatus(YELLOWLED), getPinStatus(GREENLED), getPinStatus(SW1), getPinStatus(SW2), getPinStatus(PB4), getPinStatus(PB5), (int)t, getADC(), ctime.tv_sec, ctime.tv_nsec);

	//Increment the counter
	counter++;

	//printf("Counter is now %d\n", counter);
}

void setup(){
	//Variables
	char msg[MSG_SIZE];
	int b, var;

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

	//Broadcast message to find historian
	memset(msg, '\0', MSG_SIZE);
	sprintf(msg, "WHOIS");
	var = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&me, length);

	while(1){
		memset(msg, '\0', MSG_SIZE);
		var = recvfrom(sock, msg, MSG_SIZE, 0, (struct sockaddr *)&server, &length);
		if (msg[0] == '~'){
			myID = msg[1] - '0';
			break;
		}
	}	

	printf("\nID number is %d", myID);

	//Call readKM thread to read in events from the kernel module
	//Call thread 1 to constantly check for messages from the historian
	//Call eventthread to send events to the historian
	pthread_create(&cdev_thread, NULL, (void *)readKM, (void *)&myID);
	pthread_create(&t1, NULL, (void *)thread1, (void *)&myID);	
	pthread_create(&et, NULL, (void *)eventthread, (void *)&myID);
}

void checkSignal(float currentreading){
	int i;

	//Check for no power
	if (currentreading < 0.1){
		if (sigoffflag == 0){
			event(sigoff);
			sigoffflag = 1;
		}
	} else {
		if (sigoffflag == 1){
			event(sigoff);
			sigoffflag = 0;
		}
	}

	//Check for out of bounds
	if (currentreading > 2.0){
		if (sighighflag == 0){
			event(sighigh);
			sighighflag = 1;
		}
	} else {
		if (sighighflag == 1){
			sighighflag = 0;
			event(sighigh);
		}
	}

	if (currentreading < 1.0){
		if (siglowflag == 0){
			event(siglow);
			siglowflag = 1;	
		}
	} else {
		if (siglowflag == 1){
			siglowflag = 0;
			event(siglow);
		}
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
	uint8_t data2[3] = {
		0x01,	//Start Byte
		0xA0,	//Channel 2
		0x00    //Empty Byte
	};

	//Send and recieve data over the SPI bus
	wiringPiSPIDataRW(0, data2, 3);

	//00000000 000000XX
	received = data2[1];

	//000000XX 00000000
	received = received << 8;

	//000000XX XXXXXXXX
	received = received | data2[2];

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

	digitalWrite(SegA, a);
	digitalWrite(SegB, b);
	digitalWrite(SegC, c);
	digitalWrite(SegD, d);
}

void *thread1(void *ptr){
	char message[MSG_SIZE];
	int ID = *((int *)ptr), var;

	while(1){
		memset(message,'\0', 40);			
		var = recvfrom(sock, message, MSG_SIZE, 0, (struct sockaddr *)&server, &length);

		//check if message was recieved properly 	
		if (var < 0){					
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
						default:
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

	//Initialize as a real time task
	param.sched_priority = 55;
	sched_setscheduler(0, SCHED_FIFO, &param);

	timer_value.it_interval.tv_sec = 1;
	timer_value.it_interval.tv_nsec = 0;
	timer_value.it_value.tv_sec = 2;
	timer_value.it_value.tv_nsec = 100;

	timerfd_settime(timer, 0, &timer_value, NULL);
	read(timer, &periods, sizeof(periods));

	while(1){
		int i2, var;

		//Make sure something is always sent each second
		if (counter == 0){
			event(noevent);
		}
		
        	for (i2 = 0; i2 < counter; i2++){
                	var = sendto(sock, data + (i2 * 40), MSG_SIZE, 0, (struct sockaddr *)&me, length);
        	}

		//Reset counter
		counter = 0;

		read(timer, &periods, sizeof(periods));	
	}

	pthread_exit(0);
}

//fucntion that will read in events from the character device and generate an event 
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
			printf("\nReading in from character device failed...");
		}

		switch(readin[0]){
			case '1':
				event(pb4);
				break;
			case '2':
				event(pb5);
				break;
			case '3':
				event(sw1);
				break;
			case '4':
				event(sw2); 
				break;
			default:
				break;
		}
	}

	pthread_exit(0);
}


