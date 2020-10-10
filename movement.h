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

void moveXRight();
void moveXLeft();
void stopX();
int getXPos();
void gotoX(int xPos);

// Y movement:

void moveYIn();
void moveYOut();
void stopY();
int getYPos();
void gotoY(int yPos);

// Z movement:

void moveZUp();
void moveZDown();
void stopZ();
int getZPos();
void gotoZ(int zPos);
void gotoZUp();
void gotoZDown();
void putPartInCell();
void takePartFromCell();

// RESET:

void resetPos();