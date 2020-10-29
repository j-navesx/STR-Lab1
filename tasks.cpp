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

//OTHER VARS
int led_period = 0;
TaskHandle_t left_button_handle;

#define INCLUDE_vTaskSuspend 1

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

//LEFT   0000 0001  0x1
void vTaskLeftLED(void* pvParameters) {
	uInt8 p;

	while (TRUE) {
		while (led_period) {
			p = readDigitalU8(2);
			setBitValue(&p, 0, 1);

			taskENTER_CRITICAL();
			writeDigitalU8(2, p);
			taskEXIT_CRITICAL();

			Sleep(led_period);

			p = readDigitalU8(2);
			setBitValue(&p, 0, 0);

			taskENTER_CRITICAL();
			writeDigitalU8(2, p);
			taskEXIT_CRITICAL();

			Sleep(led_period);
		}
	}
}

// RIGHT  0000 0010  0x2
void vTaskRightLED(void* pvParameters) {
	uInt8 p;

	while (TRUE) {
		while (led_period) {
			p = readDigitalU8(2);
			setBitValue(&p, 1, 1);

			taskENTER_CRITICAL();
			writeDigitalU8(2, p);
			taskEXIT_CRITICAL();

			Sleep(led_period);

			p = readDigitalU8(2);
			setBitValue(&p, 1, 0);

			taskENTER_CRITICAL();
			writeDigitalU8(2, p);
			taskEXIT_CRITICAL();

			Sleep(led_period);
		}
	}
}

void vTaskEmergencyStop(void* pvParameters) {
	uInt8 p;

	while (TRUE) {
		//Switch 1 -> p1 xx1x xxxx
		//Switch 2 -> p1 x1xx xxxx
		p = readDigitalU8(1);
		if (getBitValue(p, 5) && getBitValue(p, 6)) { // If both pressed, enter Emergency Stop
			led_period = 500; //0.5 s period of flashing
			uInt8 currentMovement_State = readDigitalU8(2); //save the current grid movement state
			printf("\n\n!! Emergency Stop !!\n\n");

			vTaskSuspend(left_button_handle); //stop the task that acts on left button being pressed
			taskENTER_CRITICAL();
			writeDigitalU8(2, 0); //stop all movement
			taskEXIT_CRITICAL();

			while (TRUE) {
				p = readDigitalU8(1);
				if (getBitValue(p, 5) && !getBitValue(p, 6)) {  //Pressed left button. Resume Operation.
					printf("\nFalse Alarm. Resuming operation.\n");
					taskENTER_CRITICAL();
					writeDigitalU8(2, currentMovement_State); //resumes the state before forced stopped
					taskEXIT_CRITICAL();
					break;
				}
				else if (getBitValue(p, 6) && !getBitValue(p, 5)) { //Pressed right button. Reset System.
					/*
					* Not done yet
					*/
					break;
				}
			}
			vTaskResume(left_button_handle);
			led_period = 0;
		}
	}
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

	xTaskCreate(vTaskLeftLED, "vTaskLeftLED", 100, NULL, 0, NULL);
	xTaskCreate(vTaskRightLED, "vTaskRightLED", 100, NULL, 0, NULL);
	xTaskCreate(vTaskEmergencyStop, "vTaskEmergencyStop", 100, NULL, 0, NULL);

	initialisePorts();
}