#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <time.h>

#include "queue.h"
#include "shmem.h"
const int pagesize = 1;
const int procsize = 32;
const int memsize = 256;
struct 
{
	long msgtype;
	char message[100];

} msg;

struct frame
{
	int pid;
	unsigned dirtybit : 1;
	unsigned referbit : 8;

};

struct page
{
	int frame;
	int swaps;
	float weight;
};

struct pagetable
{
	struct page frames[32];

};

struct memory
{
	struct frame frameinstance[256];
	struct pagetable pagetableinstance[18];
};
struct sigaction satime;
struct sigaction sactrl;
struct Queue *waitingroom;
struct memory mem;
shmem *smseg;
FILE* outlog;
int sipcid;
int tooss;
int tousr;
int plist[PCAP];
int scheme = 0;
int pagefaults;
int reqcount;
int lcount = 0;
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
		(byte & 0x80 ? '1' : '0'), \
		(byte & 0x40 ? '1' : '0'), \
		(byte & 0x20 ? '1' : '0'), \
		(byte & 0x10 ? '1' : '0'), \
		(byte & 0x08 ? '1' : '0'), \
		(byte & 0x04 ? '1' : '0'), \
		(byte & 0x02 ? '1' : '0'), \
		(byte & 0x01 ? '1' : '0')
void optset(int, char **);
void helpme();
static int satimer();
void killctrl(int, siginfo_t *, void *);
void killtime(int, siginfo_t *, void *);

void shminit();
void msginit();
void tabinit();
void arrinit();
void manager();
void moppingup();

void clockinc(simclock *, int, int);

void overlay(int, int);
int findaseat();
void shifter();
void printer();
void insertpage(int, int);
int main(int argc, char *argv[])
{
    optset(argc, argv);

	satime.sa_sigaction = killtime;
	sigemptyset(&satime.sa_mask);
	satime.sa_flags = 0;
	sigaction(SIGALRM, &satime, NULL);

    sactrl.sa_sigaction = killctrl;
	sigemptyset(&sactrl.sa_mask);
	sactrl.sa_flags = 0;
	sigaction(SIGINT, &sactrl, NULL);

	outlog = fopen("log.txt", "w");
	if(outlog == NULL)
	{
		perror("\noss: error: failed to open output file");
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));

	shminit();
	msginit();
	tabinit();

	if(scheme == 1)
	{
		arrinit();
	}

	sem_wait(&(smseg->clocksem));
	smseg->smtime.secs = 0;
	smseg->smtime.nans = 0;
	sem_post(&(smseg->clocksem));

	manager();
	
	moppingup();

return 0;
}
void manager()
{
	int status;
        int ecount = 0;
	int acount = 0;

	pid_t pids[PCAP];
	pid_t tpid;

	simclock forktime = {0, 0};
	/* deploy random time between 1 and 500 milliseconds of logical
	   clock, which is, at this point, initiated to { 0, 0 }     */
	int nextfork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
	
	sem_wait(&(smseg->clocksem));
	clockinc(&forktime, 0, nextfork);
	sem_post(&(smseg->clocksem));

	waitingroom = queueinit(200);

	printf("\n[oss]: running simulation\n[oss]: on memory request scheme %i\n[oss]: ctrl-c to terminate\n", scheme);

	while(1)
	{
		/* advance clock by 10 milliseconds,
		   or 0.01 seconds at the beginning
		   of each loop to simulate time  */
		sem_wait(&(smseg->clocksem));
		clockinc(&(smseg->smtime), 0, 10000);
		sem_post(&(smseg->clocksem));

		if(acount < PCAP && ((smseg->smtime.secs > forktime.secs) || (smseg->smtime.secs == forktime.secs && smseg->smtime.nans >= forktime.nans)))
		{
			/* set previous user process fork t
			ime to current logical clock     */
			forktime.secs = smseg->smtime.secs;
			forktime.nans = smseg->smtime.nans;
			/* then add random time between 1 and 500 milliseconds of
			current logical clock to set chronomatic deployment varia
			ble for upcoming random time user process fork         */
			nextfork = (rand() % (500000000 - 1000000 + 1)) + 1000000;
			
			sem_wait(&(smseg->clocksem));
			clockinc(&forktime, 0, nextfork);
			sem_post(&(smseg->clocksem));
			
			int seat = findaseat() + 1;
			if(seat - 1 > -1)
			{
	                        pid_t cpid = pids[seat - 1] = fork();
				if(pids[seat - 1] == 0)
				{
					overlay(seat, scheme);
				}

				acount++;

				if(lcount < 100000)
				{
					lcount++;
					fprintf(outlog, "\n[oss]: [spawn process]     -> [pid: %i] [time: %is:%ins]", cpid, smseg->smtime.secs, smseg->smtime.nans);
				}
			}
		}

		if(msgrcv(tooss, &msg, sizeof(msg), 0, IPC_NOWAIT) > -1)
		{
			int proc = msg.msgtype;

			if(strcmp(msg.message, "TERMINATE") == 0)
			{
				while(waitpid(pids[proc - 1], NULL, 0) > 0);

				plist[proc - 1] = 0;
				ecount++;
				acount--;
				
				if(lcount < 100000)
				{
					lcount++;
					fprintf(outlog, "\n[oss]: [process terminated]     -> [pid: %i] [time: %is:%ins]", proc, smseg->smtime.secs, smseg->smtime.nans);
				}
			}

			else if(strcmp(msg.message, "WRITE") == 0)
			{
				reqcount++;
				msgrcv(tooss, &msg, sizeof(msg), proc, 0);
				int writer = atoi(msg.message);
				
				if(lcount < 100000)
				{
					lcount++;
					fprintf(outlog, "\n[oss]: [write request]     -> [pid: %i] requesting write of address [0x%-5x] at [time: %is:%ins]", proc, writer * 1000, smseg->smtime.secs, smseg->smtime.nans);
				}

				if(mem.pagetableinstance[proc - 1].frames[writer].frame == -1)
				{
					if(lcount < 100000)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [page fault]     -> address [0x%-5x] is not in a frame, at [time: %is:%ins]", writer * 1000, smseg->smtime.secs, smseg->smtime.nans);
					}	

					pagefaults++;
					smseg->procs[proc - 1].pid = proc - 1;
					smseg->procs[proc - 1].unblock = 1;
					smseg->procs[proc - 1].frame = writer;

					int interim = (rand() & (140000000 - 10000000 + 1)) + 10000000;
					
					sem_wait(&(smseg->clocksem));
					clockinc(&smseg->procs[proc - 1].unblockclock, 0, interim);
					sem_post(&(smseg->clocksem));
					enqueue(waitingroom, proc);
				}
			}

			else if(strcmp(msg.message, "REQUEST") == 0)
			{
				reqcount++;
				msgrcv(tooss, &msg, sizeof(msg), proc, 0);
				int pageid = atoi(msg.message);
				if(lcount < 100000)
				{
					lcount++;
					fprintf(outlog, "\n[oss]: [read request]     ->  [pid: %i] requesting read of address [0x%-5x] at [time: %is:%ins]", proc, pageid * 1000, smseg->smtime.secs, smseg->smtime.nans);
				}	

				if(mem.pagetableinstance[proc - 1].frames[pageid].frame == -1)
				{
					if(lcount < 100000)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [page fault]     -> address [0x%-5x] is not in a frame, at [time: %is:%ins]", pageid * 1000, smseg->smtime.secs, smseg->smtime.nans);
					}

					pagefaults++;
					
					smseg->procs[proc - 1].pid = proc - 1;
					smseg->procs[proc - 1].unblock = 0;
					smseg->procs[proc - 1].frame = pageid;

					int interim = (rand() & (140000000 - 10000000 + 1)) + 10000000;

					sem_wait(&(smseg->clocksem));
					clockinc(&smseg->procs[proc - 1].unblockclock, 0, interim);
					sem_post(&(smseg->clocksem));
					enqueue(waitingroom, proc);

				}
				
				else if(mem.pagetableinstance[proc - 1].frames[pageid].swaps == 0)
				{	
					if(lcount < 100000)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [read request]    -> [pid: %i] granted for read at [time: %is:%ins]", proc, smseg->smtime.secs, smseg->smtime.nans);				
					}
					
					strcpy(msg.message,"GRANTED READ REQ");
					msg.msgtype = proc;

					sem_wait(&(smseg->clocksem));
					clockinc(&smseg->smtime, 0, 10);	
					sem_post(&(smseg->clocksem));

					int frameloc = mem.pagetableinstance[proc - 1].frames[pageid].frame;	
					mem.frameinstance[frameloc].referbit = mem.frameinstance[frameloc].referbit | 0x80;

					msgsnd(tousr, &msg, sizeof(msg), IPC_NOWAIT);

				}
				
				else if(mem.pagetableinstance[proc - 1].frames[pageid].swaps == 1)
				{
					if(lcount < 100000)
					{
						lcount++;
						fprintf(outlog, "\n[oss]: [page fault]     -> address [0x%-5x] is not in a frame, at [time: %is:%ins]", pageid * 1000, smseg->smtime.secs, smseg->smtime.nans);
					}

					pagefaults++;
					
					smseg->procs[proc - 1].pid = proc - 1;
					smseg->procs[proc - 1].unblock = 0;
					smseg->procs[proc - 1].frame = pageid;
					
					int interim = (rand() & (140000000 - 10000000 + 1)) + 10000000;
					
					sem_wait(&(smseg->clocksem));
					clockinc(&smseg->procs[proc - 1].unblockclock, 0, interim);
					sem_post(&(smseg->clocksem));

					enqueue(waitingroom, proc);			
				}
				
			}

			if(((reqcount % 100) == 0) && reqcount != 0)
			{
				shifter();
				printer();
			}
		}

		int iter = 0;

		if(isempty(waitingroom) == 0)
		{
			int qsize = getsize(waitingroom);
			
			while(iter < qsize)
			{
				int proc = dequeue(waitingroom);

				if(((smseg->smtime.secs > smseg->procs[proc - 1].unblockclock.secs) || (smseg->smtime.secs == smseg->procs[proc - 1].unblockclock.secs && smseg->smtime.nans >= smseg->procs[proc - 1].unblockclock.nans)))
				{		
					int pageid = smseg->procs[proc - 1].frame;
					
					int frameloc = mem.pagetableinstance[proc - 1].frames[pageid].frame;	
					mem.frameinstance[frameloc].referbit = mem.frameinstance[frameloc].referbit | 0x80;
					
					if(smseg->procs[proc - 1].unblock == 0)
					{
						if(mem.pagetableinstance[proc - 1].frames[pageid].frame == -1 || mem.pagetableinstance[proc - 1].frames[pageid].swaps == 1)
						{			
							insertpage(pageid, proc);
						}

						msg.msgtype = proc;
						
						strcpy(msg.message,"GRANTED READ REQ");
						msgsnd(tousr, &msg, sizeof(msg), IPC_NOWAIT);
						
						if(lcount < 100000)
						{
							lcount++;
							fprintf(outlog, "\n[oss]: [read request]    -> [pid: %i] granted for address [0x%-5x] read at [time: %is:%ins]", proc, pageid * 1000, smseg->smtime.secs, smseg->smtime.nans);
						}

					}
					
					else if(smseg->procs[proc - 1].unblock == 1)
					{
						mem.frameinstance[frameloc].dirtybit = 0x1;
						
						if(mem.pagetableinstance[proc - 1].frames[pageid].frame == -1 || mem.pagetableinstance[proc - 1].frames[pageid].swaps == 1)
						{
							insertpage(pageid, proc);
						}

						msg.msgtype = proc;
						
						strcpy(msg.message,"GRANTED WRITE REQ");
						msgsnd(tousr, &msg, sizeof(msg), IPC_NOWAIT);
						
						if(lcount < 100000)
						{
							lcount++;
							fprintf(outlog, "\n[oss]: [write request]    -> [pid: %i] granted for address [0x%-5x] write at [time: %is:%ins]", proc, pageid * 1000, smseg->smtime.secs, smseg->smtime.nans);
						}
					}

				} else {
					enqueue(waitingroom, proc);
				}

				iter++;
			}
		}
	}

	while((tpid = wait(&status)) > 0);	

	float accesspersecond = ((float)(reqcount)/((float)(smseg->smtime.secs)+((float)smseg->smtime.nans/(float)(1000000000))));
	float faultsperaccess = ((float)(pagefaults)/(float)reqcount);
	float avgaccessspeeds = (((float)(smseg->smtime.secs)+((float)smseg->smtime.nans/(float)(1000000000)))/((float)reqcount));
	
	fprintf(outlog, "\n\n\tStatistics of Interest\n\t---------- -- --------\n");
	fprintf(outlog, "\tNumber of Memory Accesses Per Second: \t[%f]\n", accesspersecond);	
	fprintf(outlog, "\tNumber of Page Faults Per Memory Access: \t[%f]\n", faultsperaccess);
	fprintf(outlog, "\tAverage Memory Access Speed: \t[%f]\n\n", avgaccessspeeds);			
}
void insertpage(int id, int proc)
{
	int i;
	struct frame tempframe;
	tempframe.referbit = 0xff;
	int temploc = -1;
	
	for(i = 0; i < 256; i++)
	{
		if(mem.frameinstance[i].pid == -1)
		{
			temploc = i;
			break;
		}

		if(mem.frameinstance[i].referbit < tempframe.referbit)
		{
			tempframe.referbit = mem.frameinstance[i].referbit;
			temploc = i;	
		}
	}

	if(temploc == -1)
	{
		temploc = 0;
	}

	if(mem.frameinstance[temploc].pid > -1)
	{
		fprintf(outlog, "\n[oss]: [swap] -> [pid: %i] from frame %i to %i at [time: %is:%ins]\n", proc, temploc, mem.frameinstance[temploc].pid, smseg->smtime.secs, smseg->smtime.nans);
		
		if(mem.frameinstance[temploc].dirtybit == 0x1)
		{

			sem_wait(&(smseg->clocksem));
			clockinc(&smseg->smtime, 0, 100000);
			sem_post(&(smseg->clocksem));

		}
		mem.pagetableinstance[proc - 1].frames[id].swaps = 1;
	}

	mem.frameinstance[temploc].referbit = 0x0;
	mem.frameinstance[temploc].dirtybit = 0x0;
	mem.frameinstance[temploc].pid = - 1;

	mem.frameinstance[temploc].pid = proc;
	mem.frameinstance[temploc].referbit = mem.frameinstance[temploc].referbit | 0x80;

	mem.pagetableinstance[proc - 1].frames[id].swaps = 0;
	mem.pagetableinstance[proc - 1].frames[id].frame = temploc;
}
void printer()
{

	if(lcount < 100000)
	{
		fprintf(outlog, "\n\nMemory Layout at [%is:%ins]\n", smseg->smtime.secs, smseg->smtime.nans);
		fprintf(outlog, "\t     Occupied\t\tAddress\t\tRefByte\t\tDirtyBit\n");
		lcount = lcount + 2;
	}

	int i;
	for(i = 0; i < memsize / pagesize; i++)
	{
		if(lcount < 100000)
		{	
			fprintf(outlog, "Frame %i:\t", i + 1);
			
			if(mem.frameinstance[i].pid != -1)
			{
				fprintf(outlog, "yes\t");
			} else {
				fprintf(outlog, "no\t");
			}

			fprintf(outlog, "\t[0x%-5x]\t%c%c%c%c%c%c%c%c\t%x", i * 1000, BYTE_TO_BINARY(mem.frameinstance[i].referbit), mem.frameinstance[i].dirtybit);	
			lcount++;
		}

		fprintf(outlog, "\n");	
			
	}

	if(lcount < 100000)
	{
		lcount++;
		fprintf(outlog, "\n");
	}

}
void shifter()
{
	int i;
	for(i = 0; i < memsize; i++)
	{
		mem.frameinstance[i].referbit = mem.frameinstance[i].referbit >> 1;
	}
}
int findaseat()
{
	int searcher;
	for(searcher = 0; searcher < PCAP; searcher++)
	{
		if(plist[searcher] == 0)
		{
			plist[searcher] = 1;
			return searcher;
		}
	}
	return -1;
}
void moppingup()
{
	//shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);
	fclose(outlog);
}
void overlay(int id, int scheme)
{
	char proc[20]; 
	char schm[10];
					
	sprintf(proc, "%i", id);
	sprintf(schm, "%i", scheme);	
				
	char* fargs[] = {"./usr", proc, schm, NULL};
	execv(fargs[0], fargs);

	/* oss will not reach here unerred */	
	perror("\noss: error: exec failure");
	exit(EXIT_FAILURE);	
}
void clockinc(simclock* khronos, int sec, int nan)
{
	khronos->secs = khronos->secs + sec;
	khronos->nans = khronos->nans + nan;
	while(khronos->nans >= 1000000000)
	{
		khronos->nans -= 1000000000;
		(khronos->secs)++;
	}
}
void arrinit()
{
	float weighted;
	int i;
	for(i = 1; i <= 32; i++)
	{
		weighted = 1 / (float)i;
		smseg->weightarr[i - 1] = weighted;
	}
}
void tabinit()
{
	int m;
	int n;
	for(n = 0; n < 256; n++)
	{
		mem.frameinstance[n].referbit = 0x0;
		mem.frameinstance[n].dirtybit = 0x0;
		mem.frameinstance[n].pid = -1;
	}

	for(n = 0; n < PCAP; n++)
	{
		for(m = 0; m < procsize; m++)
		{
			mem.pagetableinstance[n].frames[m].frame = -1;
			mem.pagetableinstance[n].frames[m].swaps = -1;
		}
	}
}
void msginit()
{
	key_t msgkey = ftok("msg1", 925);
	if(msgkey == -1)
	{
		perror("\noss: error: ftok message failed");
		exit(EXIT_FAILURE);
	}

	tousr = msgget(msgkey, 0600 | IPC_CREAT);
	if(tousr == -1)
	{
		perror("\noss: error: failed to create message");
		exit(EXIT_FAILURE);
	}

	msgkey = ftok("msg2", 825);
	if(msgkey == -1)
	{
		perror("\noss: error: ftok message failed");
		exit(EXIT_FAILURE);
	}

	tooss = msgget(msgkey, 0600 | IPC_CREAT);
	if(tooss == -1)
	{
		perror("\noss: error: failed to create message");
		exit(EXIT_FAILURE);
	}
}
void shminit()
{
	key_t smkey = ftok("shmfile", 'a');
	if(smkey == -1)
	{
		perror("\noss: error: ftok shared memory failed");
		exit(EXIT_FAILURE);
	}

	sipcid = shmget(smkey, sizeof(shmem), 0600 | IPC_CREAT);
	if(sipcid == -1)
	{
		perror("\noss: error: failed to create shared memory");
		exit(EXIT_FAILURE);
	}

	smseg = (shmem*)shmat(sipcid,(void*)0, 0);

	if(smseg == (void*)-1)
	{
		perror("\noss: error: failed to attach shared memory");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&(smseg->clocksem), 1, 1) == -1)
	{
		perror("\nmaster: error: sem_init failed");
		exit(EXIT_FAILURE);
	}
}
void killtime(int sig, siginfo_t *sainfo, void *ptr)
{
	char msgtime[] = "\n[oss]: exit: simulation terminated after 10s run.\n\nrefer to log.txt for results.\n\n";
	int msglentime = sizeof(msgtime);

	write(STDERR_FILENO, msgtime, msglentime);

	float accesspersecond = ((float)(reqcount)/((float)(smseg->smtime.secs)+((float)smseg->smtime.nans/(float)(1000000000))));
	float faultsperaccess = ((float)(pagefaults)/(float)reqcount);
	float avgaccessspeeds = (((float)(smseg->smtime.secs)+((float)smseg->smtime.nans/(float)(1000000000)))/((float)reqcount));
	
	fprintf(outlog, "\n\n\tStatistics of Interest\n\t---------- -- --------\n");
	fprintf(outlog, "\tNumber of Memory Accesses Per Second: \t[%f]\n", accesspersecond);	
	fprintf(outlog, "\tNumber of Page Faults Per Memory Access: \t[%f]\n", faultsperaccess);
	fprintf(outlog, "\tAverage Memory Access Speed: \t[%f]\n\n", avgaccessspeeds);

	// int i;
	// for(i = 0; i < PCAP; i++)
	// {
	// 	if(pids[i] != 0)
	// 	{
	// 		if(kill(pids[i], SIGTERM) == -1)
	// 		{
	// 			perror("\noss: error: ");			
	// 		}
	// 	}
	// }

	fclose(outlog);
	//shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);

	kill(getpid(), SIGTERM);

	exit(EXIT_SUCCESS);			
}
void killctrl(int sig, siginfo_t *sainfo, void *ptr)
{
	char msgctrl[] = "\n[oss]: exit: received ctrl-c interrupt signal\n\nrefer to log.txt for results.\n\n";
	int msglenctrl = sizeof(msgctrl);

	write(STDERR_FILENO, msgctrl, msglenctrl);

	float accesspersecond = ((float)(reqcount)/((float)(smseg->smtime.secs)+((float)smseg->smtime.nans/(float)(1000000000))));
	float faultsperaccess = ((float)(pagefaults)/(float)reqcount);
	float avgaccessspeeds = (((float)(smseg->smtime.secs)+((float)smseg->smtime.nans/(float)(1000000000)))/((float)reqcount));
	
	fprintf(outlog, "\n\n\tStatistics of Interest\n\t---------- -- --------\n");
	fprintf(outlog, "\tNumber of Memory Accesses Per Second: \t[%f]\n", accesspersecond);	
	fprintf(outlog, "\tNumber of Page Faults Per Memory Access: \t[%f]\n", faultsperaccess);
	fprintf(outlog, "\tAverage Memory Access Speed: \t[%f]\n\n", avgaccessspeeds);

	// int i;
	// for(i = 0; i < PCAP; i++)
	// {
	// 	if(pids[i] != 0)
	// 	{
	// 		if(kill(pids[i], SIGTERM) == -1)
	// 		{
	// 			perror("\noss: error: ");
	// 		}
	// 	}
	// }

	fclose(outlog);
	//shmdt(smseg);
	shmctl(sipcid, IPC_RMID, NULL);
	msgctl(tooss, IPC_RMID, NULL);
	msgctl(tousr, IPC_RMID, NULL);

	kill(getpid(), SIGTERM);

	exit(EXIT_SUCCESS);			
}
static int satimer()
{
	struct itimerval t;
	t.it_value.tv_sec = 2;
	t.it_value.tv_usec = 0;
	t.it_interval.tv_sec = 0;
	t.it_interval.tv_usec = 0;
	
	return(setitimer(ITIMER_REAL, &t, NULL));
}
void optset(int argc, char *argv[])
{
	int choice;
	while((choice = getopt(argc, argv, "hm:")) != -1)
	{
		switch(choice)
		{
			case 'h':
				helpme();
				exit(EXIT_SUCCESS);
			case 'm':
				scheme = atoi(optarg);
				if(scheme > 1 || scheme < 0)
				{
					printf("\noss: error: determinant of memory request scheme must be 0 or 1\n");
					exit(EXIT_FAILURE);
				}
				break;
			case '?':
				fprintf(stderr, "\noss: error: invalid argument\n");
				exit(EXIT_FAILURE);				
		}
	}
}
void helpme()
{
	printf("\n|HELP|MENU|\n\n");
    printf("\t-h : display help menu\n");
	printf("\t-m x : specify memory request scheme. either 0 or 1\n");
}
