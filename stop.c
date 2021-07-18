#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(int argc,char * argv[])
{
    int stopid;
    int *stop;
    key_t key_sm;//key
    
    if((key_sm=ftok("/.",'S'))==-1)//generate key
        {perror("error ftok\n");exit(1);}
    if((stopid=shmget(key_sm,1024,0666|IPC_CREAT))==-1){ perror("stop,fail shmget");exit(1);}//connect to shared memory
    if((stop=(int*) shmat(stopid,(void*)0,0))==(void*)-1) perror("stop,fail shmat");//attaches
    *stop=-1;
    shmdt(stop);
    
    return 0;
}