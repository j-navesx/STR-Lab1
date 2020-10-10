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

void initialisePorts();
void myDaemonTaskStartupHook(void);