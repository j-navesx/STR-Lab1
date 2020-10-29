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
}StorageRequest;

typedef struct {
	xQueueHandle mbx_server;
	int spaces_available;
}StorageServer;

typedef struct {
	int xcord;
	int zcord;
}Coords;

typedef struct {
	xQueueHandle mbx_xMov;
	xSemaphoreHandle sem_xMov;
}xMov_param;

typedef struct {
	xQueueHandle mbx_yMov;
	xSemaphoreHandle sem_yMov;
}yMov_param;

typedef struct {
	xSemaphoreHandle sem_zDownMov;
	xSemaphoreHandle sem_zDownMov_done;
}zDownMov_param;

typedef struct {
	xSemaphoreHandle sem_zUpMov;
	xSemaphoreHandle sem_zUpMov_done;
}zUpMov_param;

typedef struct {
	xQueueHandle mbx_zMov;
	xSemaphoreHandle sem_zMov;
}zMov_param;

typedef struct {
	xQueueHandle mbx_xzMov;
	xSemaphoreHandle sem_xzMov;
}xzCom_param;

typedef struct {
	xSemaphoreHandle sem_takeFromCellMov;
	xSemaphoreHandle sem_takeFromCellMov_done;
}takeFromCellCom_param;

typedef struct {
	xSemaphoreHandle sem_putInCellMov;
	xSemaphoreHandle sem_putInCellMov_done;
}putInCellCom_param;

typedef struct {
	xQueueHandle mbx_addStockMov;
	xSemaphoreHandle sem_addStockMov;
}addStockCom_param;

typedef struct {
	xQueueHandle mbx_takeStockMov;
	xSemaphoreHandle sem_takeStockMov;
}takeStockCom_param;

typedef struct {
	xzCom_param* xzCom_params;
	yMov_param* yMov_params;
	putInCellCom_param* putInCellCom_params;
	takeFromCellCom_param* takeFromCellCom_params;
	addStockCom_param* addStockCom_params;
	takeStockCom_param* takeStockCom_params;
}taskStockMov_param;

typedef struct {
	yMov_param* yMov_params;
	zDownMov_param* zDownMov_params;
	zUpMov_param* zUpMov_params;
	putInCellCom_param* putInCellCom_params;
	takeFromCellCom_param* takeFromCellCom_params;
}tasksCellMov_param;

typedef struct {
	xzCom_param* xzCom_params;
	xMov_param* xMov_params;
	zMov_param* zMov_params;
	yMov_param* yMov_params;
}taskXZ_param;

//Task Comunication params
typedef struct {
	//Will expand during development
	xzCom_param* xzCom_params;
	addStockCom_param* addStockCom_params;
	takeStockCom_param* takeStockCom_params;
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

void infoMenu() {
	while (true) {
		printf("n");
		if (_kbhit()) {
			char input = _getch();
			printf("Number %c", input);
		}
	}

}

void cmd(void * pvParameters) {
	
	//cmd needed parameters 
	cmd_param* cmd_params = (cmd_param*)pvParameters;

	//XZ comunication parameters 
	xzCom_param* xzCom_params = cmd_params->xzCom_params;
	xQueueHandle mbx_xzMov = xzCom_params->mbx_xzMov;
	xSemaphoreHandle sem_xzMov = xzCom_params->sem_xzMov;

	//addStock comunication parameters 
	addStockCom_param* addStockCom_params = cmd_params->addStockCom_params;
	xQueueHandle mbx_addStockMov = addStockCom_params->mbx_addStockMov;
	xSemaphoreHandle sem_addStockMov = addStockCom_params->sem_addStockMov;

	//takeStock comunication parameters 
	takeStockCom_param* takeStockCom_params = cmd_params->takeStockCom_params;
	xQueueHandle mbx_takeStockMov = takeStockCom_params->mbx_takeStockMov;
	xSemaphoreHandle sem_takeStockMov = takeStockCom_params->sem_takeStockMov;

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
			xQueueSend(mbx_xzMov, &coords, 0);
			xSemaphoreTake(sem_xzMov, portMAX_DELAY);
		}
		if (!comm.compare("info")) {
			infoMenu();
		}
		if (!comm.compare("exit")) {
			exit(0);
		}
	}
}

void gotoXZ(void* pvParameters) {
	int x, z, y;
	int coords[2];
	
	//XZ needed parameters 
	taskXZ_param* gotoXZ_params = (taskXZ_param*)pvParameters;
	
	//XZ comunication parameters 
	xzCom_param* xzCom_params = gotoXZ_params->xzCom_params;
	xQueueHandle mbx_xzMov = xzCom_params->mbx_xzMov;
	xSemaphoreHandle sem_xzMov = xzCom_params->sem_xzMov;

	//X movement parameters 
	xMov_param* xMov_params = gotoXZ_params->xMov_params;
	xQueueHandle mbx_xMov = xMov_params->mbx_xMov;
	xSemaphoreHandle sem_xMov = xMov_params->sem_xMov;

	//Z movement parameters 
	zMov_param* zMov_params = gotoXZ_params->zMov_params;
	xQueueHandle mbx_zMov = zMov_params->mbx_zMov;
	xSemaphoreHandle sem_zMov = zMov_params->sem_zMov;

	//Y movement parameters 
	yMov_param* yMov_params = gotoXZ_params->yMov_params;
	xQueueHandle mbx_yMov = yMov_params->mbx_yMov;
	xSemaphoreHandle sem_yMov = yMov_params->sem_yMov;

	while (true) {
		xQueueReceive(mbx_xzMov, &coords, portMAX_DELAY);
		//printf("\n%d,%d\n", coords[0], coords[1]);

		if (getYPos() != 2) {
			y = 2;
			xQueueSend(mbx_yMov, &y, portMAX_DELAY);
			xSemaphoreTake(sem_yMov, portMAX_DELAY);
		}

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

void gotoY(void* pvParameters) {
	int y;
	int crtpos;

	yMov_param* yMov_params = (yMov_param*)pvParameters;
	xQueueHandle mbx_yMov = yMov_params->mbx_yMov;
	xSemaphoreHandle sem_yMov = yMov_params->sem_yMov;

	while (true) {
		xQueueReceive(mbx_yMov, &y, portMAX_DELAY);
		crtpos = getYPos();

		if (crtpos > y)
			moveYIn();
		else
			if (crtpos < y)
				moveYOut();
		while (getYPos() != y) {
			taskYIELD();
		}
		stopY();
		xSemaphoreGive(sem_yMov);
	}
}

void gotoZUp(void* pvParameters) {
	zUpMov_param* zUpMov_params = (zUpMov_param*)pvParameters;
	xSemaphoreHandle sem_zUpMov = zUpMov_params->sem_zUpMov;
	xSemaphoreHandle sem_zUpMov_done = zUpMov_params->sem_zUpMov_done;

	while (1) {
		xSemaphoreTake(sem_zUpMov, portMAX_DELAY);
		moveZUp();

		uInt8 p0 = readDigitalU8(0);
		uInt8 p1 = readDigitalU8(1);

		while (getBitValue(p1, 2) && getBitValue(p1, 0) && getBitValue(p0, 6)) {
			p0 = readDigitalU8(0);
			p1 = readDigitalU8(1);
			taskYIELD();
		}
		stopZ();
		xSemaphoreGive(sem_zUpMov_done);
	}
}

void gotoZDown(void* pvParameters) {
	zDownMov_param* zDownMov_params = (zDownMov_param*)pvParameters;
	xSemaphoreHandle sem_zDownMov = zDownMov_params->sem_zDownMov;
	xSemaphoreHandle sem_zDownMov_done = zDownMov_params->sem_zDownMov_done;

	while (1) {
		xSemaphoreTake(sem_zDownMov, portMAX_DELAY);
		moveZDown();

		uInt8 p0 = readDigitalU8(0);
		uInt8 p1 = readDigitalU8(1);

		while (getBitValue(p1, 3) && getBitValue(p1, 1) && getBitValue(p0, 7)) {
			p0 = readDigitalU8(0);
			p1 = readDigitalU8(1);
			taskYIELD();
		}
		stopZ();
		xSemaphoreGive(sem_zDownMov_done);
	}
}

void putPartInCell(void* pvParameters) {
	int y;

	//putPartInCell needed parameters 
	tasksCellMov_param* tasksCellMov_params = (tasksCellMov_param*)pvParameters;

	//putPartInCell comunication parameters 
	putInCellCom_param* putInCellCom_params = tasksCellMov_params->putInCellCom_params;
	xSemaphoreHandle sem_putInCellMov = putInCellCom_params->sem_putInCellMov;
	xSemaphoreHandle sem_putInCellMov_done = putInCellCom_params->sem_putInCellMov_done;

	//ZUp movement parameters 
	zUpMov_param* zUpMov_params = tasksCellMov_params->zUpMov_params;
	xSemaphoreHandle sem_zUpMov = zUpMov_params->sem_zUpMov;
	xSemaphoreHandle sem_zUpMov_done = zUpMov_params->sem_zUpMov_done;

	//ZDown movement parameters 
	zDownMov_param* zDownMov_params = tasksCellMov_params->zDownMov_params;
	xSemaphoreHandle sem_zDownMov = zDownMov_params->sem_zDownMov;
	xSemaphoreHandle sem_zDownMov_done = zDownMov_params->sem_zDownMov_done;

	//Y movement parameters 
	yMov_param* yMov_params = tasksCellMov_params->yMov_params;
	xQueueHandle mbx_yMov = yMov_params->mbx_yMov;
	xSemaphoreHandle sem_yMov = yMov_params->sem_yMov;

	while (1) {
		xSemaphoreTake(sem_putInCellMov, portMAX_DELAY);

		//gotoZUp();
		xSemaphoreGive(sem_zUpMov);
		xSemaphoreTake(sem_zUpMov_done, portMAX_DELAY);
		//gotoY(1);
		y = 1;
		xQueueSend(mbx_yMov, &y, 0);
		xSemaphoreTake(sem_yMov, portMAX_DELAY);
		//gotoZDown();
		xSemaphoreGive(sem_zDownMov);
		xSemaphoreTake(sem_zDownMov_done, portMAX_DELAY);
		//gotoY(2);
		y = 2;
		xQueueSend(mbx_yMov, &y, 0);
		xSemaphoreTake(sem_yMov, portMAX_DELAY);

		xSemaphoreGive(sem_putInCellMov_done);
	}
}

void takePartFromCell(void* pvParameters) {
	int y;

	//takePartFromCell needed parameters 
	tasksCellMov_param* tasksCellMov_params = (tasksCellMov_param*)pvParameters;

	//takePartFromCell comunication parameters 
	takeFromCellCom_param* takeFromCellCom_params = tasksCellMov_params->takeFromCellCom_params;
	xSemaphoreHandle sem_takeFromCellMov = takeFromCellCom_params->sem_takeFromCellMov;
	xSemaphoreHandle sem_takeFromCellMov_done = takeFromCellCom_params->sem_takeFromCellMov_done;

	//ZUp movement parameters 
	zUpMov_param* zUpMov_params = tasksCellMov_params->zUpMov_params;
	xSemaphoreHandle sem_zUpMov = zUpMov_params->sem_zUpMov;
	xSemaphoreHandle sem_zUpMov_done = zUpMov_params->sem_zUpMov_done;

	//ZDown movement parameters 
	zDownMov_param* zDownMov_params = tasksCellMov_params->zDownMov_params;
	xSemaphoreHandle sem_zDownMov = zDownMov_params->sem_zDownMov;
	xSemaphoreHandle sem_zDownMov_done = zDownMov_params->sem_zDownMov_done;

	//Y movement parameters 
	yMov_param* yMov_params = tasksCellMov_params->yMov_params;
	xQueueHandle mbx_yMov = yMov_params->mbx_yMov;
	xSemaphoreHandle sem_yMov = yMov_params->sem_yMov;

	while (1) {
		xSemaphoreTake(sem_takeFromCellMov, portMAX_DELAY);

		//gotoY(1);
		y = 1;
		xQueueSend(mbx_yMov, &y, 0);
		xSemaphoreTake(sem_yMov, portMAX_DELAY);
		//gotoZUp();
		xSemaphoreGive(sem_zUpMov);
		xSemaphoreTake(sem_zUpMov_done, portMAX_DELAY);
		//gotoY(2);
		y = 2;
		xQueueSend(mbx_yMov, &y, 0);
		xSemaphoreTake(sem_yMov, portMAX_DELAY);
		//gotoZDown();
		xSemaphoreGive(sem_zDownMov);
		xSemaphoreTake(sem_zDownMov_done, portMAX_DELAY);

		xSemaphoreGive(sem_takeFromCellMov_done);
	}
}

void addStock(void* pvParameters) {
	int y;
	int user[2];
	int dock[2] = { 1, 1 };

	//addStock needed parameters 
	taskStockMov_param* taskStockMov_params = (taskStockMov_param*)pvParameters;

	//addStock comunication parameters 
	addStockCom_param* addStockCom_params = taskStockMov_params->addStockCom_params;
	xQueueHandle mbx_addStockMov = addStockCom_params->mbx_addStockMov;
	xSemaphoreHandle sem_addStockMov = addStockCom_params->sem_addStockMov;

	//putPartInCell comunication parameters 
	putInCellCom_param* putInCellCom_params = taskStockMov_params->putInCellCom_params;
	xSemaphoreHandle sem_putInCellMov = putInCellCom_params->sem_putInCellMov;
	xSemaphoreHandle sem_putInCellMov_done = putInCellCom_params->sem_putInCellMov_done;

	//XZ comunication parameters 
	xzCom_param* xzCom_params = taskStockMov_params->xzCom_params;
	xQueueHandle mbx_xzMov = xzCom_params->mbx_xzMov;
	xSemaphoreHandle sem_xzMov = xzCom_params->sem_xzMov;

	//Y movement parameters 
	yMov_param* yMov_params = taskStockMov_params->yMov_params;
	xQueueHandle mbx_yMov = yMov_params->mbx_yMov;
	xSemaphoreHandle sem_yMov = yMov_params->sem_yMov;

	while (1) {
		xQueueReceive(mbx_addStockMov, &user, portMAX_DELAY);

		//gotoXZ(1,1)
		xQueueSend(mbx_xzMov, &dock, portMAX_DELAY);
		xSemaphoreTake(sem_xzMov, portMAX_DELAY);
		//gotoY(3)
		y = 3;
		xQueueSend(mbx_yMov, &y, 0);
		xSemaphoreTake(sem_yMov, portMAX_DELAY);
		//receive
		//gotoY(2)
		y = 2;
		xQueueSend(mbx_yMov, &y, 0);
		xSemaphoreTake(sem_yMov, portMAX_DELAY);
		//gotoXZ(x,z), (x,z) is the position chosen by the user or the system
		xQueueSend(mbx_xzMov, &user, portMAX_DELAY);
		xSemaphoreTake(sem_xzMov, portMAX_DELAY);
		//putPartInCell();
		xSemaphoreGive(sem_putInCellMov);
		xSemaphoreTake(sem_putInCellMov_done, portMAX_DELAY);

		xSemaphoreGive(sem_addStockMov);
	}
}

void takeStock(void* pvParameters) {
	int y;
	int user[2];
	int dock[2] = { 1, 1 };

	//takeStock needed parameters 
	taskStockMov_param* taskStockMov_params = (taskStockMov_param*)pvParameters;

	//takeStock comunication parameters 
	takeStockCom_param* takeStockCom_params = taskStockMov_params->takeStockCom_params;
	xQueueHandle mbx_takeStockMov = takeStockCom_params->mbx_takeStockMov;
	xSemaphoreHandle sem_takeStockMov = takeStockCom_params->sem_takeStockMov;

	//takePartFromCell comunication parameters 
	takeFromCellCom_param* takeFromCellCom_params = taskStockMov_params->takeFromCellCom_params;
	xSemaphoreHandle sem_takeFromCellMov = takeFromCellCom_params->sem_takeFromCellMov;
	xSemaphoreHandle sem_takeFromCellMov_done = takeFromCellCom_params->sem_takeFromCellMov_done;

	//XZ comunication parameters 
	xzCom_param* xzCom_params = taskStockMov_params->xzCom_params;
	xQueueHandle mbx_xzMov = xzCom_params->mbx_xzMov;
	xSemaphoreHandle sem_xzMov = xzCom_params->sem_xzMov;

	//Y movement parameters 
	yMov_param* yMov_params = taskStockMov_params->yMov_params;
	xQueueHandle mbx_yMov = yMov_params->mbx_yMov;
	xSemaphoreHandle sem_yMov = yMov_params->sem_yMov;

	while (1) {
		xQueueReceive(mbx_takeStockMov, &user, portMAX_DELAY);

		//goto(x,z), (x,z) is the position of the item
		xQueueSend(mbx_xzMov, &user, portMAX_DELAY);
		xSemaphoreTake(sem_xzMov, portMAX_DELAY);
		//takePartFromCell();
		xSemaphoreGive(sem_takeFromCellMov);
		xSemaphoreTake(sem_takeFromCellMov_done, portMAX_DELAY);
		//gotoXZ(1,1)
		xQueueSend(mbx_xzMov, &dock, portMAX_DELAY);
		xSemaphoreTake(sem_xzMov, portMAX_DELAY);
		//gotoY(3)
		y = 3;
		xQueueSend(mbx_yMov, &y, 0);
		xSemaphoreTake(sem_yMov, portMAX_DELAY);
		//deliver
		//gotoY(2)
		y = 2;
		xQueueSend(mbx_yMov, &y, 0);
		xSemaphoreTake(sem_yMov, portMAX_DELAY);

		xSemaphoreGive(sem_takeStockMov);
	}
}

void myDaemonTaskStartupHook(void) {
	//Grid initialization 
	StorageRequest grid[3][3] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	
	xzCom_param* my_xzCom_param = (xzCom_param*)pvPortMalloc(sizeof(xzCom_param));
	my_xzCom_param->mbx_xzMov = xQueueCreate(1, sizeof(Coords));
	my_xzCom_param->sem_xzMov = xSemaphoreCreateCounting(1, 0);

	zMov_param* my_zMov_param = (zMov_param*)pvPortMalloc(sizeof(zMov_param));
	my_zMov_param->mbx_zMov = xQueueCreate(1, sizeof(int));
	my_zMov_param->sem_zMov = xSemaphoreCreateCounting(1, 0);

	zDownMov_param* my_zDownMov_param = (zDownMov_param*)pvPortMalloc(sizeof(zDownMov_param));
	my_zDownMov_param->sem_zDownMov = xSemaphoreCreateCounting(1, 0);
	my_zDownMov_param->sem_zDownMov_done = xSemaphoreCreateCounting(1, 0);

	zUpMov_param* my_zUpMov_param = (zUpMov_param*)pvPortMalloc(sizeof(zUpMov_param));
	my_zUpMov_param->sem_zUpMov = xSemaphoreCreateCounting(1, 0);
	my_zUpMov_param->sem_zUpMov_done = xSemaphoreCreateCounting(1, 0);

	xMov_param* my_xMov_param = (xMov_param*)pvPortMalloc(sizeof(xMov_param));
	my_xMov_param->mbx_xMov = xQueueCreate(1, sizeof(int));
	my_xMov_param->sem_xMov = xSemaphoreCreateCounting(1, 0);

	yMov_param* my_yMov_param = (yMov_param*)pvPortMalloc(sizeof(yMov_param));
	my_yMov_param->mbx_yMov = xQueueCreate(1, sizeof(int));
	my_yMov_param->sem_yMov = xSemaphoreCreateCounting(1, 0);

	takeFromCellCom_param* my_takeFromCellCom_param = (takeFromCellCom_param*)pvPortMalloc(sizeof(takeFromCellCom_param));
	my_takeFromCellCom_param->sem_takeFromCellMov = xSemaphoreCreateCounting(1, 0);
	my_takeFromCellCom_param->sem_takeFromCellMov_done = xSemaphoreCreateCounting(1, 0);

	putInCellCom_param* my_putInCellCom_param = (putInCellCom_param*)pvPortMalloc(sizeof(putInCellCom_param));
	my_putInCellCom_param->sem_putInCellMov = xSemaphoreCreateCounting(1, 0);
	my_putInCellCom_param->sem_putInCellMov_done = xSemaphoreCreateCounting(1, 0);

	addStockCom_param* my_addStockCom_param = (addStockCom_param*)pvPortMalloc(sizeof(addStockCom_param));
	my_addStockCom_param->mbx_addStockMov = xQueueCreate(1, sizeof(Coords));
	my_addStockCom_param->sem_addStockMov = xSemaphoreCreateCounting(1, 0);

	takeStockCom_param* my_takeStockCom_param = (takeStockCom_param*)pvPortMalloc(sizeof(takeStockCom_param));
	my_takeStockCom_param->mbx_takeStockMov = xQueueCreate(1, sizeof(Coords));
	my_takeStockCom_param->sem_takeStockMov = xSemaphoreCreateCounting(1, 0);
	
	taskXZ_param* my_taskXZ_param = (taskXZ_param*)pvPortMalloc(sizeof(taskXZ_param));
	my_taskXZ_param->xzCom_params = my_xzCom_param;
	my_taskXZ_param->xMov_params = my_xMov_param;
	my_taskXZ_param->zMov_params = my_zMov_param;
	my_taskXZ_param->yMov_params = my_yMov_param;

	tasksCellMov_param* my_tasksCellMov_param = (tasksCellMov_param*)pvPortMalloc(sizeof(tasksCellMov_param));
	my_tasksCellMov_param->zUpMov_params = my_zUpMov_param;
	my_tasksCellMov_param->zDownMov_params = my_zDownMov_param;
	my_tasksCellMov_param->yMov_params = my_yMov_param;
	my_tasksCellMov_param->takeFromCellCom_params = my_takeFromCellCom_param;
	my_tasksCellMov_param->putInCellCom_params = my_putInCellCom_param;

	taskStockMov_param* my_taskStockMov_param = (taskStockMov_param*)pvPortMalloc(sizeof(taskStockMov_param));
	my_taskStockMov_param->xzCom_params = my_xzCom_param;
	my_taskStockMov_param->yMov_params = my_yMov_param;
	my_taskStockMov_param->takeFromCellCom_params = my_takeFromCellCom_param;
	my_taskStockMov_param->putInCellCom_params = my_putInCellCom_param;
	my_taskStockMov_param->addStockCom_params = my_addStockCom_param;
	my_taskStockMov_param->takeStockCom_params = my_takeStockCom_param;

	cmd_param* my_cmd_param = (cmd_param*)pvPortMalloc(sizeof(cmd_param));
	my_cmd_param->xzCom_params = my_xzCom_param;
	my_cmd_param->addStockCom_params = my_addStockCom_param;
	my_cmd_param->takeStockCom_params = my_takeStockCom_param;
	
	//xTaskCreate(vTaskCode_2, "vTaskCode_1", 100, NULL, 0, NULL);
	//xTaskCreate(vTaskCode_1, "vTaskCode_2", 100, NULL, 0, NULL);
	xTaskCreate(cmd, "cmd", 100, my_cmd_param, 0, NULL);
	xTaskCreate(gotoXZ, "gotoXZ", 100, my_taskXZ_param, 0, NULL);
	xTaskCreate(gotoX, "gotoX", 100, my_xMov_param, 0, NULL);
	xTaskCreate(gotoZ, "gotoZ", 100, my_zMov_param, 0, NULL);
	xTaskCreate(gotoZUp, "gotoZUp", 100, my_zUpMov_param, 0, NULL);
	xTaskCreate(gotoZDown, "gotoZDown", 100, my_zDownMov_param, 0, NULL);
	xTaskCreate(gotoY, "gotoY", 100, my_yMov_param, 0, NULL);
	xTaskCreate(takePartFromCell, "takePartFromCell", 100, my_tasksCellMov_param, 0, NULL);
	xTaskCreate(putPartInCell, "putPartInCell", 100, my_tasksCellMov_param, 0, NULL);
	xTaskCreate(addStock, "addStock", 100, my_taskStockMov_param, 0, NULL);
	xTaskCreate(takeStock, "takeStock", 100, my_taskStockMov_param, 0, NULL);
	initialisePorts();
}