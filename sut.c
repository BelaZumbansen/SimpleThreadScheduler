#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>
#include<ucontext.h>
#include<queue.h>
#include<tcb.h>
#include<sut.h>

typedef struct {
    int fd;
    char *buf;
    int size;
} read_write_arg;

typedef struct {
    pthread_t *C_EXEC;
    ucontext_t comp_parent;
    TCB *running_task;
} EXECUTING_THREAD;

int task_id, shutdown, num_tasks;
int OPEN_FAILURE = -1;
int NUM_COMP_THREADS = 2;

pthread_mutex_t ready_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t request_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t freeable_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t task_id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t task_count_mutex = PTHREAD_MUTEX_INITIALIZER;

EXECUTING_THREAD *C_EXEC_0;
EXECUTING_THREAD *C_EXEC_1;

// Kernel Threads
pthread_t *I_EXEC;

// Parent Contexts
ucontext_t io_parent;

// Running context
TCB *request_task;

// Queues
Queue *task_ready_queue;
Queue *task_wait_queue;
Queue *request_queue;
Queue *freeable_queue;

EXECUTING_THREAD *get_executor() {

    if (NUM_COMP_THREADS == 1) {
        return C_EXEC_0;
    }

    if (pthread_equal(*(C_EXEC_0->C_EXEC), pthread_self())) {
        return C_EXEC_0;
    } else {
        return C_EXEC_1;
    }
}

void *listen_ready_queue() {

    TCB *head;
    EXECUTING_THREAD *executor = get_executor();
    getcontext(&(executor->comp_parent));

    while(1) {

        if (shutdown) {
            pthread_exit(NULL);
        }

        pthread_mutex_lock(&ready_queue_mutex);
        head = pop(task_ready_queue);
        pthread_mutex_unlock(&ready_queue_mutex);

        if (head != NULL) {

            executor = get_executor();   
            executor->running_task = head;
            swapcontext(&(executor->comp_parent), &(executor->running_task->context));
        } else {
           usleep(100); 
        }
    }
}

void *listen_request_queue() {

    TCB *head;
    int id_to_retrieve;

    getcontext(&io_parent);

    while(1) {

        if (shutdown) {
            pthread_exit(NULL);
        }

        pthread_mutex_lock(&request_queue_mutex);
        head = pop(request_queue);
        pthread_mutex_unlock(&request_queue_mutex);
        
        if (head != NULL) {

            id_to_retrieve = head->id;
            
            request_task = head;
            swapcontext(&io_parent, &(head->context));
        } else {
            usleep(100);
        }
    }
}

void move_from_wait_queue() {

    TCB *waiting_task;

    while (1) {

        pthread_mutex_lock(&wait_queue_mutex);
        waiting_task = remove_by_id(task_wait_queue, request_task->id);
        pthread_mutex_unlock(&wait_queue_mutex);

        if (waiting_task != NULL) {
            break;
        }
    }

    waiting_task->return_val = request_task->return_val;

    pthread_mutex_lock(&ready_queue_mutex);
    insert(task_ready_queue, waiting_task);
    pthread_mutex_unlock(&ready_queue_mutex);
}

void open_helper() {

    char *fname = (char *)(request_task->argument);

    FILE *fptr;
    fptr = fopen(fname, "r+");

    if (fptr == NULL) {
        request_task->return_val=&OPEN_FAILURE;
    }
    request_task->return_val=fptr;

    move_from_wait_queue();
    setcontext(&io_parent);
}

void close_helper() {

    int *fd = (int *)(request_task->argument);
    close(*fd);

    move_from_wait_queue();
    setcontext(&io_parent);
}

void read_helper() {

    ssize_t bytes_read;
    read_write_arg *r = (read_write_arg *)(request_task->argument);

    bytes_read = read(r->fd, r->buf, r->size);

    if (bytes_read == 0) {
        request_task->return_val=NULL;
    } else {
        request_task->return_val=(r->buf);
    }

    move_from_wait_queue();
    setcontext(&io_parent);
}

void write_helper() {

    read_write_arg *w = (read_write_arg *)(request_task->argument);
    write(w->fd, w->buf, w->size);

    move_from_wait_queue();
    setcontext(&io_parent);
}

EXECUTING_THREAD *create_io_request(sut_task_f request, void *argument) {

    TCB *io_tcb = (TCB *)malloc(sizeof(TCB));
    EXECUTING_THREAD *executor = get_executor();

    ucontext_t io_context;
    getcontext(&io_context);
    io_context.uc_link=0;
    io_context.uc_stack.ss_sp=malloc(64000);
    io_context.uc_stack.ss_size=64000;
    io_context.uc_stack.ss_flags=0;

    makecontext(&io_context, request, 0);

    io_tcb->context = io_context;
    io_tcb->id = executor->running_task->id;
    io_tcb->argument = argument;
    io_tcb->return_val = NULL;

    pthread_mutex_lock(&freeable_queue_mutex);
    insert(freeable_queue, io_tcb);
    pthread_mutex_unlock(&freeable_queue_mutex);

    pthread_mutex_lock(&request_queue_mutex);
    insert(request_queue, io_tcb);
    pthread_mutex_unlock(&request_queue_mutex);

    return executor;
}

int sut_open(char *fname) {
    
    EXECUTING_THREAD *executor = create_io_request(open_helper, (void *)fname);
    
    TCB *getting_replaced;
    getting_replaced = executor->running_task;
    
    pthread_mutex_lock(&wait_queue_mutex);
    insert(task_wait_queue, getting_replaced);
    pthread_mutex_unlock(&wait_queue_mutex);
    
    swapcontext(&(getting_replaced->context), &executor->comp_parent);

    executor = get_executor();
    return fileno((FILE *)executor->running_task->return_val);
}

void sut_close(int fd) {

    EXECUTING_THREAD *executor = create_io_request(close_helper, (void *)&fd);

    TCB *getting_replaced;
    getting_replaced = executor->running_task;
    
    pthread_mutex_lock(&wait_queue_mutex);
    insert(task_wait_queue, getting_replaced);
    pthread_mutex_unlock(&wait_queue_mutex);
    
    swapcontext(&(getting_replaced->context), &executor->comp_parent);
    return;
}

char *sut_read(int fd, char *buf, int size) {
    
    read_write_arg *read_arg = (read_write_arg *)malloc(sizeof(read_write_arg));

    read_arg->fd = fd;
    read_arg->buf = buf;
    read_arg->size = size;

    EXECUTING_THREAD *executor = create_io_request(read_helper, (void *)read_arg);
    
    TCB *getting_replaced;
    getting_replaced = executor->running_task;
    getting_replaced->argument = read_arg;
    
    pthread_mutex_lock(&wait_queue_mutex);
    insert(task_wait_queue, getting_replaced);
    pthread_mutex_unlock(&wait_queue_mutex);
    
    swapcontext(&(getting_replaced->context), &executor->comp_parent);

    executor = get_executor(); 

    free(executor->running_task->argument);
    return (char *)executor->running_task->return_val;
}

void sut_write(int fd, char *buf, int size) {

    read_write_arg *write_arg = (read_write_arg *)malloc(sizeof(read_write_arg));

    write_arg->fd = fd;
    write_arg->buf = buf;
    write_arg->size = size;

    EXECUTING_THREAD *executor = create_io_request(write_helper, (void *)write_arg);

    TCB *getting_replaced;
    getting_replaced = executor->running_task;

    getting_replaced->argument = write_arg;
    
    pthread_mutex_lock(&wait_queue_mutex);
    insert(task_wait_queue, getting_replaced);
    pthread_mutex_unlock(&wait_queue_mutex);

    swapcontext(&(getting_replaced->context), &executor->comp_parent); 

    executor = get_executor(); 
    free(executor->running_task->argument);
    return;
}

void sut_init() {

    task_id = 0;
    num_tasks = 0;
    shutdown = 0;

    C_EXEC_0 = (EXECUTING_THREAD *)malloc(sizeof(EXECUTING_THREAD));
    C_EXEC_0->C_EXEC = (pthread_t*)malloc(sizeof(pthread_t));
    if (NUM_COMP_THREADS == 2) {
        C_EXEC_1 = (EXECUTING_THREAD *)malloc(sizeof(EXECUTING_THREAD));
        C_EXEC_1->C_EXEC = (pthread_t*)malloc(sizeof(pthread_t));
    }
    I_EXEC = (pthread_t*)malloc(sizeof(pthread_t));

    task_ready_queue = create_queue();
    task_wait_queue = create_queue();
    request_queue = create_queue();
    freeable_queue = create_queue();
    
    pthread_create(C_EXEC_0->C_EXEC, NULL, listen_ready_queue, NULL);
    if (NUM_COMP_THREADS == 2) {
        pthread_create(C_EXEC_1->C_EXEC, NULL, listen_ready_queue, NULL);
    }
    pthread_create(I_EXEC, NULL, listen_request_queue, NULL);
}

void sut_yield() {

    TCB *getting_replaced;
    EXECUTING_THREAD *executor = get_executor();

    getting_replaced = executor->running_task;

    pthread_mutex_lock(&ready_queue_mutex);
    insert(task_ready_queue, getting_replaced);
    pthread_mutex_unlock(&ready_queue_mutex);

    swapcontext(&(getting_replaced->context), &executor->comp_parent);
}

void sut_exit() {

    EXECUTING_THREAD *executor = get_executor();

    pthread_mutex_lock(&task_count_mutex);
    num_tasks--;
    pthread_mutex_unlock(&task_count_mutex);

    setcontext(&executor->comp_parent);
}

void sut_shutdown() {

    TCB *to_free;
    usleep(500);
    int i;

    while(1) {

        if (num_tasks == 0) {

            shutdown = 1;

            pthread_join(*C_EXEC_0->C_EXEC, NULL);
            
            if (NUM_COMP_THREADS == 2) {
                pthread_join(*C_EXEC_1->C_EXEC, NULL);
            }
            
            pthread_join(*I_EXEC, NULL);
            free(C_EXEC_0->C_EXEC);
            free(C_EXEC_0);

            if (NUM_COMP_THREADS == 2) {
                free(C_EXEC_1->C_EXEC);
                free(C_EXEC_1);
            }

            //free(C_EXEC_1);
            free(I_EXEC);
            free_queue(task_ready_queue);
            free_queue(task_wait_queue);
            free_queue(request_queue);

            to_free = pop(freeable_queue);
            while (to_free != NULL) {
                free_tcb(to_free);
                to_free = pop(freeable_queue);
            }
            free_queue(freeable_queue);
            exit(EXIT_SUCCESS);
        }
        else {
            usleep(100);
        }
    }
}

bool sut_create(sut_task_f fn) {

    TCB *new_tcb = (TCB *)malloc(sizeof(TCB));
    int new_task_id;

    pthread_mutex_lock(&task_count_mutex);
    num_tasks++;
    pthread_mutex_unlock(&task_count_mutex);

    pthread_mutex_lock(&task_id_mutex);
    new_task_id = task_id++;
    pthread_mutex_unlock(&task_id_mutex);

    ucontext_t new_process;
    getcontext(&new_process);
    new_process.uc_link=0;
    new_process.uc_stack.ss_sp=malloc(64000);
    new_process.uc_stack.ss_size=64000;
    new_process.uc_stack.ss_flags=0;

    makecontext(&new_process, fn, 0);

    new_tcb->context = new_process;
    new_tcb->id = new_task_id;
    new_tcb->argument = NULL;
    new_tcb->return_val = NULL;

    pthread_mutex_lock(&freeable_queue_mutex);
    insert(freeable_queue, new_tcb);
    pthread_mutex_unlock(&freeable_queue_mutex);

    pthread_mutex_lock(&ready_queue_mutex);
    insert(task_ready_queue, new_tcb);
    pthread_mutex_unlock(&ready_queue_mutex);

    return 1;
}