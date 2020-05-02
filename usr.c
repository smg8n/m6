
#include<stdio.h>
#include<stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <time.h>
#include <pthread.h>

struct clock{
        unsigned int sec;
        unsigned int nano_sec;
        int maxChild;
        sem_t sem;
};

struct frames{
        int referenceByte;
        int dirtyBit;
        int occupied;
        int fpid;
};

struct pageTable{
        int pid;
        int page[32];
};


struct clock* clk;
struct frames* frame;
struct pageTable* prm;
char* outputFile;
int childSpot;


void printAllFrames(){
	FILE *fp;
	fp = fopen(outputFile,"a");
        fprintf(fp,"\nCurrent memory layout at time %d.%d is:\n",clk->sec,clk->nano_sec);
        fprintf(fp,"\t Occupied \t  RefByte \t DirtyBit\n");
        int i = 0;
        while (i < 256){
                if(frame[i].occupied == 0){
                fprintf(fp,"Frame %d: \tNo\t\t%d\t\t%d\n",i,frame[i].referenceByte,frame[i].dirtyBit);
                }else{
                fprintf(fp,"Frame %d: \tYes\t\t%d\t\t%d\n",i,frame[i].referenceByte,frame[i].dirtyBit);
                }
		i += 1;
        }
        fprintf(fp,"\n");
        fclose(fp);
}



void clearFrame(int a){
	frame[a].referenceByte = -1;
	frame[a].dirtyBit= 0;
	frame[a].occupied = 0;
	frame[a].fpid = -1;
}

// least referenced function to return the frame with the lease reference
int LRU(){
	int i = 0;
	int spot = 0;
	int min = 10000;
	while(i<256){
		if(min > frame[i].referenceByte){
			spot = i;
			min = frame[i].referenceByte;	
		} 
		i += 1;
	}
	return i;
}


int locate(int a){
        int i = 0;
        while (i<18){
                if(prm[i].pid == a){
                        return i;
                }
                i = i+1;
        }
        return 0;
}




int main(int argc, char*argv[]){
	printf("A child process has started\n");
	srand(getpid());
	// pageTable shared memory
	int prmid;
        int prmsize = 18 * sizeof(prm);
        prmid = shmget(0x4234,prmsize,0666|IPC_CREAT);
        if(prmid == -1){
                perror("Shared memory\n");
                return 0;
        }
        prm = shmat(prmid,NULL,0);
        if(prm == (void *)-1){
                perror("Shared memory attach\n");
                return 0;
        }

	// clock shared memory
	int clockid;
        int clocksize = sizeof(clk);
        clockid = shmget(0x5234,clocksize,0666|IPC_CREAT);
        if(clockid == -1){
                perror("Shared memory\n");
                return 0;
        }
        clk = shmat(clockid,NULL,0);
        if(clk== (void *)-1){
                perror("Shared memory attach\n");
                return 0;
        }

	// frame shared memory 256 frames
	int frameid;
        int framesize = 256 * sizeof(frame);
        frameid = shmget(0x6234,framesize,0666|IPC_CREAT);
        if(frameid == -1){
                perror("Shared memory\n");
                return 0;
        }
        frame = shmat(frameid,NULL,0);
        if(frame == (void *)-1){
                perror("Shared memory attach\n");
                return 0;
        }
	
	// locate the child on the table using its pid
	int pid = getpid();
	childSpot = locate(pid);
	FILE *fp;
	outputFile = "logFile";
	int k = 0;
	int j = 0;
	int i = 0;
	while(1){
		fp = fopen(outputFile,"a");
		k+=1;
		// print the frameTable every 50, 1000,2000,3000,4000,5000,6000,7000,8000,9000 memory reference
		if(k==50 || k == 500 || k == 1200 || k == 3000 || k == 4000 || k == 5000 || k == 6000 || k == 7000 || k == 8000 || k == 9000){
			printAllFrames();
		}

		// terminate the process after 100 memory reference and clear all the frames
		if (k >10000){
			printAllFrames();
			fprintf(fp,"\nchild terminated and releasing all memory from frames:\n");
			int g = 0;
			while (g<32){
				if (prm[childSpot].page[g]!= -1){
					fprintf(fp,"%d ",prm[childSpot].page[g]);
					clearFrame(prm[childSpot].page[g]);		
				}
				g+=1;
			}
			fprintf(fp,"\n\n");
			fclose(fp);
			return 0;
		}
		// if the frame is not occupied then the page goes in.
		if(frame[i].occupied == 0&&j<32){
			prm[childSpot].page[j] = i;	
			frame[i].occupied = 1;
			j+=1;
		}	
		// now random memory spot to read or write.
		int memorySpot = rand()%32;
		if(prm[childSpot].page[memorySpot] != -1){
			int row = rand()%100;
			// high chance to read a memory than to write.
			if(row > 65){
				fprintf(fp,"Process %d requesting write of address %d at time %d:%d\n",
						getpid(),&frame[prm[childSpot].page[memorySpot]],clk->sec,clk->nano_sec);
				fprintf(fp,"address %d in frame %d, writing data to frame at time %d:%d\n",
						&frame[prm[childSpot].page[memorySpot]],prm[childSpot].page[memorySpot],clk->sec,clk->nano_sec);
				fclose(fp);
				fp = fopen(outputFile,"a");
				fprintf(fp,"dirty bit of frame %d set, adding additional time to the clock\n",
						prm[memorySpot]);
				frame[prm[childSpot].page[memorySpot]].dirtyBit = 1;
				fclose(fp);
                                fp = fopen(outputFile,"a");
				fprintf(fp,"Indicating to process %d writing has happened to address %d\n",getpid(),&frame[prm[childSpot].page[memorySpot]]);
				fclose(fp);
	
			}else{
				fclose(fp);
                                fp = fopen(outputFile,"a");
				fprintf(fp,"Process %d requesting read of address %d at time %d:%d\n",
                                                getpid(),&frame[prm[childSpot].page[memorySpot]],clk->sec,clk->nano_sec);
				fprintf(fp,"address %d in frame %d, reading data to frame at time %d:%d\n",
                                                &frame[prm[childSpot].page[memorySpot]],prm[childSpot].page[memorySpot],clk->sec,clk->nano_sec);	
				fclose(fp);
			}
			frame[prm[childSpot].page[memorySpot]].referenceByte += 1;
		}
		i+=1;	
	}	


	printf("child is at: %d\n",childSpot);
	printf("user process\n");
	return 0;

}
