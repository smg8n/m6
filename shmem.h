#ifndef SHMEM_H
#define SHMEM_H

#include <semaphore.h>

#define PCAP 18

typedef struct
{
    unsigned int secs;
    unsigned int nans;
} simclock;

typedef struct
{
    int pid;
    simclock unblockclock;
    int unblock;
    int frame;

} info;

typedef struct
{
    sem_t clocksem;
    info procs[18];
    simclock smtime;
    float weightarr[32];
} shmem;

#endif /* SHMEM_H */
