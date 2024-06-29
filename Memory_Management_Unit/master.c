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


typedef struct {
	int frameno;
	int isvalid;
	int count;
}ptbentry;

typedef struct {
	pid_t pid;
	int m;
	int f_cnt;
	int f_allo;
}pcb;

typedef struct 
{
	int current;
	int flist[];
}freelist;

int k,m,f;
int flag = 0;

key_t MQ1_key, MQ2_key, MQ3_key;
key_t SM1_key,SM2_key,SM3_key;


int MQ1,MQ2,MQ3;
int SM1,SM2,SM3;


void userexit(int status);


void GenerateProcesses()
{
	pcb *ptr = (pcb*)(shmat(SM3, NULL, 0));
	

	int i,j;
	for(i=0;i<k;i++)
	{
		int rlen = rand()%(8*ptr[i].m) + 2*ptr[i].m + 1;
		char rstring[m*20*40];
		printf("reference string length of process %d = %d\n",i,rlen);
		int l = 0;
		for(j=0;j<rlen;j++)
		{
			int r;
			r = rand()%ptr[i].m;
			float p = (rand()%100)/100.0;
			if(p < 0.01)
			{
				// r = rand()%(1000*m) + ptr[i].m;
				r = rand()%INT_MAX;
			}
			l += sprintf(rstring+l,"%d,",r);
		}
		printf("Ref string of process %d = %s\n",i,rstring);
		if(fork() == 0)
		{
			char buf1[20],buf2[20],buf3[20];
			sprintf(buf1,"%d",i);
			sprintf(buf2,"%d",MQ1_key);
			sprintf(buf3,"%d",MQ3_key);
			execlp("./process","./process",buf1,buf2,buf3,rstring,(char *)(NULL));
			exit(0);

		}
		//TODO: fork proecess here
		usleep(250*1000);	
	}

}


void GenerateKPTs()
{
	int i;
	int x=rand()%100000;
	SM1_key = ftok("master.c",x);
	if(SM1_key == -1)
	{	
		perror("SM1_key");
		userexit(EXIT_FAILURE);
	}
	SM1 = shmget(SM1_key, m*sizeof(ptbentry)*k, 0666 | IPC_CREAT | IPC_EXCL);
	if(SM1 == -1)
	{	
		perror("pcb-shmget");
		userexit(EXIT_FAILURE);
	}

	ptbentry *ptr = (ptbentry*)(shmat(SM1, NULL, 0));
	if(*(int *)ptr == -1)
	{
		perror("pcb-shmat");
		userexit(EXIT_FAILURE);
	}

	for(i=0;i<k*m;i++)
	{
		ptr[i].frameno = -1;
		ptr[i].isvalid = 0;
	}

	if(shmdt(ptr) == -1)
	{
		perror("pcb-shmdt");
		userexit(EXIT_FAILURE);
	}
}




void GenerateFreeFramesList()
{
	int i;
	int x=rand()%100000;
	SM2_key = ftok("master.c",x);
	if(SM2_key == -1)
	{	
		perror("SM2_key");
		userexit(EXIT_FAILURE);
	}
	SM2 = shmget(SM2_key, sizeof(freelist)+f*sizeof(int), 0666 | IPC_CREAT | IPC_EXCL);
	if(SM2 == -1)
	{	
		perror("free-shmget");
		userexit(EXIT_FAILURE);
	}

	freelist *ptr = (freelist*)(shmat(SM2, NULL, 0));
	if(*((int *)ptr) == -1)
	{
		perror("freel-shmat");
		userexit(EXIT_FAILURE);
	}
	for(i=0;i<f;i++)
	{
		ptr->flist[i] = i;
	}
	ptr->current = 0;

	if(shmdt(ptr) == -1)
	{
		perror("freel-shmdt");
		userexit(EXIT_FAILURE);
	}
}



void GeneratePCBs()
{
	int i;
	int x=rand()%100000;
	SM3_key = ftok("master.c",x);
	if(SM3_key == -1)
	{	
		perror("pcbkey");
		userexit(EXIT_FAILURE);
	}
	SM3 = shmget(SM3_key, sizeof(pcb)*k, 0666 | IPC_CREAT | IPC_EXCL );
	if(SM3 == -1)
	{	
		perror("pcb-shmget");
		userexit(EXIT_FAILURE);
	}

	pcb *ptr = (pcb*)(shmat(SM3, NULL, 0));
	if(*(int *)ptr == -1)
	{
		perror("pcb-shmat");
		userexit(EXIT_FAILURE);
	}

	int totpages = 0;
	for(i=0;i<k;i++)
	{
		ptr[i].pid = i;
		ptr[i].m = rand()%m + 1;
		ptr[i].f_allo = 0;
		totpages +=  ptr[i].m;
	}
	int allo_frame = 0;
	printf("tot no of pages required combined= %d, k = %d, total no of frames available=  %d\n",totpages,k,f);
	printf("So,for each process no of frames allocated is as follows\n");
	int max = 0,maxi = 0;
	for(i=0;i<k;i++)
	{
		ptr[i].pid = -1;
		int allo = (int)round(ptr[i].m*(f-k)/(float)totpages) + 1;
		if(ptr[i].m > max)
		{
			max = ptr[i].m;
			maxi = i;
		}
		allo_frame = allo_frame + allo;
		//printf("%d\n",allo);
		ptr[i].f_cnt = allo;
		
	}
	ptr[maxi].f_cnt += f - allo_frame; 

	for(i=0;i<k;i++)
	{
		printf("Process = %d m = %d f_cnt = %d\n",i,ptr[i].m,ptr[i].f_cnt);
	}

	if(shmdt(ptr) == -1)
	{
		perror("freel-shmdt");
		userexit(EXIT_FAILURE);
	}

}




void GenerateMQs()
{
	int x=rand()%100000;
	MQ1_key = ftok("master.c",x);
	if(MQ1_key == -1)
	{	
		perror("MQ1_key");
		userexit(EXIT_FAILURE);
	}
	MQ1 = msgget(MQ1_key, 0666 | IPC_CREAT| IPC_EXCL);
	if(MQ1 == -1)
	{
		perror("ready-msgget");
		userexit(EXIT_FAILURE);
	}

	MQ2_key = ftok("master.c",x+1);
	if(MQ2_key == -1)
	{	
		perror("MQ2_key");
		userexit(EXIT_FAILURE);
	}
	MQ2 = msgget(MQ2_key, 0666 | IPC_CREAT| IPC_EXCL );
	if(MQ2 == -1)
	{
		perror("msgq2-msgget");
		userexit(EXIT_FAILURE);
	} 

	MQ3_key = ftok("master.c",x+2);
	if(MQ3_key == -1)
	{	
		perror("MQ3_key");
		userexit(EXIT_FAILURE);
	}
	MQ3 = msgget(MQ3_key, 0666 | IPC_CREAT| IPC_EXCL);
	if(MQ3 == -1)
	{
		perror("msgq3-msgget");
		userexit(EXIT_FAILURE);
	} 
}


void RemoveResources()
{
	if(shmctl(SM1,IPC_RMID, NULL) == -1)
	{
		perror("shmctl-ptb");
	}
	if(shmctl(SM2,IPC_RMID, NULL) == -1)
	{
		perror("shmctl-freel");
	}
	if(shmctl(SM3,IPC_RMID, NULL) == -1)
	{
		perror("shmctl-pcb");
	}
	if(msgctl(MQ1, IPC_RMID, NULL) == -1)
	{
		perror("msgctl-ready");
	}
	if(msgctl(MQ2, IPC_RMID, NULL) == -1)
	{
		perror("msgctl-msgq2");
	}
	if(msgctl(MQ3, IPC_RMID, NULL) == -1)
	{
		perror("msgctl-msgq3");
	}
}

void userexit(int status)
{
	RemoveResources();
	exit(status);
}



int master_pid,sched_pid,mmu_pid;

void END(int sig)
{
	//printf("Mater: gi=o the signal\n");
	sleep(1);
	kill(sched_pid, SIGTERM);
	kill(mmu_pid, SIGUSR2);
	sleep(2);
	flag = 1;

}
int main(int argc, char const *argv[])
{
	srand(time(NULL));
	signal(SIGUSR1, END);
	signal(SIGINT, userexit);
	
	printf("Total number of processes(k):");
    scanf("%d",&k);

    printf("Maximum number of pages required per process (Virtual address space)(m):");
    scanf("%d",&m);

    printf("Total number of frames(Physical address space)(f):");
    scanf("%d",&f);

	
	master_pid = getpid();
	if(k <= 0 || m <= 0 || f <=0 || f < k)
	{
		printf("Invalid input\n");
		userexit(EXIT_FAILURE);
	}

	GenerateFreeFramesList();
	GenerateKPTs();
	GeneratePCBs();
	GenerateMQs();

	if((sched_pid = fork()) == 0)
	{
		char buf1[20],buf2[20],buf3[20],buf4[20];
		sprintf(buf1,"%d",MQ1_key);
		sprintf(buf2,"%d",MQ2_key);
		sprintf(buf3,"%d",k);
		sprintf(buf4,"%d",master_pid);
		execlp("./sched","./sched",buf1,buf2,buf3,buf4,(char *)(NULL));
		exit(0);
	}


	if((mmu_pid = fork()) == 0)
	{
		char buf1[20],buf2[20],buf3[20],buf4[20],buf5[20],buf6[20],buf7[20];
		sprintf(buf1,"%d",MQ2);
		sprintf(buf2,"%d",MQ3);
		sprintf(buf3,"%d",SM1);
		sprintf(buf4,"%d",SM2);
		sprintf(buf5,"%d",SM3);
		sprintf(buf6,"%d",m);
		sprintf(buf7,"%d",k);
		execlp("./MMU","./MMU",buf1,buf2,buf3,buf4,buf5,buf6,buf7,(char *)(NULL));
		// execlp("xterm", "xterm", "-T", "Memory Management Unit", "-e","./MMU",buf1,buf2,buf3,buf4,buf5,buf6,buf7,(char *)(NULL));

		exit(0);
	}
	printf("Creating %d Processess\n",k);
	GenerateProcesses();
	if(flag == 0)
		pause();

	// sleep(60);
	RemoveResources();

	return 0;
}