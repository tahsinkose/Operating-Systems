#include "writeOutput.h"
#include "auxFuncs.h"
#include <errno.h>

#define BUFFER_SIZE 1000
pthread_mutex_t ship_mutex;
pthread_mutex_t ship_mutex2;
pthread_mutex_t entry_mutex;

pthread_mutex_t unload_mutex;
pthread_mutex_t load_mutex;
sem_t ship_semaphor;
sem_t barrier;

int numDocks,numShips,numCargoes;

void exitDock(int sID,int dockID,struct Dock* docks){
	WriteOutput(sID,dockID,0,LEAVE_DOCK);
	++docks[dockID].track_ships;
	pthread_cond_signal(&(docks[dockID].canEnter));
}

void load(int sID,int dockID,int * remaining_route,struct Ship* ship,int actual_size,int max_size,struct Dock* docks,int remaining_routeSize,struct Cargo* cargoes){
	if(actual_size!=max_size){
		struct timespec waitTime;
		struct timeval now;
		int ul_count;
		gettimeofday(&now,NULL);
		waitTime.tv_sec = now.tv_sec;
		waitTime.tv_nsec = (now.tv_usec + 20000)*1000;
		pthread_mutex_lock(&ship_mutex);
		WriteOutput(sID,dockID,0,REQUEST_LOAD);
		ul_count = docks[dockID].unloadcount;
		
		if(!(is_any_cargo_in_route(cargoes,numCargoes,dockID,remaining_route,remaining_routeSize)))
		{
			pthread_mutex_unlock(&ship_mutex);
			return ;
		}
		pthread_mutex_unlock(&ship_mutex);
		if(ul_count>0){		
			pthread_mutex_lock(&ship_mutex);
			if(pthread_cond_timedwait(&(docks[dockID].canLoad),&ship_mutex,&waitTime) == ETIMEDOUT)
			{
				pthread_mutex_unlock(&ship_mutex);
				return ;
			}
			pthread_mutex_unlock(&ship_mutex);
		}
		
		
		pthread_mutex_lock(&ship_mutex);
		docks[dockID].loadcount++;
		pthread_mutex_unlock(&ship_mutex);
		for(int i=0;i<numCargoes;i++)
		{
			if(cargoes[i].initialDock == dockID){//then cargo is in the deck
				if(is_cargo_in_route(cargoes[i],numCargoes,dockID,remaining_route,remaining_routeSize)){ //if it is in the route for ship
					pthread_mutex_lock(&ship_mutex);
					if(!(cargoes[i].isLoaded)){
						WriteOutput(sID,dockID,cargoes[i].cid,LOAD_CARGO);	
						cargoes[i].isLoaded = true;
						pthread_mutex_unlock(&ship_mutex);
						usleep(3000);
						ship->storage = loadCargo(cargoes[i],ship->storage,&(ship->storageSize));
						actual_size = ship->storageSize;//update actual_size
						if(actual_size == max_size){
							pthread_mutex_lock(&ship_mutex);
							if(--docks[dockID].loadcount == 0){
								pthread_cond_broadcast(&(docks[dockID].canUnload));
							}
							pthread_mutex_unlock(&ship_mutex);
							return;
						}
					}
					else
						pthread_mutex_unlock(&ship_mutex);
				}
			}
		}
		pthread_mutex_lock(&ship_mutex);
		if(--docks[dockID].loadcount == 0)
			pthread_cond_broadcast(&(docks[dockID].canUnload));
		pthread_mutex_unlock(&ship_mutex);
	}
	
}
void unload(struct Ship* ship, int sID,int dockID,struct Dock* docks,int actual_size){

	pthread_mutex_lock(&ship_mutex);
	WriteOutput(sID,dockID,0,REQUEST_UNLOAD);
	pthread_mutex_unlock(&ship_mutex);

	if(docks[dockID].loadcount>0){
		pthread_mutex_lock(&ship_mutex);
		pthread_cond_wait(&(docks[dockID].canUnload),&ship_mutex);
		pthread_mutex_unlock(&ship_mutex);
	}
	pthread_mutex_lock(&ship_mutex);
	docks[dockID].unloadcount++;
	pthread_mutex_unlock(&ship_mutex);
	for(int i=0;i<actual_size;i++){
		pthread_mutex_lock(&ship_mutex);
		if(ship->storage[i].destDock == dockID)
		{
			WriteOutput(sID,dockID,ship->storage[i].cid,UNLOAD_CARGO);
			pthread_mutex_unlock(&ship_mutex);
			usleep(2000);
			ship->storage = removeCargo(ship->storage,i,actual_size); ship->storageSize = --actual_size;i--;
		}
		else
			pthread_mutex_unlock(&ship_mutex);
	}
	pthread_mutex_lock(&ship_mutex);
	if(--docks[dockID].unloadcount == 0)
	{
	
		pthread_cond_broadcast(&(docks[dockID].canLoad));
	}
	pthread_mutex_unlock(&ship_mutex);
}

void enter(int sID,int dockID,struct Dock* docks)
{
	pthread_mutex_lock(&ship_mutex);
	WriteOutput(sID,dockID,0,REQUEST_ENTRY);
	
	
	docks[dockID].track_ships--;
	if(docks[dockID].track_ships<0)
	{
		docks[dockID].head = enqueue(pthread_self(),docks[dockID].head);
		pthread_mutex_unlock(&ship_mutex);

		pthread_mutex_lock(&ship_mutex);
		pthread_cond_wait(&(docks[dockID].canEnter), &ship_mutex);
		if(pthread_self() == headElement(docks[dockID].head)) //then this is the head thread
		{
			
			docks[dockID].head = dequeue(docks[dockID].head);
			pthread_mutex_unlock(&ship_mutex);
			
		}
		else{//the thread which won the race is not the head thread.So devise a waiting mechanism.
			
			pthread_cond_signal(&(docks[dockID].canEnter));
			pthread_mutex_unlock(&ship_mutex);

			pthread_mutex_lock(&ship_mutex);
			pthread_cond_wait(&(docks[dockID].canEnter), &ship_mutex);
			pthread_mutex_unlock(&ship_mutex);
		}
		pthread_mutex_unlock(&entry_mutex);	
	}
	else
		pthread_mutex_unlock(&ship_mutex);
	WriteOutput(sID,dockID,0,ENTER_DOCK);
}
void travel(int sID,int dockID,int ttime)
{
	
	usleep(1000*ttime);
	
}
void * Ship_thread_main(void * arg){

	pthread_mutex_lock(&ship_mutex);
	int ttime;
	struct Combined* hold_args = (struct Combined *)arg;
	int sIndex = hold_args->shipIndex;
	struct Ship* sh = (struct Ship *) &(hold_args->ships[sIndex]);
	struct Dock* dc = hold_args->docks;
	sh->storage= NULL;
	sh->storageSize = 0;
	struct Cargo* cr = hold_args->cargoes;
	sem_post(&ship_semaphor);
	WriteOutput(sh->sid,0,0,CREATE_SHIP);
	ttime = sh->arrivalTime - (GetTimestamp()/1000);
	pthread_mutex_unlock(&ship_mutex);
	
	for(int i=0;i<sh->length;i++)
	{
		bool anyCargoWithDockID;
		bool notLastEntryOnRoute;
		travel(sh->sid,sh->dockList[i],ttime);
		ttime = sh->travelTime;
		enter(sh->sid,sh->dockList[i],dc);
		anyCargoWithDockID = checkStorage(sh->storage,sh->storageSize,sh->dockList[i]);
		if(anyCargoWithDockID)
		{
			
			unload(sh,sh->sid,sh->dockList[i],dc,sh->storageSize);
			
		}
		notLastEntryOnRoute = ((i+1) != sh->length);
		
		
		if(notLastEntryOnRoute){
		        load(sh->sid,sh->dockList[i],(sh->dockList)+i+1,sh,sh->storageSize,sh->maxCargo,dc,(sh->length)-1,cr);
			exitDock(sh->sid,sh->dockList[i],dc);
			
		}
		else
			exitDock(sh->sid,sh->dockList[i],dc);	
	}
	

	pthread_mutex_lock(&ship_mutex2);
	WriteOutput(sh->sid,0,0,DESTROY_SHIP);
	sem_post(&barrier);
	pthread_mutex_unlock(&ship_mutex2);
}



int main()
{
	pthread_t tid;

	pthread_attr_t detach_attr;
	pthread_attr_init(&detach_attr);
	pthread_attr_setdetachstate(&detach_attr,PTHREAD_CREATE_DETACHED);

	pthread_mutex_init(&ship_mutex, NULL);
	pthread_mutex_init(&ship_mutex2, NULL);
	pthread_mutex_init(&entry_mutex, NULL);
	pthread_mutex_init(&unload_mutex, NULL);
	pthread_mutex_init(&load_mutex, NULL);
	sem_init(&ship_semaphor,0,0);
	sem_init(&barrier,0,0);
	char * buffer, *line;
	
	struct Dock * docks;
	struct Ship * ships;
	struct Cargo * cargoes;
	struct Combined * arg;
	buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
	line = fgets(buffer,BUFFER_SIZE,stdin);

	HandleFirstLine(line,&numDocks,&numShips,&numCargoes);

	line = fgets(buffer,BUFFER_SIZE,stdin);
	
	docks = HandleSecondLine(line,numDocks);
	

	ships = (struct Ship *) malloc(numShips * sizeof(struct Ship));
	for(int i = 0;i<numShips;i++)
	{
		line = fgets(buffer,BUFFER_SIZE,stdin);
		ships[i] = createShip(line,i);
	}
	//PrintShips(ships,numShips);
	ships = sort(ships,numShips);
	cargoes = (struct Cargo *) malloc(numCargoes * sizeof(struct Cargo));
	for(int i=0;i<numCargoes;i++)
	{	
		line = fgets(buffer,BUFFER_SIZE,stdin);
		cargoes[i] = createCargo(line,i);
	}




	//PrintDocks(docks,numDocks);
	//PrintCargoes(cargoes,numCargoes);
	arg = (struct Combined *) malloc((numShips * sizeof(struct Ship)) + (numCargoes * sizeof(struct Cargo)) + (numDocks * sizeof(struct Dock)));
	arg->docks = docks;
	arg->ships = ships;
	arg->cargoes = cargoes;
	InitWriteOutput();
	for(int i=0;i<numShips;i++){
		arg->shipIndex=i;
		pthread_create(&tid,&detach_attr,Ship_thread_main,(void *)arg);
		sem_wait(&ship_semaphor);
		
	}
	for(int i=0;i<numShips;i++){
		sem_wait(&barrier);
	}
	return 0;
}
