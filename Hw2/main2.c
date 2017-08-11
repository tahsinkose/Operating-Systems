#include "writeOutput.h"
#include "auxFuncs.h"
#include <errno.h>

#define BUFFER_SIZE 80
pthread_mutex_t ship_mutex;
pthread_mutex_t ship_mutex2;
pthread_mutex_t entry_mutex;

pthread_mutex_t unload_mutex;
pthread_mutex_t load_mutex;
sem_t ship_semaphor;
sem_t barrier;



void exitDock(int sID,int dockID,struct Dock* docks){
	WriteOutput(sID,dockID,0,LEAVE_DOCK);
	++docks[dockID].track_ships;
	pthread_cond_signal(&(docks[dockID].canEnter));
}

void load(int sID,int dockID,int * remaining_route,struct Ship* ship,int actual_size,int max_size,struct Dock* docks,int remaining_routeSize){
	if(actual_size!=max_size){
		struct timespec waitTime;
		struct timeval now;
		int ul_count;
		gettimeofday(&now,NULL);
		waitTime.tv_sec = now.tv_sec;
		waitTime.tv_nsec = (now.tv_usec + 20000)*1000;
		pthread_mutex_lock(&load_mutex);
		WriteOutput(sID,dockID,0,REQUEST_LOAD);
		ul_count = docks[dockID].unloadcount;
		
		if(!(is_any_cargo_in_route(docks[dockID].cargoes_in_dock,docks[dockID].actual_cargo_size,remaining_route,remaining_routeSize)))
		{
			pthread_mutex_unlock(&load_mutex);
			return ;
		}
		pthread_mutex_unlock(&load_mutex);
		if(ul_count>0){		
			pthread_mutex_lock(&ship_mutex);
			if(pthread_cond_timedwait(&(docks[dockID].canLoad),&ship_mutex,&waitTime) == ETIMEDOUT)
			{
				pthread_mutex_unlock(&ship_mutex);
				return ;
			}
			pthread_mutex_unlock(&ship_mutex);
		}
		
		pthread_mutex_lock(&load_mutex);
		docks[dockID].loadcount++;
		pthread_mutex_unlock(&load_mutex);
		for(int i=0;i<docks[dockID].actual_cargo_size;i++)
		{
			if(is_cargo_in_route(docks[dockID].cargoes_in_dock[i],remaining_route,remaining_routeSize)){
				int hold_destDock;
				pthread_mutex_lock(&load_mutex);	
				WriteOutput(sID,dockID,docks[dockID].cargoes_in_dock[i].cid,LOAD_CARGO);	
				hold_destDock = docks[dockID].cargoes_in_dock[i].destDock;
				/*printf("before dock storage <load>\n");
				PrintCargoes(docks[dockID].cargoes_in_dock,docks[dockID].actual_cargo_size);*/
				docks[dockID].cargoes_in_dock[i].destDock=-1;
				pthread_mutex_unlock(&load_mutex);
				usleep(3000);
				/*printf("after dock storage <load>\n");
				PrintCargoes(docks[dockID].cargoes_in_dock,docks[dockID].actual_cargo_size);
				printf("before ship storage with size %d: \n",ship->storageSize);
				PrintCargoes(ship->storage,ship->storageSize);*/
				ship->storage = loadCargo(docks[dockID].cargoes_in_dock[i],ship->storage,&(ship->storageSize),hold_destDock);
				/*printf("after ship storage with size %d: \n",ship->storageSize);
				PrintCargoes(ship->storage,ship->storageSize);*/
				actual_size = ship->storageSize;//update actual_size
				if(actual_size == max_size){
					pthread_mutex_lock(&load_mutex);
					if(--docks[dockID].loadcount == 0){
						docks[dockID].cargoes_in_dock = regulate_dock_storage(docks[dockID].cargoes_in_dock,&(docks[dockID].actual_cargo_size));
						pthread_cond_broadcast(&(docks[dockID].canUnload));
					}
					pthread_mutex_unlock(&load_mutex);
					return;
				}
						
			}
		}
		pthread_mutex_lock(&load_mutex);
		if(--docks[dockID].loadcount == 0)
			pthread_cond_broadcast(&(docks[dockID].canUnload));
		pthread_mutex_unlock(&load_mutex);
	}
	
}
void unload(struct Ship* ship, int sID,int dockID,struct Dock* docks,int actual_size){

	pthread_mutex_lock(&unload_mutex);
	WriteOutput(sID,dockID,0,REQUEST_UNLOAD);
	pthread_mutex_unlock(&unload_mutex);

	if(docks[dockID].loadcount>0){
		pthread_mutex_lock(&ship_mutex);
		pthread_cond_wait(&(docks[dockID].canUnload),&ship_mutex);
		pthread_mutex_unlock(&ship_mutex);
	}
	pthread_mutex_lock(&unload_mutex);
	docks[dockID].unloadcount++;
	pthread_mutex_unlock(&unload_mutex);
	for(int i=0;i<actual_size;i++){
		if(ship->storage[i].destDock == dockID)
		{
			pthread_mutex_lock(&unload_mutex);
			WriteOutput(sID,dockID,ship->storage[i].cid,UNLOAD_CARGO);
			pthread_mutex_unlock(&unload_mutex);
			usleep(2000);
			pthread_mutex_lock(&unload_mutex);
			/*printf("before dock storage <unload>\n");
			PrintCargoes(docks[dockID].cargoes_in_dock,docks[dockID].actual_cargo_size);*/
			docks[dockID].cargoes_in_dock = loadCargo(ship->storage[i],docks[dockID].cargoes_in_dock,&(docks[dockID].actual_cargo_size),ship->storage[i].destDock);
			/*printf("after dock storage <unload>\n");
			PrintCargoes(docks[dockID].cargoes_in_dock,docks[dockID].actual_cargo_size);*/
			pthread_mutex_unlock(&unload_mutex);
			/*printf("before ship storage with size: %d \n",ship->storageSize);
			PrintCargoes(ship->storage,actual_size);*/
			ship->storage = removeCargo(ship->storage,i,actual_size); actual_size--;i--;
			/*printf("after ship storage with size: %d \n",ship->storageSize);
			PrintCargoes(ship->storage,actual_size);*/
		}
	}
	pthread_mutex_lock(&unload_mutex);
	if(--docks[dockID].unloadcount == 0)
	{
	
		pthread_cond_broadcast(&(docks[dockID].canLoad));
	}
	pthread_mutex_unlock(&unload_mutex);
}

void enter(int sID,int dockID,struct Dock* docks)
{
	
	WriteOutput(sID,dockID,0,REQUEST_ENTRY);
	pthread_mutex_lock(&ship_mutex);
	
	docks[dockID].track_ships--;
	pthread_mutex_unlock(&ship_mutex);
	if(docks[dockID].track_ships<0)
	{
		pthread_mutex_lock(&ship_mutex);
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

	sem_post(&ship_semaphor);
	WriteOutput(sh->sid,0,0,CREATE_SHIP);
	ttime = sh->arrivalTime - (GetTimestamp()/1000);
	//ttime = sh->arrivalTime;
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
		        load(sh->sid,sh->dockList[i],(sh->dockList)+i+1,sh,sh->storageSize,sh->maxCargo,dc,(sh->length)-1);
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
	int numDocks,numShips,numCargoes;
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

	for(int i=0;i<numCargoes;i++)
	{
		docks[cargoes[i].initialDock].cargoes_in_dock = loadCargo(cargoes[i],docks[cargoes[i].initialDock].cargoes_in_dock,&(docks[cargoes[i].initialDock].actual_cargo_size),cargoes[i].destDock);
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
