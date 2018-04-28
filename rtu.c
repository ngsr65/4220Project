#include <wiringPi.h>
#include <stdio.h>
#include <stdint.h>

#define SegENABLE 21
#define SegDP     23
#define SegA      24
#define SegB      25
#define SegC      26
#define SegD      27


void segmentDisplay(int number);
uint16_t readADC();

int main(){
	
	wiringPiSetupGpio();
	wiringPiSPISetup(0, 1000000);
	

	//7 Segment Display
	pinMode(SegENABLE, OUTPUT);
	pinMode(SegDP, OUTPUT);
	pinMode(SegA, OUTPUT);
	pinMode(SegB, OUTPUT);
	pinMode(SegC, OUTPUT);
	pinMode(SegD, OUTPUT);

	
	//ADC
	while (1){

		printf("Data - %d\n", readADC());
		segmentDisplay(2);
	}	
	
	
	
}

uint16_t readADC(){

	uint16_t received = 0;
	uint8_t data[3] = {
		0x01,	//Start Byte
		0xA0,	//Channel 2
		0x00    //Empty Byte
	};

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
