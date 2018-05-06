//Included Libraries
#include <errno.h>
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

//Definitions
#define MSG_SIZE 40
#define PORT 4000

//Event enum
enum eventtypes{sigoff, sighigh, siglow, led1, led2, led3, sw1, sw2, pb4, pb5, noevent};

//Event struct
struct eventvar{
	int rtuID, led1, led2, led3, sw1, sw2, pb4, pb5;
	enum eventtypes type;
	float voltage;
	struct timespec timestamp;
	uint64_t time;
	struct eventvar *nextevent;
}typedef Event;

//Global Variables
struct sockaddr_in me;
socklen_t length;
int sock, connections = 0, file_write = 41;
char msg[MSG_SIZE];
char *commands[] = {"red LED on", "red LED off", "yellow LED on", "yellow LED off", "green LED on", "green LED off"};
Event *head;
FILE *fPtr;

//Function Prototypes
void setup();
void sendmessage(char*);
void *receivethread(void *ptr);
void add_node(Event *add);
void free_list(Event *node);

int main(){

	//Variables
	char buff[40];	
	int option = 0, i, running = 1, rtu, command;
	pthread_t t1;

	//Run setup function
	setup();

	//Create a thread for receiving information and sending IDs to new RTUs
	pthread_create(&t1, NULL, (void *)receivethread, (void *)&i);

	while(running == 1){

		printf("\nFile Output - 2");
		printf("\nChange an LED - 1");
		printf("\nExit - 0");
		printf("\nPlease enter a command: ");
		scanf("%d", &option);
		fflush(stdin);		
		
		switch (option){
			case 0:
				running = 0;
				break;
			case 1:
				printf("\nPlease enter the RTU ID you wish to command: ");
				scanf("%d", &rtu);
				printf("\nTurn on red LED - 1");
				printf("\nTurn off red LED - 2");
				printf("\nTurn on yellow LED - 3");
				printf("\nTurn off yellow LED - 4");
				printf("\nTurn on green LED - 5");
				printf("\nTurn off green LED - 6");
				printf("\nPlease enter the command: ");
				scanf("%d", &command);

				switch (command){
					case 1:
						buff[2] = '0';
						break;
					case 2:
						buff[2] = '1';
						break;
					case 3:
						buff[2] = '2';
						break;
					case 4:
						buff[2] = '3';
						break;
					case 5:
						buff[2] = '4';
						break;
					case 6:
						buff[2] = '5';
						break;
					default: 
						printf("Error, invalid command");
						break;
				}

				printf("\nTelling RTU #%d to turn the %s.", rtu, commands[command - 1]);

				buff[0] = '#';
				buff[1] = rtu + '0';	//Single digit int to char conversion

				//Send the command
				sendmessage(buff);


				break;
			case 2:
				fPtr = fopen("output.txt", "w");
        			if(fPtr == NULL){
                			printf("\nThe file output.txt could not be opened please try again..");
        			}
				file_write = 0;
				break;
			default:
				printf("\nError, invalid option!");
		}

	}

		
	free_list(head);
	printf("\nEnding program");
	return 0;
}


void sendmessage(char* msg){
	int a;

	//a = write(sock, msg, MSG_SIZE);
	a = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&me, length);
}

void setup(){

	int var, b = 1;

	//Create a socket
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

//	var = sendto(sock, "TEST", strlen("TEST"), 0, (struct sockaddr *)&me, length);
//	printf("Write status - %d --- %d\n", var, errno);
}

void *receivethread(void *ptr){
	int var;
	Event *new;

	while (1){
		//var = recvfrom(sock, message, MSG_SIZE, 0, (struct sockaddr *)&me, &length);
		var = read(sock, msg, MSG_SIZE);
		printf("Received: %s\n", msg); 		//Deleteme		
		if (strncmp(msg, "WHOIS", 5) == 0){
			printf("Got the WHOIS\n");
			memset(msg, '\0', MSG_SIZE);
			sprintf(msg, "~%d", connections++);
			inet_aton("128.206.19.255", &me.sin_addr);
			var = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&me, length);
			//var = write(sock, msg, MSG_SIZE);
		}
		else if(msg[0] == '$'){
			//malloc space for a new node in the list 
			new = (Event *)malloc(sizeof(Event));		
			//the first element corresponds to the RTU ID
			new->rtuID = msg[1] - '0';
			//the next sevent elements correspond to the status if the LED's, buttons and switches 
			new->led1 = msg[3] - '0';
			new->led2 = msg[4] - '0';
			new->led3 = msg[5] - '0';
			new->sw1 = msg[6] - '0';
			new->sw2 = msg[7] - '0';
			new->pb4 = msg[8] - '0';
			new->pb5 = msg[9] - '0';
			//the eleventh element is the type number in the enum 		
			new->type = atoi(msg +11);	
			//then get the voltage as a float number 
			new->voltage = atof(msg + 14);
			//first get the seconds 
			new->time = atoi(msg + 21);
			//multiply it by 10^8 
			new->time = new->time * 100000000;
			//add the 8 decimal places of nanoseconds 
			new->time = new->time + atoi(msg +32);
			//add the new node to the list 
			add_node(new);

			//check for the file writing flag 
			if(file_write < 40){
				printf("\nWriting to file %d: %.04f	%ld\n", file_write, new->voltage, new->time);
				fprintf(fPtr, "\n%.04f	%ld", new->voltage, new->time);
				file_write++;
				if(file_write >= 40){
					fclose(fPtr);	
				}
			}

		}
	}
}
//This function will add an event to the end of our linked list 
void add_node(Event *add){
	//check that the list exists 
	if(head == NULL){
		head = add;
		head->nextevent = NULL;
	}
	else{
		//create pointer to the head 
		 Event *ptr = head;

		//while loop to find the end of the linked list 
		while(ptr->nextevent != NULL ){//| ptr->time < add->time){
			ptr = ptr->nextevent;
		}

		//when loop breaks we want to add the new node 
		ptr->nextevent = add;
		add->nextevent = NULL;
	}
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

        while(ptr != NULL){
                printf("\nEvent #%d on RTU #%d at time %ld.%ld", ptr->type, ptr->rtuID, ptr->timestamp.tv_sec, ptr->timestamp.tv_nsec);
                printf("Pin Status:\nRED LED: %d\nYELLOW LED: %d\nGREEN LED: %d\nSWITCH 1:%d\n", ptr->led1, ptr->led2, ptr->led3, ptr->sw1);
                printf("SWITCH 2: %d\nPB4: %d\nPB5: %d\n", ptr->sw2, ptr->pb4, ptr->pb5);
                ptr = ptr->nextevent;
        }
}
