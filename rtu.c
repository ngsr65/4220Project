//Included Libraries
#include <wiringPi.h>
#include <stdio.h>
#include <stdint.h>

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

//Function Prototypes
void segmentDisplay(int number);
uint16_t readADC();
void setLED(int number, int onoff);
int getPinStatus(int number);
void checkSignal(int currentreading, int currentreadingindex, int* pastreadings);
void setup();

//Global Variables
nopowerflag = 0;


int main(){
	
	//Variables
	int i;
	int pastreadings[10] = {0}, signalindex = 0; //For checking signal is active
	int currentreading;
	
	//Run setup functions
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
