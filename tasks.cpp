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

typedef struct {
	xQueueHandle mbx_xMov;
	xSemaphoreHandle sem_xMov;
}xMov_param;

typedef struct {
	xQueueHandle mbx_zMov;
	xSemaphoreHandle sem_zMov;
}zMov_param;

typedef struct {
	xQueueHandle mbx_xzMov;
	xSemaphoreHandle sem_xzMov;
	xSemaphoreHandle sync_xzMov;
}xzCom_param;

typedef struct {
	xzCom_param* xzCom_params;
	xMov_param* xMov_params;
	zMov_param* zMov_params;
}taskXZ_param;

//Task Comunication params
typedef struct {
	//Will expand during development
	xzCom_param* xzCom_params;
}cmd_param;

//MAILBOXES
xQueueHandle mbx_keyb;
xQueueHandle mbx_request;


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
	
	//cmd needed parameters 
	cmd_param* cmd_params = (cmd_param*)pvParameters;

	//XZ comunication parameters 
	xzCom_param* xzCom_params = cmd_params->xzCom_params;
	xQueueHandle mbx_xzMov = xzCom_params->mbx_xzMov;
	xSemaphoreHandle sem_xzMov = xzCom_params->sem_xzMov;
	xSemaphoreHandle sync_xzMov = xzCom_params->sync_xzMov;

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
			xQueueSend(mbx_xzMov, &coords, 0);
			xSemaphoreGive(sync_xzMov);

		}
		if (!comm.compare("info")) {
			while (true) {
				system("cls");
				printf("Hello World\n");
			}
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
	
	//XZ needed parameters 
	taskXZ_param* gotoXZ_params = (taskXZ_param*)pvParameters;
	
	//XZ comunication parameters 
	xzCom_param* xzCom_params = gotoXZ_params->xzCom_params;
	xQueueHandle mbx_xzMov = xzCom_params->mbx_xzMov;
	xSemaphoreHandle sem_xzMov = xzCom_params->sem_xzMov;
	xSemaphoreHandle sync_xzMov = xzCom_params->sync_xzMov;

	//X movement parameters 
	xMov_param* xMov_params = gotoXZ_params->xMov_params;
	xQueueHandle mbx_xMov = xMov_params->mbx_xMov;
	xSemaphoreHandle sem_xMov = xMov_params->sem_xMov;

	//Z movement parameters 
	zMov_param* zMov_params = gotoXZ_params->zMov_params;
	xQueueHandle mbx_zMov = zMov_params->mbx_zMov;
	xSemaphoreHandle sem_zMov = zMov_params->sem_zMov;

	while (true) {
		xSemaphoreTake(sync_xzMov, portMAX_DELAY);
		xQueueReceive(mbx_xzMov, &coords, portMAX_DELAY);
		printf("\n%d,%d\n", coords[0], coords[1]);

		x = coords[0];
		z = coords[1];

		xQueueSend(mbx_xMov, &x, portMAX_DELAY);
		xQueueSend(mbx_zMov, &z, portMAX_DELAY);

		//wait for goto_x completion, synchronization
		xSemaphoreTake(sem_xMov, portMAX_DELAY);
		//wait for goto_z completion, synchronization
		xSemaphoreTake(sem_zMov, portMAX_DELAY);

		xSemaphoreGive(sem_xzMov);
	}
}

void gotoX(void* pvParameters) {
	int x;
	int crtpos;

	xMov_param* xMov_params = (xMov_param*)pvParameters;
	xQueueHandle mbx_xMov = xMov_params->mbx_xMov;
	xSemaphoreHandle sem_xMov = xMov_params->sem_xMov;


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
		xSemaphoreGive(sem_xMov);
	}
}

void gotoZ(void* pvParameters) {
	int z;
	int crtpos;
	
	zMov_param* zMov_params = (zMov_param*)pvParameters;
	xQueueHandle mbx_zMov = zMov_params->mbx_zMov;
	xSemaphoreHandle sem_zMov = zMov_params->sem_zMov;

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
		xSemaphoreGive(sem_zMov);
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
	StoreRequest grid[3][3];
	fill(*grid, *grid + 9, NULL);

	xzCom_param* my_xzCom_param = (xzCom_param*)pvPortMalloc(sizeof(xzCom_param));
	my_xzCom_param->mbx_xzMov = xQueueCreate(1, sizeof(Coords));
	my_xzCom_param->sem_xzMov = xSemaphoreCreateCounting(1, 1);
	my_xzCom_param->sync_xzMov = xSemaphoreCreateCounting(1, 0);

	zMov_param* my_zMov_param = (zMov_param*)pvPortMalloc(sizeof(zMov_param));
	my_zMov_param->mbx_zMov = xQueueCreate(1, sizeof(int));
	my_zMov_param->sem_zMov = xSemaphoreCreateCounting(1, 0);

	xMov_param* my_xMov_param = (xMov_param*)pvPortMalloc(sizeof(xMov_param));
	my_xMov_param->mbx_xMov = xQueueCreate(1, sizeof(int));
	my_xMov_param->sem_xMov = xSemaphoreCreateCounting(1, 0);
	
	taskXZ_param* my_taskXZ_param = (taskXZ_param*)pvPortMalloc(sizeof(taskXZ_param));
	my_taskXZ_param->xzCom_params = my_xzCom_param;
	my_taskXZ_param->xMov_params = my_xMov_param;
	my_taskXZ_param->zMov_params = my_zMov_param;

	cmd_param* my_cmd_param = (cmd_param*)pvPortMalloc(sizeof(cmd_param));
	my_cmd_param->xzCom_params = my_xzCom_param;
	
	//xTaskCreate(vTaskCode_2, "vTaskCode_1", 100, NULL, 0, NULL);
	//xTaskCreate(vTaskCode_1, "vTaskCode_2", 100, NULL, 0, NULL);
	xTaskCreate(cmd, "cmd", 100, my_cmd_param, 0, NULL);
	xTaskCreate(gotoXZ, "gotoXZ", 100, my_taskXZ_param, 0, NULL);
	xTaskCreate(gotoX, "gotoX", 100, my_xMov_param, 0, NULL);
	xTaskCreate(gotoZ, "gotoZ", 100, my_zMov_param, 0, NULL);
	initialisePorts();
}