#include<conio.h>
#include<iostream>
#include<stdlib.h>
#include<string>
#include<cstring>
#include <windows.h> //for Sleep function
extern "C" {
#include <interface.h>
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <queue.h>
#include <semphr.h>
#include <interrupts.h>
}

using namespace std;

#include <functions.h>
#include <movement.h>

typedef struct {
	int reference;
	int entryDate;
	int expiration;
}StoreRequest;

//MAILBOXES

xQueueHandle request;
xQueueHandle mov;
xQueueHandle xMov;
xQueueHandle zMov;

//SEMAPHORES

xSemaphoreHandle xzMov;


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

void cmd(void * pvParameters) {
	
	while (true) {
		
		//system("cls");
		string comm;
		printf("\nInput your function:\n\t:");
		cin >> comm;
		if (!comm.compare("help")){
			printf("\n\tCommands available:\n");
			printf("-");
		}
		if (!comm.compare("goto")) {
			int coords[2];
			printf("Coords:\n\t:");
			scanf("%*c");
			scanf("(%d,%d)", &coords[0], &coords[1]);
			xSemaphoreTake(xzMov, portMAX_DELAY);
			xQueueSend(mov, &coords, 0);

		}
		if (!comm.compare("exit")) {
			exit(0);
		}
	}
}

void gridMovement(void* pvParameters) {
	while (true) {
		int coords[2];
		xQueueReceive(mov, &coords, portMAX_DELAY);
		printf("\n%d,%d\n", coords[0], coords[1]);
	}
	//Wait for msg from addStock or takeStock()
	Sleep(4000);
	//msg xMovement and zMovement()
	xSemaphoreGive(xzMov);
}

void xMovement() {

	//Wait for msg from gridMovement
	gotoX(0);
}

void zMovement() {
	//Wait for msg from gridMovement
	gotoZ(0);
}

void gotoDock() {

}

void addStock() {

	//Msg gridMovement
	putPartInCell();

}

void takeStock() {

	//Msg gridMovement
	takePartFromCell();

}

void myDaemonTaskStartupHook(void) {
	mov = xQueueCreate(1, 2 * sizeof(int));
	xzMov = xSemaphoreCreateCounting(1, 1);
	
	//xTaskCreate(vTaskCode_2, "vTaskCode_1", 100, NULL, 0, NULL);
	//xTaskCreate(vTaskCode_1, "vTaskCode_2", 100, NULL, 0, NULL);
	xTaskCreate(cmd, "cmd", 100, NULL, 0, NULL);
	xTaskCreate(gridMovement, "gridMovement", 100, NULL, 0, NULL);
	initialisePorts();
}