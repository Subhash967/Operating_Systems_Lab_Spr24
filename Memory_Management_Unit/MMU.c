#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <limits.h>
#include <math.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>


#define PSEND_TYPE 10
#define MMUTOPRO 20
#define INVALID_PAGE_REF -2
#define PAGEFAULT -1
#define PROCESS_OVER -9
#define PAGEFAULT_HANDLED 5
#define TERMINATED 10

int count = 0;
int *pffreq;
int *invalidfreq;
FILE *resultf;
int i;

typedef struct {
	int frameno;
	int isvalid;
	int count;
} ptbentry;

typedef struct {
	pid_t pid;
	int m;
	int f_cnt;
	int f_allo;
} pcb;

typedef struct
{
	int current;
	int flist[];
} freelist;

struct msgbuf
{
	long mtype;
	int id;
	int pageno;
};

struct mmutopbuf
{
	long mtype;
	int frameno;
};

struct mmutosch
{
	long mtype;
	char mbuf[1];
};



int SM1, SM2,SM3;
int MQ2, MQ3;



int m,k;

int readRequest(int* id)
{
	struct msgbuf mbuf;
	int length;
	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(struct msgbuf) - sizeof(long);
	memset(&mbuf, 0, sizeof(mbuf));

	int rst = msgrcv(MQ3, &mbuf, length, PSEND_TYPE, 0);
	if (rst == -1)
	{
		if(errno == EINTR)
			return -1;
		perror("msgrcv");
		exit(EXIT_FAILURE);
	}
	*id = mbuf.id;
	return mbuf.pageno;
}

void sendreply(int id, int frameno)
{
	struct mmutopbuf mbuf;
	mbuf.mtype = id + MMUTOPRO;
	mbuf.frameno = frameno;
	int length = sizeof(struct msgbuf) - sizeof(long);
	int rst = msgsnd(MQ3, &mbuf, length, 0);
	if (rst == -1)
	{
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
}

void notifySched(int type)
{
	struct mmutosch mbuf;
	mbuf.mtype = type;
	int length = sizeof(struct msgbuf) - sizeof(long);
	int rst = msgsnd(MQ2, &mbuf, length, 0);
	if (rst == -1)
	{
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
	//printf("Sent signal to sched = %d\n",type);

}
pcb *pcbptr;
ptbentry *ptbptr;
freelist *freeptr;

int handlePageFault(int id, int pageno)
{
	int i;
	if (freeptr->current == -1 || pcbptr[i].f_cnt <= pcbptr[i].f_allo)
	{
		int min = INT_MAX, mini = -1;
		int victim = 0;
		for (i = 0; i < pcbptr[i].m; i++)
		{
			if (ptbptr[id * m + i].isvalid == 1)
			{
				if (ptbptr[id * m + i].count < min)
				{
					min = ptbptr[id * m + i].count;
					victim = ptbptr[id * m + i].frameno;
					mini = i;
				}
			}
		}
		ptbptr[id * m + mini].isvalid = 0;
		return victim;
	}
	else
	{
		int fn = freeptr->flist[freeptr->current];
		freeptr->current += 1;
		return fn;
	}
}

void freepages(int i)
{

	int k = 0;
	for (k = 0; k < pcbptr[i].m; i++)
	{
		if (ptbptr[i * m + k].isvalid == 1)
		{
			freeptr->flist[freeptr->current - 1] = ptbptr[i * m + k].frameno;
			freeptr->current -= 1;
		}
	}
	//TODO: Think shuld change the allocation
}

int serviceMRequest()
{
	pcbptr = (pcb*)(shmat(SM3, NULL, 0));
	
	ptbptr = (ptbentry*)(shmat(SM1, NULL, 0));
	
	freeptr = (freelist*)(shmat(SM2, NULL, 0));


	int id = -1, pageno;
	pageno = readRequest(&id);
	if(pageno == -1 && id == -1)
	{
		return 0;
	}
	int i = id;
	if (pageno == PROCESS_OVER)
	{
		freepages(id);
		notifySched(TERMINATED);
		
		return 0;
	}
	count ++;
	printf("MMU: Page reference : (%d,%d,%d)\n",count,id,pageno);
	fprintf(resultf,"Page reference : (%d,%d,%d)\n",count,id,pageno);
	if (pcbptr[id].m < pageno || pageno < 0)
	{
		printf("MMU: Invalid Page Reference : (%d %d)\n",id,pageno);
		fprintf(resultf,"Invalid Page Reference : (%d %d)\n",id,pageno);
		sendreply(id, INVALID_PAGE_REF);
		printf("MMU: Process %d: TRYING TO ACCESS INVALID PAGE REFERENCE %d\n", id, pageno);
		invalidfreq[id] += 1;
		freepages(id);
		notifySched(TERMINATED);
		//Invalid reference

	}
	else
	{
		if (ptbptr[i * m + pageno].isvalid == 0)
		{
			//PAGE FAULT
			printf("MMU: Page Fault : (%d,%d)\n",id,pageno);
			fprintf(resultf,"Page Fault : (%d, %d)\n",id,pageno);
			pffreq[id] += 1;
			sendreply(id, -1);
			int fno = handlePageFault(id, pageno);
			ptbptr[i * m + pageno].isvalid = 1;
			ptbptr[i * m + pageno].count = count;
			ptbptr[i * m + pageno].frameno = fno;
			
			notifySched(PAGEFAULT_HANDLED);
		}
		else
		{
			printf("MMU: Page Hit\n");
			sendreply(id, ptbptr[i * m + pageno].frameno);
			ptbptr[i * m + pageno].count = count;
			//FRAME FOUND
		}
	}
	if(shmdt(pcbptr) == -1)
	{
		perror("pcbptr-shmdt");
		exit(EXIT_FAILURE);
	}
	if(shmdt(ptbptr) == -1)
	{
		perror("ptbptr-shmdt");
		exit(EXIT_FAILURE);
	}
	if(shmdt(freeptr) == -1)
	{
		perror("freel-shmdt");
		exit(EXIT_FAILURE);
	}
}
int flag = 1;
void handletgerm(int sig)
{
	//printf("I am a god kill me\n");
	flag = 0;
}

int main(int argc, char const *argv[])
{
	if (argc < 4)
	{
		printf("Invalid number of arguments\n");
		exit(EXIT_FAILURE);
	}
	MQ2 = atoi(argv[1]);
	MQ3 = atoi(argv[2]);
	SM1 = atoi(argv[3]);
	SM2 = atoi(argv[4]);
	SM3 = atoi(argv[5]);
	m = atoi(argv[6]);
	k = atoi(argv[7]);
	signal(SIGUSR2, handletgerm);
	pffreq = (int *)malloc(k*sizeof(int));
	invalidfreq = (int *)malloc(k*sizeof(int));
	for(i=0;i<k;i++)
	{
		pffreq[i] = 0;
	} 
	resultf = fopen("Result.txt","w");
	while(flag)
	{
		serviceMRequest();
	}
	printf("Page fault Count and invalid page references for each Process:\n");	
	fprintf(resultf,"Page fault Count and invalid page references for each Process:\n");
	printf("Process Id\tno of page faults\tInvalid page references\n");
	fprintf(resultf,"Process Id\tno of page faults\tInvalid page references\n");
	for(i = 0;i<k;i++)
	{
		printf("%d\t\t%d\t\t\t%d\n",i,pffreq[i],invalidfreq[i]);
		fprintf(resultf,"%d\t\t%d\t\t\t%d\n",i,pffreq[i],invalidfreq[i]);
	}
	// sleep(20);
	fclose(resultf);
	// sleep(60);
	return 0;
}