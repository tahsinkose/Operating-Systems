#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>

struct tQueue {
	pthread_t tid;
	struct tQueue *next;
};

int queuesize(struct tQueue* head);
struct tQueue * enqueue(pthread_t data,struct tQueue* head);
struct tQueue* dequeue(struct tQueue* head);	
bool empty(struct tQueue* head);
pthread_t headElement(struct tQueue* head);
void PrintQueue(struct tQueue* head);
	

struct Dock {
	int id;
	int ship_capacity; //for ships
	pthread_cond_t canEnter;
	int track_ships;
	struct tQueue* head; //queue for handling FIFO scheduling of threads
	pthread_cond_t canLoad;
	pthread_cond_t canUnload;
	int loadcount;
	int unloadcount;
	pthread_mutex_t dock_mutex;
};

struct Ship {
	int sid;
	int travelTime;
	int maxCargo;
	int arrivalTime;
	int length;
	int *dockList;
	struct Cargo* storage;
	int storageSize;
	
};

struct Cargo {
	int cid;
	int initialDock;
	int destDock;
	bool isLoaded;
};

struct Combined {
	struct Dock* docks;
	struct Ship* ships;
	struct Cargo* cargoes;
	int shipIndex;
};




struct Dock * HandleSecondLine(char *line,int nD);
void HandleFirstLine(char *line,int *nD,int *nS,int *nC);
struct Ship createShip(char *line,int id);
struct Ship* sort(struct Ship* toBeSorted,int nS);
struct Cargo createCargo(char *line,int id);
void PrintShips(struct Ship * sh,int len);
void PrintCargoes(struct Cargo * cr,int len);
void PrintDocks(struct Dock * dc,int len);
bool checkStorage(struct Cargo * storage,int size,int dockID);
struct Cargo* removeCargo(struct Cargo *storage,int indexToDelete,int size);
struct Cargo* loadCargo(struct Cargo cargo,struct Cargo * dCargoes,int *dSize);
bool is_cargo_in_route(struct Cargo cargo,int cSize, int dockID,int *route,int routeSize);
bool is_any_cargo_in_route(struct Cargo* cargoes,int cSize,int dockID,int *route,int routeSize);
