#ifndef COMP310_A2_Q
#define COMP310_A2_Q

#include "tcb.h"
#include <unistd.h>

typedef struct queue Queue;

Queue *create_queue();

TCB *pop(Queue *q);

void insert(Queue *q, TCB *tcb);

TCB *retrieve_by_id(Queue *q, int id);

void free_queue(Queue *q);

TCB *remove_by_id(Queue *q, int id);

#endif