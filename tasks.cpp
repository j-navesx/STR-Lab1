#include<conio.h>
#include<iostream>
#include<stdlib.h>
#include<string>
#include<cstring>
#include <ctime>
#include <array>
#include <functional>
#include <algorithm>
#include <tuple>
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
	int xcord;
	int zcord;
}Coords;

typedef struct {
	string request;
	Coords location;
}ServerComms;

typedef struct {
	int reference;
	string* name;
	int expiration;
}StorageRequest;

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

typedef struct {
	xQueueHandle mbx_LeftLed;
}LeftLed;

typedef struct {
	xQueueHandle mbx_RightLed;
}RightLed;

typedef struct {
	LeftLed* LeftLed_param;
	RightLed* RightLed_param;
}emergency_param;

//Task Comunication params
typedef struct {
	//Will expand during development
	int availableSpaces;
	int expiredFlag;
	xQueueHandle mbx_cmd;
	xzCom_param* xzCom_params;
	addStockCom_param* addStockCom_params;
	takeStockCom_param* takeStockCom_params;
	emergency_param* emergencyStop_params;
	StorageRequest* StorageGrid[3][3];
}cmd_param;

xSemaphoreHandle sem_interruptMode;
Coords priorityList[9] = { {1,1}, {2,1}, {1,2}, {2,2}, {3,1}, {1,3}, {3,2}, {2,3}, {3,3} };

//MAILBOXES

TaskHandle_t left_button_handle;

#define INCLUDE_vTaskSuspend 1
#define LED_PERIOD 500

void initialisePorts() {
	//positions on the x axis (sensors on the bottom activate on passage)
	createDigitalInput(0);
	//postitions in the z axis (sensors on the crane activate on passage)
	createDigitalInput(1);
	//Controls the directions on the crane
	createDigitalOutput(2);

	writeDigitalU8(2, 0);
}

void idleStore(cmd_param* idle_params) {
	int boxNumber = 9 - idle_params->availableSpaces;
	array<int, 15> indexes = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	array<array<tuple<int, Coords>, 9>, 15> boxes;
	int index;
	Coords coord;
	int nOrdered = 0;
	int c, l;
	StorageRequest nullItem = {NULL,NULL,NULL};
	Coords originalCoords;
	//order boxes by reference with specific coordinates
	for (c = 0; c < 3; c++) {
		for (l = 0; l < 3; l++) {
			if (idle_params->StorageGrid[c][l]->reference != NULL) {
				index = idle_params->StorageGrid[c][l]->reference - 1;
				coord.xcord = c + 1;
				coord.zcord = l + 1;
				boxes[index][indexes[index]] = make_tuple(index, coord);
				indexes[index]++;
			}
		}
	}
	for (int i = 0; i <= indexes.size(); i++) {
		//check if there are any objects with the reference
		if (indexes[i] != 0) {
			for (int k = 0; k < indexes[i]; k++) {
				c = nOrdered % 3;
				l = nOrdered / 3;
				//Space not occupied
				if (idle_params->StorageGrid[c][l]->reference == NULL) {
					//Coords from the box we want to change
					originalCoords.xcord = get<1>(boxes[i][k]).xcord;
					originalCoords.zcord = get<1>(boxes[i][k]).zcord;
					//Alocate item to change
					StorageRequest* auxItem = (StorageRequest*) malloc(sizeof(StorageRequest));
					auxItem = idle_params->StorageGrid[originalCoords.xcord][originalCoords.zcord];
					//Delete from the old space
					free(idle_params->StorageGrid[originalCoords.xcord][originalCoords.zcord]);
					idle_params->StorageGrid[originalCoords.xcord][originalCoords.zcord] = (StorageRequest*)malloc(sizeof(StorageRequest));
					idle_params->StorageGrid[originalCoords.xcord][originalCoords.zcord] = &nullItem;
					//Put item in place
					idle_params->StorageGrid[c][l] = auxItem;
					//Create Request TODO
					nOrdered++;
				}
				//Space occupied
				else {
					//Put element in the last spaces of priorityList

				}
			}
		}
	}
}

void timePass(void* pvParameters) {
	cmd_param* timePass_params = (cmd_param*)pvParameters;

	emergency_param* emergencyStop_params = timePass_params->emergencyStop_params;
	LeftLed* LeftLed_params = emergencyStop_params->LeftLed_param;
	xQueueHandle mbx_LeftLed = LeftLed_params->mbx_LeftLed;

	time_t now = time(0);
	tm* ltm = localtime(&now);
	int min = ltm->tm_min;
	int activate = 1;
	while (true) {
		time_t now = time(0);
		tm* ltm = localtime(&now);
		if (ltm->tm_min != min) {
			min = ltm->tm_min;
			for (int c = 0; c < 3; c++) {
				for (int l = 0; l < 3; l++) {
					if (timePass_params->StorageGrid[c][l]->reference != NULL) {
						if (--timePass_params->StorageGrid[c][l]->expiration <= 0) {
							xQueueSend(mbx_LeftLed, &activate, 0);
							timePass_params->expiredFlag = 1;
						}
					}
				}
			}
		}
	}
}

//SetConsoleTextAttribute(hConsole, 14);
void iListAll(cmd_param* grid) {
	while (true) {
		system("cls");
		cout << "\t\t\t\t|  List All Products  |\n\n\n";
		for (int c = 2; c >= 0; c--) {
			cout << "\t\t------|------|------\n\t\t| ";
			for (int l = 0; l < 3; l++) {
				if (grid->StorageGrid[c][l]->reference != NULL) {
					if (grid->StorageGrid[c][l]->reference < 10) {
						cout << "0";
					}
					cout << grid->StorageGrid[c][l]->reference;
				}
				else {
					cout << "-1";
				}
				cout << "  |  ";
			}
			cout << "\n";

		}
		cout << "\t\t------|------|------\n";

		cout << "\nREFERENCE-NAME-COORDINATES-EXPIRATION DATE\n\n";
		for (int c = 0; c < 3; c++) {
			for (int l = 0; l < 3; l++) {
				if (grid->StorageGrid[c][l]->reference != NULL) {
					if (grid->StorageGrid[c][l]->reference < 10) {
						cout << "0";
					}
					cout << grid->StorageGrid[c][l]->reference;
					cout << "-";
					cout << *grid->StorageGrid[c][l]->name;
					cout << "-";
					printf("(%d,%d)", c + 1, l + 1);
					cout << "-(";
					cout << grid->StorageGrid[c][l]->expiration;
					cout << ")\n";
				}
			}


		}

		cout << "\n\nTo return to the Main Menu press \"e\"";

		if (_kbhit()) {
			char input = _getch();
			if (input == 'e') {
				break;
			}
			else {
				break;
			}
		}
	}
}

void iListExp(cmd_param* grid) {
	while (true) {
		system("cls");
		cout << "\t\t\t\t|  List Expired Products  |\n\n\n";
		for (int c = 2; c >= 0; c--) {
			cout << "\t\t------|------|------\n\t\t| ";
			for (int l = 0; l < 3; l++) {
				if (grid->StorageGrid[c][l]->reference != NULL) {
					if (grid->StorageGrid[c][l]->expiration <= 0) {
						if (grid->StorageGrid[c][l]->reference < 10) {
							cout << "0";
						}
						cout << grid->StorageGrid[c][l]->reference;
					}
					else {
						cout << "-1";
					}
				}
				else {
					cout << "-1";
				}
				cout << "  |  ";
			}
			cout << "\n";

		}
		cout << "\t\t------|------|------\n";

		cout << "\nREFERENCE-NAME-COORDINATES-EXPIRATION DATE\n\n";
		for (int c = 0; c < 3; c++) {
			for (int l = 0; l < 3; l++) {
				if (grid->StorageGrid[c][l]->reference != NULL) {
					if (grid->StorageGrid[c][l]->expiration <= 0) {
						if (grid->StorageGrid[c][l]->reference < 10) {
							cout << "0";
						}
						cout << grid->StorageGrid[c][l]->reference;
						cout << "-";
						cout << *grid->StorageGrid[c][l]->name;
						cout << "-";
						printf("(%d,%d)", c + 1, l + 1);
						cout << "-(";
						cout << grid->StorageGrid[c][l]->expiration;
						cout << ")\n";
					}
				}
			}


		}

		cout << "\n\nTo return to the Main Menu press \"e\"";

		if (_kbhit()) {
			char input = _getch();
			if (input == 'e') {
				break;
			}
			else {
				break;
			}
		}
	}
}

void iSearchProd(cmd_param* grid) {
	string search;
	int x = -1;
	int z = -1;
	Coords selected[9];
	for (int i = 0; i < 9; i++) {
		selected[i].xcord = -1;
	}
	int auxSelect = 0;
	while (true) {
		system("cls");
		cout << "\t\t\t\t|  SEARCH REFERENCE MENU  |\n\n\n";
		cout << "Write the reference you want to search:\n\t:";
		if (_kbhit()) {
			cin >> search;
			if (!search.compare("exit")) {
				break;
			}
			long int numInt = strtol(search.c_str(), NULL, 10);
			if(numInt >= 1 && numInt <= 15){
				for (int c = 0; c < 3; c++) {
					for (int l = 0; l < 3; l++) {
						if (grid->StorageGrid[c][l]->reference != NULL) {
							if (grid->StorageGrid[c][l]->reference == stoi(search)) {
								x = c;
								z = l;
								selected[auxSelect].xcord = c;
								selected[auxSelect].zcord = l;
								auxSelect++;
							}
						}
					}
				}
			}
		}
		if (x != -1) {
			cout << "\n\t\tResult: ";
			if (auxSelect != 0) {
				for (int j = 0; j < auxSelect; j++) {
					x = selected[j].xcord;
					z = selected[j].zcord;
					cout << grid->StorageGrid[x][z]->reference;
					cout << "-";
					cout << *grid->StorageGrid[x][z]->name;
					cout << "-";
					printf("(%d,%d)", x + 1, z + 1);
					cout << "-(";
					cout << grid->StorageGrid[x][z]->expiration;
					cout << ")\n\t\t";
				}
			}
		}
		cout << "\n\nexit - Exit menu";
	}
}

void infoMenu(cmd_param* grid) {

	while (true) {
		system("cls");
		cout << "\t\t\t\t|  INFORMATION MENU  |\n\n\n";
		cout << "1 - List all products\n";
		cout << "2 - List expired products\n";
		cout << "3 - Search from reference\n";
		cout << "e - Exit menu";
		if (_kbhit()) {
			char input = _getch();
			if (input == '1') {
				iListAll(grid);
			}
			if (input == '2') {
				iListExp(grid);
			}
			if (input == '3') {
				iSearchProd(grid);
			}
			if (input == 'e') {
				break;
			}
		}
	}

}

Coords coordsInput() {
	//TODO RESTRICTIONS
	system("cls");
	Coords coord;
	cout << "X Coordinate:\n\t:";
	scanf("%*c");
	scanf("%d", &coord.xcord);
	cout << "Z Coordinate:\n\t:";
	scanf("%d", &coord.zcord);
	return coord;
}

Coords addMenu(cmd_param* grid) {
	Coords coord;
	int cancel = -2;
	while (cancel == -2) {
		system("cls");
		cout << "Select position or closest to (1,1)?\n";
		cout << "- Select Pos (s)\n";
		cout << "- Closest to (1,1) (c)\n";
		cout << "- Exit (e)\n";
		char input = _getch();
		if (input == 's') {
			coord = coordsInput();
			cancel = -1;
		}
		if (input == 'c') {
			//select from priority list
			for (int i = 0; i < 9; i++) {
				if (grid->StorageGrid[priorityList[i].xcord - 1][priorityList[i].zcord - 1]->reference == NULL) {
					coord.xcord = priorityList[i].xcord;
					coord.zcord = priorityList[i].zcord;
					cancel = 1;
					break;
				}
			}
		}
		if (input == 'e') {
			cancel = 0;
		}
	}
	while (cancel == -1) {
		cout << grid->StorageGrid[coord.xcord - 1][coord.zcord - 1]->reference;
		if (grid->StorageGrid[coord.xcord - 1][coord.zcord - 1]->reference != NULL) {
			cancel = -1;
			cout << "Grid space already occupied\n";
			cout << "- Try again (press anything)\n";
			cout << "- Exit (e)";
			char input = _getch();
			if (input == 'e') {
				cancel = 0;
				break;
			}
			else {
				coord = coordsInput();
			}
		}
		else {
			cancel = 1;
		}
	}
	if (cancel) {
		system("cls");
		StorageRequest* newItem = (StorageRequest*)malloc(sizeof(StorageRequest));
		newItem->reference = -1;
		newItem->expiration = -1;
		newItem->name = new string;
		int ref;
		while (newItem->reference == -1) {
			cout << "Insert Reference [1..15]:\n\t:";
			scanf("%d", &ref);
			if (ref >= 1 && ref <= 15) {
				newItem->reference = ref;
			}
			else {
				cout << "Invalid Reference: Insert a Reference between 1 and 15\n";
			}
		}
		cout << "Insert Name of the Product:\n\t:";
		string strin;
		cin >> strin;
		*newItem->name = strin;
		cout << "Insert expiration:\n\t:";
		scanf("%d", &newItem->expiration);

		grid->StorageGrid[coord.xcord - 1][coord.zcord - 1] = newItem;

		//grid->StorageGrid[coord.xcord][coord.zcord]->reference = newItem.reference;
		//grid->StorageGrid[coord.xcord][coord.zcord]->name = (newItem.name);
		//grid->StorageGrid[coord.xcord][coord.zcord]->expiration = newItem.expiration;
		return coord;
	}
	Coords nullItem = { NULL,NULL };
	return nullItem;
}

Coords takeMenu(cmd_param* grid) {
	StorageRequest nullItem = {NULL,NULL,NULL};
	int cancel = -1;
	Coords coord = coordsInput();
	while (cancel == -1) {
		cout << grid->StorageGrid[coord.xcord - 1][coord.zcord - 1]->reference;
		if (grid->StorageGrid[coord.xcord - 1][coord.zcord - 1]->reference == NULL) {
			cancel = -1;
			cout << "Grid space empty\n";
			cout << "- Try again (press anything)\n";
			cout << "- Exit (e)";
			char input = _getch();
			if (input == 'e') {
				cancel = 0;
				break;
			}
			else {
				coord = coordsInput();
			}
		}
		else {
			cancel = 1;
		}
	}
	if (cancel) {
		grid->StorageGrid[coord.xcord - 1][coord.zcord - 1] = (StorageRequest*) malloc(sizeof(StorageRequest));
		grid->StorageGrid[coord.xcord - 1][coord.zcord - 1]->reference = NULL;
		return coord;
	}
	Coords nullCoords = { NULL,NULL };
	return nullCoords;
}

void cmdUser(void* pvParameters) {

	cmd_param* cmdUser_params = (cmd_param*)pvParameters;

	xQueueHandle mbx_cmd = cmdUser_params->mbx_cmd;

	string comm;

	cmdUser_params->StorageGrid[2][1]->expiration = 6;

	while (true) {
		if (uxSemaphoreGetCount(sem_interruptMode) != 3) {
			system("cls");
			cout << "\n\tCommands available:\n";
			cout << "- goto\n";
			cout << "- add\n";
			cout << "- take\n";
			cout << "- info\n";
			cout << "- exit\n";
			cout << "\nInput your function:\n\t:";
			if (_kbhit()) {
				cin >> comm;
				if (!comm.compare("goto")) {
					Coords coord = coordsInput();
					ServerComms request;
					request.request = "goto";
					request.location = coord;
					xQueueSend(mbx_cmd, &request, 0);
				}
				if (!comm.compare("add")) {
					if (cmdUser_params->availableSpaces != 0) {
						Coords coord = addMenu(cmdUser_params);
						if (coord.xcord != NULL) {
							ServerComms request;
							request.request = "add";
							request.location = coord;
							xQueueSend(mbx_cmd, &request, 0);
							cmdUser_params->availableSpaces--;
						}
					}
					else {
						cout << "No available spaces at the moment\n";
						Sleep(1000);
					}
				}
				if (!comm.compare("take")) {
					if (cmdUser_params->availableSpaces > 0) {
						Coords coord = takeMenu(cmdUser_params);
						if (coord.xcord != NULL) {
							ServerComms request;
							request.request = "take";
							request.location = coord;
							xQueueSend(mbx_cmd, &request, 0);
							cmdUser_params->availableSpaces++;
						}
					}
					else {
						cout << "No spaces occupied at the moment\n";
						Sleep(1000);
					}
				}
				if (!comm.compare("info")) {
					infoMenu(cmdUser_params);
				}
				if (!comm.compare("exit")) {
					exit(0);
				}
			}
		}
	}
}

void cmd(void* pvParameters) {

	//cmd needed parameters 
	cmd_param* cmd_params = (cmd_param*)pvParameters;

	//cmd communication paramaters
	xQueueHandle mbx_cmd = cmd_params->mbx_cmd;

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

	ServerComms requestReceived;

	while (true) {
		xQueueReceive(mbx_cmd, &requestReceived, portMAX_DELAY);
		//semaphore take
		if (!requestReceived.request.compare("goto")) {
			xQueueSend(mbx_xzMov, &requestReceived.location, 0);
			xSemaphoreTake(sem_xzMov, portMAX_DELAY);
		}
		if (!requestReceived.request.compare("add")) {
			xQueueSend(mbx_addStockMov, &requestReceived.location, 0);
			xSemaphoreTake(sem_addStockMov, portMAX_DELAY);
		}
		if (!requestReceived.request.compare("take")) {
			xQueueSend(mbx_takeStockMov, &requestReceived.location, 0);
			xSemaphoreTake(sem_takeStockMov, portMAX_DELAY);
		}
		if (!requestReceived.request.compare("moveto")) {

		}
		//Semaphore give
	}

}

void gotoXZ(void* pvParameters) {
	int x, z, y;
	int coords[2];
	Coords coord;



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
		if (uxSemaphoreGetCount(sem_interruptMode) < 2) {
			xQueueReceive(mbx_xzMov, &coord, portMAX_DELAY);
		}
		while (uxSemaphoreGetCount(sem_interruptMode) >= 2) {
			taskYIELD();
		}

		if (getYPos() != 2) {
			y = 2;
			xQueueSend(mbx_yMov, &y, portMAX_DELAY);
			xSemaphoreTake(sem_yMov, portMAX_DELAY);
		}

		//x = coords[0];
		//z = coords[1];
		x = coord.xcord;
		z = coord.zcord;

		xQueueSend(mbx_xMov, &x, portMAX_DELAY);
		xQueueSend(mbx_zMov, &z, portMAX_DELAY);

		//wait for goto_x completion, synchronization
		xSemaphoreTake(sem_xMov, portMAX_DELAY);
		//wait for goto_z completion, synchronization
		xSemaphoreTake(sem_zMov, portMAX_DELAY);

		if (uxSemaphoreGetCount(sem_interruptMode) < 2) {
			xSemaphoreGive(sem_xzMov);
		}
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
		while (getXPos() != x && uxSemaphoreGetCount(sem_interruptMode) < 2) {
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
		while (getZPos() != z && uxSemaphoreGetCount(sem_interruptMode) < 2) {
			taskYIELD();
		}
		stopZ();
		xSemaphoreGive(sem_zMov);
	}
}

void gotoY(void* pvParameters) {
	int y;
	int crtpos;
	int posSave = 2;

	yMov_param* yMov_params = (yMov_param*)pvParameters;
	xQueueHandle mbx_yMov = yMov_params->mbx_yMov;
	xSemaphoreHandle sem_yMov = yMov_params->sem_yMov;

	while (true) {
		if (uxSemaphoreGetCount(sem_interruptMode) < 2) {
			xQueueReceive(mbx_yMov, &y, portMAX_DELAY);
			posSave = getYPos();
		}
		crtpos = getYPos();

		while (uxSemaphoreGetCount(sem_interruptMode) >= 2) {
			taskYIELD();
			crtpos = posSave;
		}

		if (crtpos > y)
			moveYIn();
		else
			if (crtpos < y)
				moveYOut();
		while (getYPos() != y && uxSemaphoreGetCount(sem_interruptMode) < 2) {
			taskYIELD();
		}
		stopY();

		if (uxSemaphoreGetCount(sem_interruptMode) < 2) {
			xSemaphoreGive(sem_yMov);
		}
	}
}

void gotoZUp(void* pvParameters) {
	zUpMov_param* zUpMov_params = (zUpMov_param*)pvParameters;
	xSemaphoreHandle sem_zUpMov = zUpMov_params->sem_zUpMov;
	xSemaphoreHandle sem_zUpMov_done = zUpMov_params->sem_zUpMov_done;

	while (1) {
		if (uxSemaphoreGetCount(sem_interruptMode) < 2) {
			xSemaphoreTake(sem_zUpMov, portMAX_DELAY);
		}
		while (uxSemaphoreGetCount(sem_interruptMode) >= 2) {
			taskYIELD();
		}
		moveZUp();

		uInt8 p0 = readDigitalU8(0);
		uInt8 p1 = readDigitalU8(1);

		while (getBitValue(p1, 2) && getBitValue(p1, 0) && getBitValue(p0, 6) && uxSemaphoreGetCount(sem_interruptMode) < 2) {
			p0 = readDigitalU8(0);
			p1 = readDigitalU8(1);
			taskYIELD();
		}
		stopZ();
		if (uxSemaphoreGetCount(sem_interruptMode) < 2) {
			xSemaphoreGive(sem_zUpMov_done);
		}
	}
}

void gotoZDown(void* pvParameters) {
	zDownMov_param* zDownMov_params = (zDownMov_param*)pvParameters;
	xSemaphoreHandle sem_zDownMov = zDownMov_params->sem_zDownMov;
	xSemaphoreHandle sem_zDownMov_done = zDownMov_params->sem_zDownMov_done;

	while (1) {
		if (uxSemaphoreGetCount(sem_interruptMode) < 2) {
			xSemaphoreTake(sem_zDownMov, portMAX_DELAY);
		}
		while (uxSemaphoreGetCount(sem_interruptMode) >= 2) {
			taskYIELD();
		}
		moveZDown();

		uInt8 p0 = readDigitalU8(0);
		uInt8 p1 = readDigitalU8(1);

		while (getBitValue(p1, 3) && getBitValue(p1, 1) && getBitValue(p0, 7) && uxSemaphoreGetCount(sem_interruptMode) < 2) {
			p0 = readDigitalU8(0);
			p1 = readDigitalU8(1);
			taskYIELD();
		}
		stopZ();
		if (uxSemaphoreGetCount(sem_interruptMode) < 2) {
			xSemaphoreGive(sem_zDownMov_done);
		}
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
	Coords user;
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

void takeExpired(void* pvParameters) {
	cmd_param* takeExpired_params = (cmd_param*)pvParameters;
	Coords aux;
	StorageRequest nullItem = { NULL };
	while (1) {
		while (uxSemaphoreGetCount(sem_interruptMode) != 4) {
			taskYIELD();
		}
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		//ainda não retira
		int i = 0;
		if (takeExpired_params->expiredFlag) {
			for (int c = 0; c < 3; c++) {
				for (int l = 0; l < 3; l++) {
					if (takeExpired_params->StorageGrid[c][l]->reference != NULL) {
						if (takeExpired_params->StorageGrid[c][l]->expiration <= 0) {
							aux.xcord = c + 1;
							aux.zcord = l + 1;
							ServerComms request;
							request.request = "take";
							request.location = aux;
							xQueueSend(takeExpired_params->mbx_cmd, &request, 0);
							takeExpired_params->StorageGrid[c][l] = &nullItem;
						}
					}
				}
			}
		}
	}
}

void vTaskLeftLED(void* pvParameters) {
	uInt8 p;
	int time_flag = 0;
	int time_flag_save = time_flag;

	LeftLed* LeftLed_param = (LeftLed*)pvParameters;
	xQueueHandle mbx_LeftLed = LeftLed_param->mbx_LeftLed;

	while (TRUE) {
		if (uxQueueMessagesWaiting(mbx_LeftLed)) {
			xQueueReceive(mbx_LeftLed, &time_flag, 0);
			time_flag_save = time_flag;
		}
		if (uxSemaphoreGetCount(sem_interruptMode) >= 2) {
			time_flag = 1;
		}
		else {
			time_flag = time_flag_save;
		}
		if (time_flag) {
			p = readDigitalU8(2);
			taskENTER_CRITICAL();
			setBitValue(&p, 0, 1);
			taskEXIT_CRITICAL();

			taskENTER_CRITICAL();
			writeDigitalU8(2, p);
			taskEXIT_CRITICAL();

			Sleep(LED_PERIOD* time_flag);

			p = readDigitalU8(2);
			taskENTER_CRITICAL();
			setBitValue(&p, 0, 0);
			taskEXIT_CRITICAL();

			taskENTER_CRITICAL();
			writeDigitalU8(2, p);
			taskEXIT_CRITICAL();

			Sleep(LED_PERIOD * time_flag);
		}
	}
}

// RIGHT  0000 0010  0x2
void vTaskRightLED(void* pvParameters) {
	uInt8 p;
	int time_flag = 0;
	int time_flag_save = time_flag;

	RightLed* RightLed_param = (RightLed*)pvParameters;
	xQueueHandle mbx_RightLed = RightLed_param->mbx_RightLed;

	while (TRUE) {
		if (uxQueueMessagesWaiting(mbx_RightLed)) {
			xQueueReceive(mbx_RightLed, &time_flag, 0);
			time_flag_save = time_flag;
		}
		if (uxSemaphoreGetCount(sem_interruptMode) >= 2) {
			time_flag = 1;
		}
		else {
			time_flag = time_flag_save;
		}
		if (time_flag) {
			p = readDigitalU8(2);
			taskENTER_CRITICAL();
			setBitValue(&p, 1, 1);
			taskEXIT_CRITICAL();

			taskENTER_CRITICAL();
			writeDigitalU8(2, p);
			taskEXIT_CRITICAL();

			Sleep(LED_PERIOD * time_flag);

			p = readDigitalU8(2);
			taskENTER_CRITICAL();
			setBitValue(&p, 1, 0);
			taskEXIT_CRITICAL();

			taskENTER_CRITICAL();
			writeDigitalU8(2, p);
			taskEXIT_CRITICAL();

			Sleep(LED_PERIOD * time_flag);
		}
	}
}

/*void vTaskEmergencyStop(void* pvParameters) {
	uInt8 p;

	emergency_param* emergency_params = (emergency_param*)pvParameters;
	RightLed* RightLed_param = emergency_params->RightLed_param;
	LeftLed* LeftLed_param = emergency_params->LeftLed_param;

	xQueueHandle mbx_RightLed = RightLed_param->mbx_RightLed;
	xQueueHandle mbx_LeftLed = LeftLed_param->mbx_LeftLed;
	int time_flag;

	while (TRUE) {
		//Switch 1 -> p1 xx1x xxxx
		//Switch 2 -> p1 x1xx xxxx
		p = readDigitalU8(1);
		if (getBitValue(p, 5) && getBitValue(p, 6)) { // If both pressed, enter Emergency Stop
			uInt8 currentMovement_State = readDigitalU8(2); //save the current grid movement state
			printf("\n\n!! Emergency Stop !!\n\n");

			time_flag = 1;
			xQueueSend(mbx_RightLed, &time_flag, portMAX_DELAY);
			xQueueSend(mbx_LeftLed, &time_flag, portMAX_DELAY);

			vTaskSuspend(left_button_handle); //Depende de como a FR7 for feita
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

					break;
				}
			}

			vTaskResume(left_button_handle); // depende de como fizeres a FR7
			time_flag = 0;
			xQueueSend(mbx_RightLed, &time_flag, portMAX_DELAY);
			xQueueSend(mbx_LeftLed, &time_flag, portMAX_DELAY);
		}
	}
}*/

void StorageCalibration() {
	int c;
	int x = 0, z = 0;
	uInt8 p0;
	uInt8 p1;

	while (x != 1 || z != 1) {
		system("cls");
		cout << "Calibrate the system manually by using the arrow keys";
		c = 0;
		if (_kbhit()) {
			switch (c = _getch()) {
			case 72:
				printf("Up\n");
				if (z == 0) {
					moveZUp();
					while (1) {
						p0 = readDigitalU8(0);
						p1 = readDigitalU8(1);
						if (!getBitValue(p1, 3) || !getBitValue(p1, 1) || !getBitValue(p0, 7)) {
							stopZ();
							z = 1;
							break;
						}
					}
				}
				break;
			case 80:
				printf("Down\n");
				if (z == 0) {
					moveZDown();
					while (1) {
						p0 = readDigitalU8(0);
						p1 = readDigitalU8(1);
						if (!getBitValue(p1, 3) || !getBitValue(p1, 1) || !getBitValue(p0, 7)) {
							stopZ();
							z = 1;
							break;
						}
					}
				}
				break;
			case 75:
				printf("Left\n");
				if (x == 0) {
					moveXLeft();
					while (1) {
						p0 = readDigitalU8(0);
						if (!getBitValue(p0, 2) || !getBitValue(p0, 1) || !getBitValue(p0, 0)) {
							stopX();
							x = 1;
							break;
						}
					}
				}
				break;
			case 77:
				printf("Right\n");
				if (x == 0) {
					moveXRight();
					while (1) {
						p0 = readDigitalU8(0);
						if (!getBitValue(p0, 2) || !getBitValue(p0, 1) || !getBitValue(p0, 0)) {
							stopX();
							x = 1;
							break;
						}
					}
				}
				break;
			}
		}
	}
}

void switch2_rising_isr(ULONGLONG lastTime) {
	xSemaphoreGive(sem_interruptMode);
}

void switch1_rising_isr(ULONGLONG lastTime) {
	xSemaphoreGive(sem_interruptMode);
}

void switch2_falling_isr(ULONGLONG lastTime) {

	if (uxSemaphoreGetCount(sem_interruptMode) == 1) {
		//nothing
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
	}
	else if (uxSemaphoreGetCount(sem_interruptMode) == 2) {
		//Emergency Stop
	}
	else if (uxSemaphoreGetCount(sem_interruptMode) == 3) {
		//reset
		StorageCalibration();
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
	}
	else if (uxSemaphoreGetCount(sem_interruptMode) == 4) {
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
	}
}

void switch1_falling_isr(ULONGLONG lastTime) {

	if (uxSemaphoreGetCount(sem_interruptMode) == 1) {
		//Take expired products from cells
		xSemaphoreGive(sem_interruptMode);
		xSemaphoreGive(sem_interruptMode);
		xSemaphoreGive(sem_interruptMode);
	}
	else if (uxSemaphoreGetCount(sem_interruptMode) == 3) {
		//resume
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
	}
	else if (uxSemaphoreGetCount(sem_interruptMode) == 4) {
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
		xSemaphoreTake(sem_interruptMode, portMAX_DELAY);
	}
}

void myDaemonTaskStartupHook(void) {
	StorageRequest nullItem = { NULL };

	sem_interruptMode = xSemaphoreCreateCounting(4, 0);

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

	LeftLed* my_LeftLed_param = (LeftLed*)pvPortMalloc(sizeof(LeftLed));
	my_LeftLed_param->mbx_LeftLed = xQueueCreate(1, sizeof(int));

	RightLed* my_RightLed_param = (RightLed*)pvPortMalloc(sizeof(RightLed));
	my_RightLed_param->mbx_RightLed = xQueueCreate(1, sizeof(int));

	emergency_param* my_emergency_param = (emergency_param*)pvPortMalloc(sizeof(emergency_param));
	my_emergency_param->LeftLed_param = my_LeftLed_param;
	my_emergency_param->RightLed_param = my_RightLed_param;

	cmd_param* my_cmd_param = (cmd_param*)pvPortMalloc(sizeof(cmd_param));
	my_cmd_param->mbx_cmd = xQueueCreate(90, sizeof(ServerComms));
	my_cmd_param->xzCom_params = my_xzCom_param;
	my_cmd_param->addStockCom_params = my_addStockCom_param;
	my_cmd_param->takeStockCom_params = my_takeStockCom_param;
	my_cmd_param->emergencyStop_params = my_emergency_param;
	//my_cmd_param->StorageGrid = MasterGrid;
	for (int c = 0; c < 3; c++) {
		for (int l = 0; l < 3; l++) {
			my_cmd_param->StorageGrid[c][l] = (StorageRequest*)malloc(sizeof(StorageRequest));
			my_cmd_param->StorageGrid[c][l]->reference = NULL;
		}
	}
	my_cmd_param->availableSpaces = 9;
	my_cmd_param->expiredFlag = 0;


	xTaskCreate(cmd, "cmd", 100, my_cmd_param, 0, NULL);
	xTaskCreate(cmdUser, "cmdUser", 100, my_cmd_param, 0, NULL);
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
	xTaskCreate(vTaskRightLED, "vTaskRightLED", 100, my_RightLed_param, 0, NULL);
	xTaskCreate(vTaskLeftLED, "vTaskLeftLED", 100, my_LeftLed_param, 0, NULL);
	//xTaskCreate(vTaskEmergencyStop, "vTaskEmergencyStop", 100, my_emergency_param, 0, NULL);
	xTaskCreate(timePass, "timePass", 100, my_cmd_param, 0, NULL);
	xTaskCreate(takeExpired, "takeExpired", 100, my_cmd_param, 0, NULL);
	
	attachInterrupt(1, 6, switch2_rising_isr, RISING);
	attachInterrupt(1, 5, switch1_rising_isr, RISING);
	attachInterrupt(1, 6, switch2_falling_isr, FALLING);
	attachInterrupt(1, 5, switch1_falling_isr, FALLING);

	initialisePorts();
	StorageCalibration();
}