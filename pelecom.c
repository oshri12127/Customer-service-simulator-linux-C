
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include "pelecom.h"
#include "random.h"
#include "stopwatch.h"

void entry();
void sorter();
void newCustomers();
void upgradeCustomers();
void repairCustomers();

stopwatch clock;
int arrival_queue;
int new_queue,upgrade_queue,repair_queue;
int *total_customers;
long total_time_of_service_center=0;
int main(int argc,char * argv[])
{
    key_t key_ms;//key
    int i;
    
    if((key_ms=ftok("/.",'A'))==-1)
        perror("error ftok\n");
    if((arrival_queue=msgget(key_ms, 0666 | IPC_CREAT))==-1) {perror("arrival_queue,fail msgget \n");exit(1);}
    
    if((key_ms=ftok("/.",'B'))==-1)
        perror("error ftok\n");
    if((new_queue=msgget(key_ms, 0666 | IPC_CREAT))==-1) {perror("new_queue,fail msgget \n");exit(1);}

    if((key_ms=ftok("/.",'C'))==-1)
        perror("error ftok\n");
    if((upgrade_queue=msgget(key_ms, 0666 | IPC_CREAT))==-1){perror("upgrade_queue,fail msgget \n");exit(1);}

    if((key_ms=ftok("/.",'D'))==-1)
        perror("error ftok\n");
    if((repair_queue=msgget(key_ms, 0666 | IPC_CREAT))==-1){perror("repair_queue,fail msgget \n");exit(1);}
    
    if((total_customers=mmap(NULL,sizeof *total_customers,PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,-1,0))==MAP_FAILED)//shared memory
        {perror("error mmap\n");exit(1);}

    swstart(&clock);//start clock
    *total_customers=0;
    for(i=0;i<5;i++)//crate 5 forks(child)
    {
        if(fork()==0)
        {
            if(i==0)entry();
            if(i==1)sorter();
            if(i==2)newCustomers();
            if(i==3)upgradeCustomers();
            if(i==4)repairCustomers();
        }
    }
    for(i=0;i<5;i++)//wait all child finish
    {
        wait(NULL);
    }
    total_time_of_service_center=swlap(&clock);
    printf("\nTotal customers:%d\nTotal time of service center:%ld\n",*total_customers,total_time_of_service_center);
    if(munmap(total_customers,sizeof *total_customers)==-1)
        perror("error munmap");
    return 0;
}
int createType()
{
    double type;
    type=urand(0,100);//random 0 to 100
    if(type<POP_NEW) return TYPE_NEW;
    else if(type>45) return TYPE_REPAIR;
    else return TYPE_UPGRADE;
}
int createProcessTime(int type)
{
    if(type==TYPE_NEW){ /*printf("newPT ,");*/return pnrand(AVRG_NEW,SPRD_NEW,MIN_NEW);}
    else if(type==TYPE_UPGRADE) {/*printf("upgradePT ,");*/return pnrand(AVRG_UPGRADE,SPRD_UPGRADE,MIN_UPGRADE);}
    else {/*printf("repairPT ,");*/return pnrand(AVRG_REPAIR,SPRD_REPAIR,MIN_REPAIR);}
}
void entry()
{
    long counterCustomers=0;
    customer newCustomer;
    int stopid;//id 
    int *stop_S_M;
    key_t key_sm;//key
    
    if((key_sm=ftok("/.",'S'))==-1)//generate key
        perror("error ftok\n");
    if((stopid=shmget(key_sm,1024,0666|IPC_CREAT))==-1) perror("stop,fail shmget");//crate shared memory
    if((stop_S_M=(int*) shmat(stopid,(void*)0,0))==(void*)-1) perror("stop,fail shmat");//attaches

    initrand();
    while(1)
    {
        if(*stop_S_M==-1)//if program stop change to -1 so stop
        {
            newCustomer.c_id=999;
            newCustomer.c_data.type=TYPE_QUIT;
            if((msgsnd(arrival_queue,&newCustomer,sizeof(newCustomer)-sizeof(long),0))==-1) perror("arrival_queue,fail msgsnd \n");//send message
            break;
        }
        usleep(pnrand(AVRG_ARRIVE,SPRD_ARRIVE,MIN_ARRIVE));
        counterCustomers++;
        newCustomer.c_id=counterCustomers;
        newCustomer.c_data.enter_time=swlap(&clock);
        newCustomer.c_data.type=createType();
        newCustomer.c_data.process_time=createProcessTime(newCustomer.c_data.type);
        if((msgsnd(arrival_queue,&newCustomer,sizeof(newCustomer)-sizeof(long),0))==-1) perror("arrival_queue,fail msgsnd \n");//send message
    }
    if(shmctl(stopid, IPC_RMID,NULL)==-1) perror("stop,fail to delete msgctl");//delete shared memory
    exit(0);
}
void sorter()
{
    long elapse_time_all=0;
    customer sortCustomer;
    printf("Sorter running\n");
    while(1){
        if((msgrcv(arrival_queue,&sortCustomer,sizeof(sortCustomer)-sizeof(long),0,0))==-1) perror("arrival_queue,fail msgrcv \n");//read message
        if(sortCustomer.c_data.type==TYPE_QUIT)//if type quit send message all queue and stop
        {
            if((msgsnd(new_queue,&sortCustomer,sizeof(sortCustomer)-sizeof(long),0))==-1) perror("new_queue,fail msgsnd \n");
            if((msgsnd(upgrade_queue,&sortCustomer,sizeof(sortCustomer)-sizeof(long),0))==-1) perror("upgrade_queue,fail msgsnd \n");
            if((msgsnd(repair_queue,&sortCustomer,sizeof(sortCustomer)-sizeof(long),0))==-1) perror("repair_queue,fail msgsnd \n");
            break;
        }
        else
        {
            if(sortCustomer.c_data.type==TYPE_NEW)
            {
                if((msgsnd(new_queue,&sortCustomer,sizeof(sortCustomer)-sizeof(long),0))==-1) perror("new_queue,fail msgsnd \n");//send message
            }
            else if(sortCustomer.c_data.type==TYPE_UPGRADE)
            {
                if((msgsnd(upgrade_queue,&sortCustomer,sizeof(sortCustomer)-sizeof(long),0))==-1) perror("upgrade_queue,fail msgsnd \n");//send message
            }
            else
            {
                if((msgsnd(repair_queue,&sortCustomer,sizeof(sortCustomer)-sizeof(long),0))==-1) perror("repair_queue,fail msgsnd \n");//send message
            }
        }
        usleep(pnrand(AVRG_SORT,SPRD_SORT,MIN_SORT));
        //printf("type: %d , id: %ld \n",sortCustomer.c_data.type,sortCustomer.c_id);
    }
    elapse_time_all=swlap(&clock);
    if(msgctl(arrival_queue, IPC_RMID,NULL)==-1) perror("arrival_queue,fail to delete msgctl");//delete
    printf("\nSorter quitting, elapse:%ld\n",elapse_time_all);
    exit(0);
}
void newCustomers()
{ 
    int counterServiceCustomers=0;//count of customers in new service.
    int wait_time=0;//time of all customers waited in line
    long elapse_time_all=0;
    int work_time=0;
    int stay_in_service_center=0;//time of all customers stayed in service center
    customer serviceCustomer;
    printf("Clerk for new customers is starting\n");
    while(1){
        if((msgrcv(new_queue,&serviceCustomer,sizeof(serviceCustomer)-sizeof(long),0,0))==-1) perror("new_queue,fail msgrcv \n");//read
        if(serviceCustomer.c_data.type==TYPE_QUIT)//if type quit stop
        {
            break;
        }
        counterServiceCustomers++;
        serviceCustomer.c_data.start_time=swlap(&clock);
        usleep(serviceCustomer.c_data.process_time);
        serviceCustomer.c_data.exit_time=swlap(&clock);
        int elapse=serviceCustomer.c_data.exit_time-serviceCustomer.c_data.enter_time;
        serviceCustomer.c_data.elapse_time=elapse;
        wait_time+=serviceCustomer.c_data.start_time-serviceCustomer.c_data.enter_time;
        work_time+=serviceCustomer.c_data.process_time/1000;
        stay_in_service_center+=serviceCustomer.c_data.elapse_time;
        printf("id:%ld,type:new,arrived: %ld,started: %ld,processed: %d,exited: %ld,elapse: %ld \n",serviceCustomer.c_id,serviceCustomer.c_data.enter_time,serviceCustomer.c_data.start_time,serviceCustomer.c_data.process_time/1000,serviceCustomer.c_data.exit_time,serviceCustomer.c_data.elapse_time);
    }
    elapse_time_all=swlap(&clock);
    if(msgctl(new_queue, IPC_RMID,NULL)==-1) perror("new_queue,fail to delete msgctl");//delete
    printf("\nClerk for new customers is quitting\n");
    printf("Clerk for new customers: processed %d customers, elapse:%ld\n  work:%d wait:%d stay:%d\n",counterServiceCustomers,elapse_time_all,work_time,wait_time,stay_in_service_center);
    printf("  per customer: work:%d wait:%d stay:%d\n",work_time/counterServiceCustomers,wait_time/counterServiceCustomers,stay_in_service_center/counterServiceCustomers);
    *total_customers+=counterServiceCustomers;//update to shared memory
    exit(0);
}
void upgradeCustomers()
{
    int counterServiceCustomers=0;//count of customers in upgrade service.
    int wait_time=0;//time of all customers waited in line
    long elapse_time_all=0;
    int work_time=0;
    int stay_in_service_center=0;//time of all customers stayed in service center
    customer serviceCustomer;
    printf("Clerk for upgrade customers is starting\n");
    while(1){
        if((msgrcv(upgrade_queue,&serviceCustomer,sizeof(serviceCustomer)-sizeof(long),0,0))==-1) perror("upgrade_queue,fail msgrcv \n");
        if(serviceCustomer.c_data.type==TYPE_QUIT)//if type quit stop
        {
            break;
        }
        counterServiceCustomers++;
        serviceCustomer.c_data.start_time=swlap(&clock);
        usleep(serviceCustomer.c_data.process_time);
        serviceCustomer.c_data.exit_time=swlap(&clock);
        int elapse=serviceCustomer.c_data.exit_time-serviceCustomer.c_data.enter_time;
        serviceCustomer.c_data.elapse_time=elapse;
        wait_time+=serviceCustomer.c_data.start_time-serviceCustomer.c_data.enter_time;
        work_time+=serviceCustomer.c_data.process_time/1000;
        stay_in_service_center+=serviceCustomer.c_data.elapse_time;
        printf("id:%ld,type:upgrade,arrived: %ld,started: %ld,processed: %d,exited: %ld,elapse: %ld \n",serviceCustomer.c_id,serviceCustomer.c_data.enter_time,serviceCustomer.c_data.start_time,serviceCustomer.c_data.process_time/1000,serviceCustomer.c_data.exit_time,serviceCustomer.c_data.elapse_time);
    }
    elapse_time_all=swlap(&clock);
    if(msgctl(upgrade_queue, IPC_RMID,NULL)==-1) perror("upgrade_queue,fail to delete msgctl");//delete
    printf("\nClerk for upgrade customers is quitting\n");
    printf("Clerk for upgrade customers: processed %d customers, elapse:%ld\n  work:%d wait:%d stay:%d\n",counterServiceCustomers,elapse_time_all,work_time,wait_time,stay_in_service_center);
    printf("  per customer: work:%d wait:%d stay:%d\n",work_time/counterServiceCustomers,wait_time/counterServiceCustomers,stay_in_service_center/counterServiceCustomers);
    *total_customers+=counterServiceCustomers;//update to shared memory
    exit(0);
}
void repairCustomers()
{
    int counterServiceCustomers=0;//count of customers in reapair service.
    int wait_time=0;//time of all customers waited in line.
    long elapse_time_all=0;
    int work_time=0;
    int stay_in_service_center=0;//time of all customers stayed in service center
    customer serviceCustomer;
    printf("Clerk for repair customers is starting\n");
    while(1){
        if((msgrcv(repair_queue,&serviceCustomer,sizeof(serviceCustomer)-sizeof(long),0,0))==-1) perror("repair_queue,fail msgrcv \n");
        if(serviceCustomer.c_data.type==TYPE_QUIT)//if type quit stop
        {
            break;
        }
        counterServiceCustomers++;
        serviceCustomer.c_data.start_time=swlap(&clock);
        usleep(serviceCustomer.c_data.process_time);
        serviceCustomer.c_data.exit_time=swlap(&clock);
        int elapse=serviceCustomer.c_data.exit_time-serviceCustomer.c_data.enter_time;
        serviceCustomer.c_data.elapse_time=elapse;
        wait_time+=serviceCustomer.c_data.start_time-serviceCustomer.c_data.enter_time;
        work_time+=serviceCustomer.c_data.process_time/1000;
        stay_in_service_center+=serviceCustomer.c_data.elapse_time;
        printf("id:%ld,type:repair,arrived: %ld,started: %ld,processed: %d,exited: %ld,elapse: %ld \n",serviceCustomer.c_id,serviceCustomer.c_data.enter_time,serviceCustomer.c_data.start_time,serviceCustomer.c_data.process_time/1000,serviceCustomer.c_data.exit_time,serviceCustomer.c_data.elapse_time);
    }
    elapse_time_all=swlap(&clock);
    if(msgctl(repair_queue, IPC_RMID,NULL)==-1) perror("repair_queue,fail to delete msgctl");//delete
    printf("\nClerk for repair customers is quitting\n");
    printf("Clerk for repair customers: processed %d customers, elapse:%ld\n  work:%d wait:%d stay:%d\n",counterServiceCustomers,elapse_time_all,work_time,wait_time,stay_in_service_center);
    printf("  per customer: work:%d wait:%d stay:%d\n",work_time/counterServiceCustomers,wait_time/counterServiceCustomers,stay_in_service_center/counterServiceCustomers);
    *total_customers+=counterServiceCustomers;//update to shared memory
    exit(0);
}