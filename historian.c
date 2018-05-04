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

//Definitions
#define MSG_SIZE 40
#define PORT 4000

//Function Prototypes
void setup();
void sendmessage(char*);
void *receivethread(void *ptr);

//Global Variables
struct sockaddr_in me, server;
socklen_t length;
int sock, connections = 0;
char msg[MSG_SIZE];

char *commands[] = {"red LED on", "red LED off", "yellow LED on", "yellow LED off", "blue LED on", "blue LED off"};



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

		printf("\nChange an LED - 1");
		printf("\nExit - 0");
		printf("\nPlease enter a command: ");
		scanf("%d", &option);

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
				printf("\nTurn on blue LED - 5");
                                printf("\nTurn off blue LED - 6");
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
				buff[1] = (rtu - 48);	//Single digit int to char conversion

				//Send the command
				sendmessage(buff);

				
				break;
			default:
				printf("\nError, invalid option!");
		}

	}
	



	return 0;
}


void sendmessage(char* msg){
	int a;

	a = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&me, length);
}

void setup(){
	
	int var, b;

	//Create a socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("\nError creating socket...\n");
	}

	//Initalize server struct
	bzero(&server, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(PORT);
	inet_aton("128.206.19.255", &me.sin_addr);

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
}

void *receivethread(void *ptr){
	int var;
	char message[MSG_SIZE];

	while (1){
		var = recvfrom(sock, message, MSG_SIZE, 0, (struct sockaddr *)&me, &length);
		
		if (strncmp(message, "WHOIS", 5) == 0){
			memset(message, '\0', MSG_SIZE);
			sprintf(message, "~%d", connections++);
			var = sendto(sock, message, strlen(message), 0, (struct sockaddr *)&me, length);
		}
		

	}

}
