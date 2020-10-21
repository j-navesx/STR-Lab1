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
	xQueueHandle mbx_Mov;
	xSemaphoreHandle sem_Mov;
}Mov_param;

typedef struct {
	Mov_param* Mov_params;
	xMov_param* xMov_params;
	zMov_param* zMov_params;
}taskXZ_param;

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
	
	Mov_param* cmd_params = (Mov_param*)pvParameters;
	xQueueHandle mbx_Mov = cmd_params->mbx_Mov;
	xSemaphoreHandle sem_Mov = cmd_params->sem_Mov;

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
			xSemaphoreTake(sem_Mov, portMAX_DELAY);
			xQueueSend(mbx_Mov, &coords, 0);

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
	
	taskXZ_param* gotoXZ_params = (taskXZ_param*)pvParameters;
	
	Mov_param* Mov_params = gotoXZ_params->Mov_params;
	xQueueHandle mbx_Mov = Mov_params->mbx_Mov;
	xSemaphoreHandle sem_Mov = Mov_params->sem_Mov;

	xMov_param* xMov_params = gotoXZ_params->xMov_params;
	xQueueHandle mbx_xMov = xMov_params->mbx_xMov;
	xSemaphoreHandle sem_xMov = xMov_params->sem_xMov;

	zMov_param* zMov_params = gotoXZ_params->zMov_params;
	xQueueHandle mbx_zMov = zMov_params->mbx_zMov;
	xSemaphoreHandle sem_zMov = zMov_params->sem_zMov;

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

		xSemaphoreGive(sem_Mov, portMAX_DELAY);
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
		xSemaphoreGive(sem_xMov, portMAX_DELAY);
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
	StoreRequest grid[3][3];
	fill(*grid, *grid + 9, NULL);

	
	Mov_param* my_Mov_param = (Mov_param*)pvPortMalloc(sizeof(Mov_param));
	my_Mov_param->mbx_Mov = xQueueCreate(1, sizeof(Coords));
	my_Mov_param->sem_Mov = xSemaphoreCreateCounting(1, 1);

	zMov_param* my_zMov_param = (zMov_param*)pvPortMalloc(sizeof(zMov_param));
	my_zMov_param->mbx_zMov = xQueueCreate(1, sizeof(int));
	my_zMov_param->sem_zMov = xSemaphoreCreateCounting(1, 0);

	xMov_param* my_xMov_param = (xMov_param*)pvPortMalloc(sizeof(xMov_param));
	my_xMov_param->mbx_xMov = xQueueCreate(1, sizeof(int));
	my_xMov_param->sem_xMov = xSemaphoreCreateCounting(1, 0);
	
	taskXZ_param* my_taskXZ_param = (taskXZ_param*)pvPortMalloc(sizeof(taskXZ_param));
	my_taskXZ_param->Mov_params = my_Mov_param;
	my_taskXZ_param->xMov_params = my_xMov_param;
	my_taskXZ_param->zMov_params = my_zMov_param;
	
	//xTaskCreate(vTaskCode_2, "vTaskCode_1", 100, NULL, 0, NULL);
	//xTaskCreate(vTaskCode_1, "vTaskCode_2", 100, NULL, 0, NULL);
	xTaskCreate(cmd, "cmd", 100, my_Mov_param, 0, NULL);
	xTaskCreate(gotoXZ, "gotoXZ", 100, my_taskXZ_param, 0, NULL);
	xTaskCreate(gotoX, "gotoX", 100, my_xMov_param, 0, NULL);
	xTaskCreate(gotoZ, "gotoZ", 100, my_zMov_param, 0, NULL);
	initialisePorts();
}