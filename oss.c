
#include<stdio.h>
#include<stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h> 
#include <pthread.h>
#define maxTimeNS 1000000000
#define maxProcess 18
int maxTime = 5;

// structure for a clock.
struct clock{
        unsigned int sec;
        unsigned int nano_sec;
	int maxChild; 
	int FIFO;
};

// frame table with referecebyte,page number, dirty bit, pid, and indication of occupied.
struct frames{
	int referenceByte;
	int dirtyBit;
	int occupied;
	int fpid;
};

// page table with pid to show which process it belongs to and the pages it has.
struct pageTable{
        int pid;
	int page[32];
};


struct clock* clk;
struct frames* frame;
struct pageTable* prm;
char* outputFile;


void printAllFrames(){
	FILE *fp;
        fp = fopen(outputFile,"a");
	fprintf(fp,"\nCurrent memory layout at time %d.%d is:\n",clk->sec,clk->nano_sec);
	fprintf(fp,"\t Occupied \t  RefByte \t DirtyBit\n");
	int i = 0;
	while (i < 256){
		if(frame[i].occupied == 0){
		fprintf(fp,"Frame %d: \tNo\t\t%d\t\t%d\n",i,frame[i].occupied,frame[i].referenceByte,frame[i].dirtyBit);
		}else{
		fprintf(fp,"Frame %d: \tYes\t\t%d\t\t%d\n",i,frame[i].occupied,frame[i].referenceByte,frame[i].dirtyBit);
		}i += 1;
	}	
	fprintf(fp,"\n");		
	fclose(fp);
}

void printPT(){
	FILE *fp;
	fp = fopen(outputFile,"a");
	int i =0;
	while (i < 18){
		
		fprintf(fp,"pid: %d\n",prm[i].pid);
		i += 1;
	}	
	fclose(fp);
}

void init_frames(){
	int i = 0;
	while(i < 256){
		frame[i].referenceByte = 0;
                frame[i].dirtyBit = 0;
                frame[i].occupied = 0;
                frame[i].fpid = -1;
                i += 1;
	}
}

void init_clock(){
	clk -> nano_sec = 0;
	clk -> sec = 0;
	clk -> maxChild = 18;
	clk -> FIFO = 0;
}


void init_pageTable(){
	int i = 0;
	while (i < 18){
		prm[i].pid = -1;
		int j = 0;
		while(j < 32){
			prm[i].page[j] = -1;
			j += 1;
		}
		i += 1;
	}

}

void alarmHandler(int sig){
    	
	FILE *fp;
        fp = fopen(outputFile,"a");
        fprintf(fp,"End of program, all processes terminated after max",maxTime);
        fclose(fp);
        
	int i=0;
        while(i <18){
                if(prm[i].pid>-1){
                        kill(prm[i].pid,SIGTERM);
                }
                i=i+1;
        }
        printf("\nProgram terminated after %d seconds, review results in logFile\n",maxTime);
        kill(getpid(),SIGTERM);
}




int main(int argc, char*argv[]){
	int opt;
	int vflag;
	outputFile = "logFile"; 
	while((opt = getopt(argc,argv,"ht:"))!=-1){
                switch(opt){
                        case 'h':
                                printf("\n-h is used for listing the available command line arguments\n");
                                printf("the command line options are -h, -t\n");
                                printf("use -t command followed by an argument to change the max run time\n");
                                printf("\nprogram terminated\n\n");
                                return 0;
                        case 'v':
                                vflag = 1;
                                break;
                        case 't':
                                maxTime = atoi(optarg);
                                printf("\nDefault running time has changed from 5 seconds to %d seconds\n\n",maxTime);
                                break;
                        case '?':
                                printf("\n Invalid arguments, please use option -h for help, program terminated\n");
                                return 0;
                }
        }
	alarm(maxTime);

	// setup the PageTable in shared memory
		
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
	
	// setup clock shared memory
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

	// set up the frameTable in shared memory
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
	
	// initialize all the shared memory stuff	
	init_frames();	
	init_clock();
	init_pageTable();
	
	// print all frames at its natural state;
	printAllFrames();
	int picked = 0;
	unsigned int s;
	unsigned int ns;
	// infinite loop in OS
	while(1){
		// pick a random time to fork a process until 18 processes
		if(picked == 0){
                        s = clk->sec;
                        ns = clk->nano_sec + rand()%(10000000000)+1;
                        if(ns>= maxTimeNS){
                                s = s+1;
                                ns = ns-maxTimeNS;
                        }
                        picked = 1;
                }
		
		// increment the clock
		clk->nano_sec = clk->nano_sec +25000;
                if(clk->nano_sec >= maxTimeNS){
                        clk->sec = clk->sec +1;
                        clk->nano_sec =0;
                }
		if(clk->sec >= s && picked == 1){
                        if(clk->nano_sec >= ns){
                                picked = 0;
                                if(clk->maxChild>0){
                                        int i =0;
					// finding a empty spot for child process
                                        while(i<18){
                                                if(prm[i].pid == -1){
                                                        break;
                                                }
                                                i+=1;
                                        }
                                        clk->maxChild = clk->maxChild -1;
                                        int child_pid = fork();
                                      
					// if in child then execute user else store the child process pid in table;
					if(child_pid <=0){
                                                execvp("./user",NULL);
                                                exit(0);
                                        }else{
                                                prm[i].pid=child_pid;
                                        }
                                }
                        }
                }

	}


	return 0;
}
