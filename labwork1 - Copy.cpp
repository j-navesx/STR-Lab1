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

#define mainREGION_1_SIZE 8201
#define mainREGION_2_SIZE 29905
#define mainREGION_3_SIZE 7607

////MOVEMENT DEFINITIONS
//#define LEFT   0x40
//#define RIGHT  0x80
//#define UP     0x8
//#define DOWN   0x4
//#define OUT    0x20
//#define IN     0x10

////SENSORS DEFINITION
//P0 sensors
//#define S_LEFT		!(pos & 0x4)
//#define S_MIDDLE	!(pos & 0x2)
//#define S_RIGHT		!(pos & 0x1)
////P1 sensors
//#define S_1AU		!(pos & 0x4)
//#define S_1AD		!(pos & 0x8)
//#define S_2AU		!(pos & 0x1)
//#define S_2AD		!(pos & 0x2)
////P0 sensors
//#define S_3AU		!(pos & 0x40)
//#define S_3AD		!(pos & 0x80)
//#define B_CENTER	!(pos & 0x10)
//#define B_DELIVER	!(pos & 0x8)
//#define B_OUT		!(pos & 0x20)

//PORT 2
	// DEFAULT VALUE 0000 0000 (8 bits)
	//
	//CONTROL THE CRANE (PORT 2)
	//
	// RIGHT   1<<7  1000 0000  0x80
	// LEFT    1<<6  0100 0000  0x40
	// UP      1<<3  0000 1000  0x8
	// DOWN    1<<2  0000 0100  0x4
	// OUT(I)  1<<5  0010 0000  0x20
	// IN(O)   1<<4  0001 0000  0x10
	//
	//CONTROL LEDS (PORT 2)
	//
	// LEFT   0000 0001  0x1
	// RIGHT  0000 0010  0x2
	//
	//SENSORS (ACTIVE 0)
	// CRANE SENSORS
	//  CENTERED       1110 1111
	//  DELIVERING     1111 0111
	// CRANE POSITION
	//  RIGHT          1111 1110
	//  MIDDLE         1111 1101
	//  LEFT           1111 1011
	//             

//SEMAPHORES
xSemaphoreHandle xSemaphore = NULL;


void vAssertCalled(unsigned long ulLine, const char* const pcFileName){
	static BaseType_t xPrinted = pdFALSE;
	volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;
	/* Called if an assertion passed to configASSERT() fails. See
	http://www.freertos.org/a00110.html#configASSERT for more information. */
	/* Parameters are not used. */
	(void)ulLine;
	(void)pcFileName;
	printf("ASSERT! Line %ld, file %s, GetLastError() %ld\r\n", ulLine, pcFileName, GetLastError());
	taskENTER_CRITICAL();
	{
		/* Cause debugger break point if being debugged. */
		__debugbreak();
		/* You can step out of this function to debug the assertion by using
		the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
		value. */
		while (ulSetToNonZeroInDebuggerToContinue == 0)
		{
			__asm { NOP };
			__asm { NOP };
		}
	}
	taskEXIT_CRITICAL();
}

static void initialiseHeap(void){
	static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
	/* Just to prevent 'condition is always true' warnings in configASSERT(). */
	volatile uint32_t ulAdditionalOffset = 19;
	const HeapRegion_t xHeapRegions[] =
	{
		/* Start address with dummy offsetsSize */
		{ ucHeap + 1,mainREGION_1_SIZE },
		{ ucHeap + 15 + mainREGION_1_SIZE,mainREGION_2_SIZE },
		{ ucHeap + 19 + mainREGION_1_SIZE +
		mainREGION_2_SIZE,mainREGION_3_SIZE },
		{ NULL, 0 }
	};
	configASSERT((ulAdditionalOffset +
		mainREGION_1_SIZE +
		mainREGION_2_SIZE +
		mainREGION_3_SIZE) < configTOTAL_HEAP_SIZE);
	/* Prevent compiler warnings when configASSERT() is not defined. */
	(void)ulAdditionalOffset;
	vPortDefineHeapRegions(xHeapRegions);
}


int getBitValue(uInt8 value, uInt8 bit_n)
// given a byte value, returns the value of its bit n
{
	return(value & (1 << bit_n));
}

void setBitValue(uInt8* variable, int n_bit, int new_value_bit)
// given a byte value, set the n bit to value
{
	uInt8 mask_on = (uInt8)(1 << n_bit);
	uInt8 mask_off = ~mask_on;
	if (new_value_bit) *variable |= mask_on;
	else *variable &= mask_off;
}

// X movement:

void moveXRight() {
	uInt8 pos = readDigitalU8(2);
	setBitValue(&pos, 6, 0);
	setBitValue(&pos, 7, 1);
	writeDigitalU8(2, pos);
}

void moveXLeft() {
	uInt8 pos = readDigitalU8(2);
	setBitValue(&pos, 7, 0);
	setBitValue(&pos, 6, 1);
	writeDigitalU8(2, pos);
}

void stopX() {
	uInt8 pos = readDigitalU8(2);
	setBitValue(&pos, 7, 0);
	setBitValue(&pos, 6, 0);
	writeDigitalU8(2, pos);
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

void gotoX(int xPos) {
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
}

// Y movement:

void moveYIn() {
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 4, 0); // set bit 4 to low level
	setBitValue(&p, 5, 1); //set bit 5 to high level
	writeDigitalU8(2, p); //update port 2
}

void moveYOut() {
	uInt8 p = readDigitalU8(2); // read port 2
	setBitValue(&p, 4, 1); // set bit 4 to high level
	setBitValue(&p, 5, 0); //set bit 5 to low level
	writeDigitalU8(2, p); // update port 2
}

void stopY() {
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 4, 0); // set bit 4 to low level
	setBitValue(&p, 5, 0); //set bit 5 to low level
	writeDigitalU8(2, p); //update port 2
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

void gotoY(int yPos) {
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
}

// Z movement:

void moveZUp() {
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 2, 0); // set bit 2 to low level
	setBitValue(&p, 3, 1); //set bit 3 to high level
	writeDigitalU8(2, p); //update port 2
}

void moveZDown() {
	uInt8 p = readDigitalU8(2); // read port 2
	setBitValue(&p, 2, 1); // set bit 2 to high level
	setBitValue(&p, 3, 0); //set bit 3 to low level
	writeDigitalU8(2, p); // update port 2
}

void stopZ() {
	uInt8 p = readDigitalU8(2); //read port 2
	setBitValue(&p, 2, 0); // set bit 2 to low level
	setBitValue(&p, 3, 0); //set bit 3 to low level
	writeDigitalU8(2, p); //update port 2
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

void gotoZ(int zPos) {
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
}

void gotoZUp() {
	moveZUp();
	while (1) {
		uInt8 p0 = readDigitalU8(0);
		uInt8 p1 = readDigitalU8(1);
		if (!getBitValue(p1, 2) || !getBitValue(p1, 0) || !getBitValue(p0, 6)) {
			stopZ();
			break;
		}
	}
}

void gotoZDown() {
	moveZDown();
	while (1) {
		uInt8 p0 = readDigitalU8(0);
		uInt8 p1 = readDigitalU8(1);
		if (!getBitValue(p1, 3) || !getBitValue(p1, 1) || !getBitValue(p0, 7)) {
			stopZ();
			break;
		}
	}
}

void putPartInCell() {
	uInt8 p = readDigitalU8(1);
	if (getBitValue(p, 4)) {
		gotoZUp();
		gotoY(1);
		gotoZDown();
		gotoY(2);
	}
}

void takePartFromCell() {
	uInt8 p = readDigitalU8(1);
	if (!getBitValue(p, 4)) {
		gotoY(1);
		gotoZUp();
		gotoY(2);
		gotoZDown();
	}
}

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

void initialisePorts() {
	//positions on the x axis (sensors on the bottom activate on passage)
	createDigitalInput(0);
	//postitions in the z axis (sensors on the crane activate on passage)
	createDigitalInput(1);
	//Controls the directions on the crane
	createDigitalOutput(2);

	writeDigitalU8(2, 0);
	resetPos();
}

void vTaskCode_1(void* pvParameters){
	for (;; ) {
		printf("\nHello from TASK_1");
		// Although the kernel is in preemptive mode,
		// we should help switch to another
		// task with e.g. vTaskDelay(0) or taskYELD()
		taskYIELD();
	}
}

void vTaskCode_2(void* pvParameters){
	for (;;)
	{
		printf("\nHello from TASK_2..");
		taskYIELD();
	}
}


void myDaemonTaskStartupHook(void) {
	//xTaskCreate(vTaskCode_2, "vTaskCode_1", 100, NULL, 0, NULL);
	//xTaskCreate(vTaskCode_1, "vTaskCode_2", 100, NULL, 0, NULL);
	initialisePorts();
	xSemaphore = xSemaphoreCreateCounting(10, 0);
}

void main(int argc, char** argv) {
	initialiseHeap();
	vApplicationDaemonTaskStartupHook = &myDaemonTaskStartupHook;
	vTaskStartScheduler();

	writeDigitalU8(2, 0);
	closeChannels();
}