#ifndef __TCB_H__
#define __TCB_H__

#include <stdbool.h>
#include <ucontext.h>
#include <stdint.h>

// Task Control Block
typedef struct {
    int id;
    ucontext_t context;
    void *argument;
    void *return_val;
} TCB;

// Function to free memory associated with TCB
void free_tcb(TCB *tcb);

#endif