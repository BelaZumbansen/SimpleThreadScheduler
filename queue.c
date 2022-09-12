#include <queue.h>
#include <stdlib.h>

struct queue_entry {
    TCB *tcb;
    struct queue_entry *next;
    struct queue_entry *previous;
};

struct queue {
    struct queue_entry *head;
    struct queue_entry *tail;
};

Queue *create_queue() {

    Queue *new_queue = (Queue*)malloc(sizeof(Queue));
    new_queue->head = NULL;
    new_queue->tail = NULL;

    return new_queue;
}

void insert(Queue *q, TCB *tcb) {

    struct queue_entry *new_entry = (struct queue_entry *)malloc(sizeof(struct queue_entry));
    new_entry->tcb = tcb;
    new_entry->previous = NULL;
    new_entry->next = NULL;

    if (q->head == NULL) {
        q->head = new_entry;
        q->tail = new_entry;
    } else {
        q->tail->previous = new_entry;
        new_entry->next = q->tail;
        q->tail = new_entry;
    }
}

TCB *pop(Queue *q) {

    if (q->head == NULL) {
        return NULL;
    }
    else {
        struct queue_entry *cur_head = q->head;
        TCB *rtn = cur_head->tcb;
        q->head = cur_head->previous;

        if (q->head == NULL) {
            q->tail = NULL;
        }
        else {
            q->head->next = NULL;
        }

        free(cur_head);
        cur_head = NULL;
        return rtn;
    }
}

TCB *retrieve_by_id(Queue *q, int id) {
    
    TCB *tcb;
    struct queue_entry *e;

    if (q->tail == NULL) {
        return NULL;
    }

    e = q->tail;

    while(e != NULL) {

        if (e->tcb->id == id) {
            
            tcb = e->tcb;
            return tcb;
        }

        e = e->next;
    }

    return NULL;
}

void free_queue(Queue *q) {
    free(q);
}

TCB *remove_by_id(Queue *q, int id) {

    TCB *tcb;
    struct queue_entry *e;

    if (q->tail == NULL || q->head == NULL) {
        return NULL;
    }

    e = q->tail;

    while(e != NULL) {

        if (e->tcb->id == id) {
            
            if (e->previous != NULL) {
                e->previous->next = e->next;
            }
            else if (e->next != NULL) {
                e->next->previous = NULL;
                q->tail = e->next;
            }
            else {
                q->head = NULL;
                q->tail = NULL;
            }

            if (e->next != NULL) {
                e->next->previous = e->previous;
            }
            else if (e->previous != NULL) {
                e->previous->next = NULL;
                q->head = e->previous;
            }
            else {
                q->head = NULL;
                q->tail = NULL;
            }

            tcb = e->tcb;
            free(e);
            return tcb;
        }

        e = e->next;
    }

    return NULL;
}