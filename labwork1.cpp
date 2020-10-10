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
#include <movement.h>
#include <tasks.h>

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

void main(int argc, char** argv) {
	initialiseHeap();
	vApplicationDaemonTaskStartupHook = &myDaemonTaskStartupHook;
	vTaskStartScheduler();

	writeDigitalU8(2, 0);
	closeChannels();
}