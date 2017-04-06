/*
Operating Systems Project Assignment #3
Threads & Synchronization
Selen PARLAR 150113049
S. Talha ÖZTÜRK 150113068 
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h> 
#include <semaphore.h>
#include <sys/time.h>
#include <ctype.h>
#define MAXSIZE 65 /* Maximum character size*/

FILE *file; /* for input file*/
FILE *logFile; /* for log output file*/

struct queueItself *qRead; /* first queue for read and replace threads*/
struct queueItself *qReplace; /* second queue for replace and upper threads*/
struct queueItself *qUpper; /* third queue for upper and writer threads*/

sem_t rr; /* semaphore for read-replace*/
sem_t ru; /* semaphore for replace-upper*/
sem_t uw; /* semaphore for upper-writer*/

pthread_mutex_t mutexRR = PTHREAD_MUTEX_INITIALIZER; /* mutex for read-replace*/
pthread_mutex_t mutexRU = PTHREAD_MUTEX_INITIALIZER; /* mutex for replace-upper*/
pthread_mutex_t mutexUW = PTHREAD_MUTEX_INITIALIZER; /* mutex for upper-writer*/

/* A linked list (LL) node to store a queue entry */
struct QueueNode
{
    char line[MAXSIZE];
    struct QueueNode *next;
};
 
/* The queue, head stores the head node of LL and last stores the last node of LL */
struct queueItself
{
    struct QueueNode *head, *last;
};
 
/* Function to create a new linked list node. It takes line as parameter and copies into temp node*/
struct QueueNode* newItems(char line[])
{
    struct QueueNode *temp = (struct QueueNode*)malloc(sizeof(struct QueueNode));
    strcpy(temp->line,line);
    temp->next = NULL;
    return temp; 
}
 
/* Creates a node sized queue and returns itself. */
struct queueItself *queueCreated()
{
    struct queueItself *queue = (struct queueItself*)malloc(sizeof(struct queueItself));
    queue->head = queue->last = NULL;
    return queue;
}
 
/* Function to remove a line from given queue */
struct QueueNode *removeeFromQueue(struct queueItself *queue)
{
    /* If queue is empty, return NULL. */
    if (queue->head == NULL)
       return NULL;
 
    /* Store previous head and move head one node ahead */
    struct QueueNode *temp = queue->head;
    queue->head = queue->head->next;
 
    /* If head becomes NULL, then change last also as NULL */
    if (queue->head == NULL)
       queue->last = NULL;
    return temp;
}

/* The function to add given line to given queue */
void addtoQueue(struct queueItself *queue, char line[]){
    /* Create a new linled list node */
    struct QueueNode *temp = newItems(line);
 
    /* If queue is empty, then new node is head and last both */
    if (queue->last == NULL)
    {
       queue->head = queue->last = temp;
       return;
    }
 
    /* Add the new node at the end of queue and change last */
    queue->last->next = temp;
    queue->last = temp;
}

/* Reader function for reader thread. It reads lines from given input file and adds these lines to qRead queue */ 
void *reader(){
    qRead = queueCreated(); /* creation of queue*/
	char line[MAXSIZE]; /* max size char array for lines */
	char temp[1000]; /* temp array for truncate line later */

	/* reads lines from file till end of file and copies these lines to temp array 
	 * strncpy provides string copy with given integer times 
	 * since qRead is a global variable we need to protect it. So mutex locks mutexRR for critical section. In this case,
	 * critical section is addtoQueue. sem_post increases rr semaphore by one for let replace thread to know that there is 
	 * a line in the queue. Thread will terminate when the input file is reaches end of file. */
	while (fgets(temp, sizeof(temp), file)!=NULL) {
		strncpy(line,temp,MAXSIZE);
	    line[MAXSIZE-2]='\n';
	    line[MAXSIZE-1]='\0';
	    pthread_mutex_lock(&mutexRR);
        addtoQueue(qRead, line);
        /* prints lines in to log file */
        fprintf(logFile, "Reader-Thread\t <The line before update>\t%s \t\t  <The line after update>\tNo update\n",line);
        pthread_mutex_unlock(&mutexRR);
        sem_post(&rr);
    }
    /* replace thread should know when to stop so when all lines in the input file is done with reading, "end" will be added to qRead */
    pthread_mutex_lock(&mutexRR);
    addtoQueue(qRead,"end");
    pthread_mutex_unlock(&mutexRR);
    sem_post(&rr);
}

/* Replace function for replace thread. It replaces lines from qRead and adds these replaced lines to qReplace queue */
void *replace(){
    qReplace = queueCreated();    
    char beforeUpdate[MAXSIZE]; /* It stores lines before update */

    /* Replace thread will wait until rr equals 1 since initial value of rr is 0, there must be a line in the queue 
     * It should also wait for removing from read queue since reader thread may add a line to qRead */
    while(1){
    	sem_wait(&rr);
	    pthread_mutex_lock(&mutexRR);
    	struct QueueNode *n = removeeFromQueue(qRead);
    	/* It checks whether reader thread is terminated or not. If it is, it unlocks mutexRR before terminates.
    	 * And also, it adds "end" keyword to qReplace queue for let upper thread know replace thread is terminated. 
    	 * It increase ru semaphore before terminates since after compiler sees return 0, it won't execute rest of it. */
    	if(strcmp(n->line,"end")==0){
    	   pthread_mutex_unlock(&mutexRR);
    	   pthread_mutex_lock(&mutexRU);
           addtoQueue(qReplace,"end");
           pthread_mutex_unlock(&mutexRU);
           sem_post(&ru);
    	   
           return 0;
    	}
        strcpy(beforeUpdate,n->line);
        /* checks every character in the line array and converts spaces with "-" */
    	for(int i=0;i<MAXSIZE;i++){
        	switch (n->line[i]){
            	case ' ':
	            n->line[i] = '-';
    	        break;

            	default: continue;
        	}
	    }
	    /* It locks replace-upper mutex for adding replaced line in to qReplace */
	    pthread_mutex_lock(&mutexRU);
	    addtoQueue(qReplace, n->line);  
	    /* prints before updated lines and updated lines in to log file */
        fprintf(logFile, "Replace-Thread\t<The line before update>\t%s \t\t  <The line after update>\t%s",beforeUpdate,n->line);
	    pthread_mutex_unlock(&mutexRU);
	    pthread_mutex_unlock(&mutexRR);
	    sem_post(&ru);
	}
}

/* Upper function for upper thread. It converts lowercase letters to uppercase and these uppered lines to qUpper queue */
void *upper(){
    qUpper = queueCreated();
    char beforeUpdate[MAXSIZE]; /* It stores lines before update */
    /* Upper thread will wait until ru equals 1 since initial value of ru is 0, there must be a line in the queue
     * It should also wait for removing from replace queue since replace thread may add a line to qReplace */ 
    while(1){
    	sem_wait(&ru);
    	pthread_mutex_lock(&mutexRU);
    	struct QueueNode *n = removeeFromQueue(qReplace);
    	/* It checks whether replace thread is terminated or not. If it is, it unlocks mutexRU before terminates.
    	 * And also, it adds "end" keyword to qUpper queue for let writer thread know upper thread is terminated. 
    	 * It increase uw semaphore before terminates since after compiler sees return 0, it won't execute rest of it. */
    	if(strcmp(n->line,"end")==0){
    		pthread_mutex_unlock(&mutexRU);
    		pthread_mutex_lock(&mutexUW);
    		addtoQueue(qUpper,"end");
            pthread_mutex_unlock(&mutexUW);
            sem_post(&uw);
    		return 0;
    	}
        strcpy(beforeUpdate,n->line);
        /* -It checks every character in the line whether it is lowercase letter or not. If it is, it converts lowercase letters to uppercase */
    	for(int i=0;i<MAXSIZE;i++){
        	if(n->line[i]>='a' && n->line[i]<='z'){
            	n->line[i]=('A'+n->line[i]-'a');
        	}else n->line[i]=n->line[i];
    	}
    	/* It locks upper-writer mutex for adding uppered line in to qUpper */
    	pthread_mutex_lock(&mutexUW);
    	addtoQueue(qUpper, n->line);
    	/* prints before updated lines and updated lines in to log file */
        fprintf(logFile, "Upper-Thread\t<The line before update>\t%s \t\t  <The line after update>\t%s",beforeUpdate,n->line);
    	pthread_mutex_unlock(&mutexUW);
    	pthread_mutex_unlock(&mutexRU);
    	sem_post(&uw);
	}
}

/* Writer function for writer thread. It prints final lines in to stdout */
void *writer(){
    int numofLines=0; /* variable for holding number of lines */
    int j=1;
    /* Writer thread will wait until uw equals 1 since initial value of uw is 0, there must be a line in the queue.
     * It should also wait for removing from upper queue since upper thread may add a line to qUpper */ 
    while(1){
    	sem_wait(&uw);
    	pthread_mutex_lock(&mutexUW);
    	struct QueueNode *n = removeeFromQueue(qUpper);
    	/* It checks whether upper thread is terminated or not. If it is, it prints number of lines to stdout and terminates. */
    	if(strcmp(n->line,"end")==0){
    		pthread_mutex_unlock(&mutexUW);
    		fprintf(stdout, "Number of lines: %d\n", numofLines);
    		return 0;
    	} 	
    	/* prints before updated lines and updated lines in to log file */
    	fprintf(logFile, "Writer-Thread\t <The line before update>\t%s \t\t  <The line after update>\tNo update\n",n->line);
        fprintf(stdout, "%d. %s ", j, n->line);
    	pthread_mutex_unlock(&mutexUW);
        numofLines++; j++;
    }
}

int main(int argc, char *argv[])
{
	/* Initialization of semaphore. */
	sem_init(&rr,0,0); 
	sem_init(&ru,0,0);
	sem_init(&uw,0,0);

	/* declaration of threads*/
	pthread_t read_thread; 
	pthread_t replace_thread;
	pthread_t upper_thread;
	pthread_t write_thread;
	char *filename; /* It holds given input file's name*/

    
	fprintf(stdout, "Program name is %s\n", argv[0]);

	/* It checks given inputs. */
	if( argc == 2 ) {
      	printf("The input file name is %s\n", argv[1]);
      	
      	filename = argv[1];
        file=fopen(filename,"r"); 
        logFile=fopen("logFile.txt","w");
        fprintf(logFile, "<Thread type>\n");
		
		/* creation of threads */
		pthread_create(&read_thread, NULL,reader,NULL);
    	pthread_create(&replace_thread,NULL,replace,NULL);
    	pthread_create(&upper_thread,NULL,upper, NULL);
    	pthread_create(&write_thread,NULL,writer, NULL);
    	/* it waits threads to terminate */
    	pthread_join(read_thread,NULL);
		pthread_join(replace_thread,NULL);
    	pthread_join(upper_thread,NULL);
    	pthread_join(write_thread,NULL);
    	
    	/* closing files */
    	fclose(file);
        fclose(logFile);
   	}
   	else if( argc > 2 ) printf("Too many arguments supplied.\n");
   	else  printf("One argument expected.\n");

	return 0;
}