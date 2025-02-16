#include<conio.h>
#include<iostream>
#include<stdlib.h>
#include <windows.h> //for Sleep function
extern "C" {
#include <interface.h>
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <semphr.h>
#include <interrupts.h>
}

#include <functions.h>

#define mainREGION_1_SIZE 8201
#define mainREGION_2_SIZE 29905
#define mainREGION_3_SIZE 7607

// X movement:

void moveXRight()
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 6, 0); // set bit 6 to low level
	setBitValue(&p, 7, 1); //set bit 7 to high level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void moveXLeft()
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); // read port 2
	setBitValue(&p, 6, 1); // set bit 6 to high level
	setBitValue(&p, 7, 0); //set bit 7 to low level
	writeDigitalU8(2, p); // update port 2
	taskEXIT_CRITICAL();
}

void stopX()
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 6, 0); // set bit 6 to low level
	setBitValue(&p, 7, 0); //set bit 7 to low level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

int getXPos() {
	uInt8 p0 = readDigitalU8(0);
	if (!getBitValue(p0, 2)) {
		return 1;
	}
	if (!getBitValue(p0, 1)) {
		return 2;
	}
	if (!getBitValue(p0, 0)) {
		return 3;
	}
	else {
		return -1;
	}
}

/*void gotoX(int xPos) {
	int currPos = getXPos();
	printf("CURRENT POS %d\n", currPos);
	if (currPos > xPos) {
		moveXLeft();
	}
	else if (currPos < xPos) {
		moveXRight();
	}
	while (true) {
		if (currPos == xPos) {
			stopX();
			break;
		}
	}
	printf("STOPPED AT %d\n\n", xPos);
}*/

// Y movement:

void moveYIn() {
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 4, 0); // set bit 4 to low level
	setBitValue(&p, 5, 1); //set bit 5 to high level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void moveYOut() {
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); // read port 2
	setBitValue(&p, 4, 1); // set bit 4 to high level
	setBitValue(&p, 5, 0); //set bit 5 to low level
	writeDigitalU8(2, p); // update port 2
	taskEXIT_CRITICAL();
}

void stopY() {
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 4, 0); // set bit 4 to low level
	setBitValue(&p, 5, 0); //set bit 5 to low level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

int getYPos() {
	uInt8 p0 = readDigitalU8(0);
	if (!getBitValue(p0, 3)) {
		return 1;
	}
	if (!getBitValue(p0, 4)) {
		return 2;
	}
	if (!getBitValue(p0, 5)) {
		return 3;
	}
	else {
		return -1;
	}
}

/*void gotoY(int yPos) {
	int currPos = getYPos();
	printf("CURRENT POS %d\n", currPos);
	if (currPos > yPos) {
		moveYIn();
	}
	else if (currPos < yPos) {
		moveYOut();
	}
	while (true) {
		if (currPos == yPos) {
			stopY();
			break;
		}
	}
	printf("STOPPED AT %d\n\n", yPos);
}*/

// Z movement:

void moveZUp()
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 2, 0); // set bit 2 to low level
	setBitValue(&p, 3, 1); //set bit 3 to high level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

void moveZDown()
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); // read port 2
	setBitValue(&p, 2, 1); // set bit 2 to high level
	setBitValue(&p, 3, 0); //set bit 3 to low level
	writeDigitalU8(2, p); // update port 2
	taskEXIT_CRITICAL();
}

void stopZ()
{
	taskENTER_CRITICAL();
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 2, 0); // set bit 2 to low level
	setBitValue(&p, 3, 0); //set bit 3 to low level
	writeDigitalU8(2, p); //update port 2
	taskEXIT_CRITICAL();
}

int getZPos() {
	uInt8 p0 = readDigitalU8(0);
	uInt8 p1 = readDigitalU8(1);
	if (!getBitValue(p1, 3)) {
		return 1;
	}
	if (!getBitValue(p1, 1)) {
		return 2;
	}
	if (!getBitValue(p0, 7)) {
		return 3;
	}
	else {
		return(-1);
	}
}

/*void gotoZ(int zPos) {
	int currPos = getZPos();
	printf("CURRENT POS %d\n", currPos);
	if (currPos > zPos) {
		moveZDown();
	}
	else if (currPos < zPos) {
		moveZUp();
	}
	while (true) {
		if (currPos == zPos) {
			stopZ();
			break;
		}
	}
	printf("STOPPED AT %d\n\n", zPos);
}*/

/*void gotoZUp() {
	moveZUp();
	while (1) {
		uInt8 p0 = readDigitalU8(0);
		uInt8 p1 = readDigitalU8(1);
		if (!getBitValue(p1, 2) || !getBitValue(p1, 0) || !getBitValue(p0, 6)) {
			stopZ();
			break;
		}
	}
}*/

/*void gotoZDown() {
	moveZDown();
	while (1) {
		uInt8 p0 = readDigitalU8(0);
		uInt8 p1 = readDigitalU8(1);
		if (!getBitValue(p1, 3) || !getBitValue(p1, 1) || !getBitValue(p0, 7)) {
			stopZ();
			break;
		}
	}
}*/

// RESET:
void resetPos() {
	moveXRight();
	while (true) {
		if (getXPos() == 2) {
			stopX();
			break;
		}
	}
	printf("RESET\n");
}