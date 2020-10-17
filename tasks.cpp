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

typedef struct {
	int xcord;
	int zcord;
}Coords;

//MAILBOXES
xQueueHandle mbx_keyb;
xQueueHandle mbx_request;
xQueueHandle mbx_mov;
xQueueHandle mbx_xMov;
xQueueHandle mbx_zMov;

//SEMAPHORES

xSemaphoreHandle sem_xzMov;
xSemaphoreHandle sem_xMov;
xSemaphoreHandle sem_zMov;


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

void kybControl(void * pvParameters) {

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
			xSemaphoreTake(sem_xzMov, portMAX_DELAY);
			xQueueSend(mbx_mov, &coords, 0);

		}
		if (!comm.compare("exit")) {
			exit(0);
		}
	}
}

/*void gridMovement(void* pvParameters) {
	while (true) {
		int coords[2];
		xQueueReceive(mbx_mov, &coords, portMAX_DELAY);
		printf("\n%d,%d\n", coords[0], coords[1]);
	}
	//Wait for msg from addStock or takeStock()
	//msg xMovement and zMovement()
	xSemaphoreGive(sem_xzMov);
}*/

void gotoXZ(void* pvParameters) {
	int x, z;
	int coords[2];
	while (true) {
		xQueueReceive(mbx_mov, &coords, portMAX_DELAY);
		printf("\n%d,%d\n", coords[0], coords[1]);

		x = coords[0];
		z = coords[1];

		xQueueSend(mbx_xMov, &x, portMAX_DELAY);
		xQueueSend(mbx_zMov, &z, portMAX_DELAY);

		//wait for goto_x completion, synchronization
		xSemaphoreTake(sem_xMov, portMAX_DELAY);
		//wait for goto_z completion, synchronization
		xSemaphoreTake(sem_zMov, portMAX_DELAY);

		xSemaphoreGive(sem_xzMov, portMAX_DELAY);
	}
}

void gotoX(void* pvParameters) {
	int x;
	int crtpos;
	while (true) {
		xQueueReceive(mbx_xMov, &x, portMAX_DELAY);
		crtpos = getXPos();

		if (crtpos < x)
			moveXRight();
		else
			if (crtpos > x)
				moveXLeft();
		while (getXPos() != x) {
			taskYIELD();
		}
		stopX();
		xSemaphoreGive(sem_xMov, portMAX_DELAY);
	}
}

void gotoZ(void* pvParameters) {
	int z;
	int crtpos;
	while (true) {
		xQueueReceive(mbx_zMov, &z, portMAX_DELAY);
		crtpos = getZPos();

		if (crtpos < z)
			moveZUp();
		else
			if (crtpos > z)
				moveZDown();
		while (getZPos() != z) {
			taskYIELD();
		}
		stopZ();
		xSemaphoreGive(sem_zMov, portMAX_DELAY);
	}
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
	mbx_mov = xQueueCreate(1, sizeof(Coords));
	mbx_xMov = xQueueCreate(1, sizeof(int));
	mbx_zMov = xQueueCreate(1, sizeof(int));
	sem_xzMov = xSemaphoreCreateCounting(1, 1);
	sem_xMov = xSemaphoreCreateCounting(1, 0);
	sem_zMov = xSemaphoreCreateCounting(1, 0);
	
	//xTaskCreate(vTaskCode_2, "vTaskCode_1", 100, NULL, 0, NULL);
	//xTaskCreate(vTaskCode_1, "vTaskCode_2", 100, NULL, 0, NULL);
	xTaskCreate(cmd, "cmd", 100, NULL, 0, NULL);
	xTaskCreate(gotoXZ, "gotoXZ", 100, NULL, 0, NULL);
	xTaskCreate(gotoX, "gotoX", 100, NULL, 0, NULL);
	xTaskCreate(gotoZ, "gotoZ", 100, NULL, 0, NULL);
	initialisePorts();
}