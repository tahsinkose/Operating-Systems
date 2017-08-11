#include "auxFuncs.h"

struct Dock * HandleSecondLine(char *line,int nD)
{
	struct Dock * tmpDocks= (struct Dock *)malloc(nD * sizeof(struct Dock));
	int i;
	char *token=strtok(line," ");
	
	for(i=0;i<nD;i++)
	{
		if(token!=NULL){
			tmpDocks[i].id = i;
        		sscanf(token, "%d", &(tmpDocks[i].ship_capacity));
			tmpDocks[i].track_ships = tmpDocks[i].ship_capacity;
			pthread_cond_init(&(tmpDocks[i].canEnter),NULL);
			pthread_cond_init(&(tmpDocks[i].canLoad),NULL);	
			pthread_cond_init(&(tmpDocks[i].canUnload),NULL);
			pthread_mutex_init(&(tmpDocks[i].dock_mutex),NULL);
			tmpDocks[i].head = NULL;
			tmpDocks[i].loadcount = 0;	
			tmpDocks[i].unloadcount = 0;	
         		token=strtok(NULL," ");
    		}
	}
	return tmpDocks;	
}
void HandleFirstLine(char *line,int *nD,int *nS,int *nC)
{
	char *token=strtok(line," ");
	if(token!=NULL){
        	sscanf(token, "%d", nD);
         	token=strtok(NULL," ");
    	}
	if(token!=NULL){
        	sscanf(token, "%d", nS);
         	token=strtok(NULL," ");
    	}
	if(token!=NULL){
        	sscanf(token, "%d", nC);
         	token=strtok(NULL," ");
    	}
}

struct Ship createShip(char *line,int id){
	struct Ship ship;
	int counter;
	ship.sid = id;
	
	
	char *token=strtok(line," ");
	if(token!=NULL){
        	sscanf(token, "%d", &(ship.travelTime));
         	token=strtok(NULL," ");
    	}
	if(token!=NULL){
        	sscanf(token, "%d", &(ship.maxCargo));
         	token=strtok(NULL," ");
    	}
	
	if(token!=NULL){
        	sscanf(token, "%d",&(ship.arrivalTime));
         	token=strtok(NULL," ");
    	}
	if(token!=NULL){
        	sscanf(token, "%d",&(ship.length));
         	token=strtok(NULL," ");
    	}
	counter = ship.length;
	ship.dockList = (int *) malloc(counter * sizeof(int));
	for(int i=0;i<counter;i++)
	{
		sscanf(token, "%d",&(ship.dockList[i]));
         	token=strtok(NULL," ");
	}
	return ship;
}


struct Ship* sort(struct Ship* toBeSorted,int nS){
	struct Ship temp;
	int pos;
	for(int i=0;i<nS-1;i++){
		pos = i;
		for(int j=i+1;j<nS;j++)
		{
			if(toBeSorted[pos].arrivalTime > toBeSorted[j].arrivalTime)
				pos = j;
		}
		if(pos!=i)
		{
			temp = toBeSorted[i];
			toBeSorted[i] = toBeSorted[pos];
			toBeSorted[pos] = temp;
		}
	}
	return toBeSorted;
}	



void PrintShips(struct Ship * sh,int len){
	for(int i = 0;i<len;i++)
	{
		printf("Ship %d : travel_time=%d , maxCargo=%d, arrivalTime = %d, length = %d ",sh[i].sid,sh[i].travelTime,sh[i].maxCargo,sh[i].arrivalTime,sh[i].length);
		int hold = sh[i].length;
		for(int j=0;j<hold;j++)
			printf("Dock [%d] en route = %d, ",j,sh[i].dockList[j]);
		printf("\n");
	}
}


struct Cargo createCargo(char *line,int id){
	struct Cargo cargo;
	cargo.cid = id;
	cargo.isLoaded = false;
	char *token=strtok(line," ");
	if(token!=NULL){
        	sscanf(token, "%d", &(cargo.initialDock));
         	token=strtok(NULL," ");
    	}
	if(token!=NULL){
        	sscanf(token, "%d", &(cargo.destDock));
         	token=strtok(NULL," ");
    	}
	return cargo;
}

struct Cargo* loadCargo(struct Cargo cargo,struct Cargo * dCargoes,int* dSize){
	int x = *dSize;
	struct Cargo *new_dCargoes = realloc(dCargoes,(x+1)*sizeof(struct Cargo));
	new_dCargoes[x] = cargo;
	x++;
	*dSize = x;
	return new_dCargoes;
}
	

bool is_cargo_in_route(struct Cargo cargo,int cSize, int dockID,int *route,int routeSize){
	for(int i=0;i<routeSize;i++){
		if(cargo.destDock == route[i])
			return true;
	}
	return false;
}

bool is_any_cargo_in_route(struct Cargo* cargoes,int cSize,int dockID,int *route,int routeSize){
	for(int i=0;i<cSize;i++){
		if(!cargoes[i].isLoaded && cargoes[i].initialDock == dockID){
			for(int j=0;j<routeSize;j++)
				if(cargoes[i].destDock == route[j])
					return true;
		}
	}
	return false;
}



	

void PrintCargoes(struct Cargo * cr,int len){
	for(int i = 0;i<len;i++)
		printf("Cargo %d : initial_dock=%d , destinationDock=%d\n",cr[i].cid,cr[i].initialDock,cr[i].destDock);
}
void PrintDocks(struct Dock * dc,int len){
	for(int i = 0;i<len;i++)
	{
		printf("Dock %d : capacity %d , ",dc[i].id,dc[i].ship_capacity);
		
	}
}


bool checkStorage(struct Cargo * storage,int size,int dockID){
	if(size==0)
		return false;
	else{
		for(int i=0;i<size;i++)
		{
			if(storage[i].destDock == dockID)	
				return true;
		}
		return false;
	}
}


struct Cargo* removeCargo(struct Cargo *storage,int indexToDelete,int size){
	
	struct Cargo * newStorage = (struct Cargo *) malloc((size-1)*sizeof(struct Cargo));
	for(int i=0;i<size;i++)
	{
		if(i==indexToDelete)
		{
			for( ; i<size-1;i++)
				newStorage[i] = storage[i+1];
			break;
		}
		else
			newStorage[i] = storage[i];
	}
	free(storage);
	return newStorage;
}		
		
int queuesize(struct tQueue* head)
{
	int count = 0;
	struct tQueue* temp;
	temp = head;
	if(head==NULL)
		return 0;
	else{
		while(temp->next)
			count++;
		return count+1;
	}
}

struct tQueue * enqueue(pthread_t data,struct tQueue* head){
	if(head==NULL){
		head = (struct tQueue *)malloc(sizeof(struct tQueue));
		head->next = NULL;
		head->tid = data;
	}
	else{
		struct tQueue* temp;	
		temp = head;
		while(temp->next)
			temp = temp->next;
		temp->next = (struct tQueue *)malloc(sizeof(struct tQueue));
		temp->next->tid = data;
		temp->next->next = NULL;
	}
	return head;
}

struct tQueue* dequeue(struct tQueue* head){
	
	struct tQueue* newHead;
	if(head==NULL)
		return NULL;
	else{
		newHead = head->next;
		free(head);
	}
	return newHead;
}	
void PrintQueue(struct tQueue* head){
	printf("Printing Queue:--------\n");
	struct tQueue* temp = head;
	while(temp){
		printf(" tid : %lu \n",temp->tid);
		temp = temp->next;
	}
}

bool empty(struct tQueue* head)
{
	return head==NULL;
}

pthread_t headElement(struct tQueue* head)
{
	return head->tid;
}
