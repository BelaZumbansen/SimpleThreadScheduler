#include <tcb.h>
#include <stdbool.h>
#include <ucontext.h>
#include <stdint.h>
#include <stdlib.h>

void free_tcb(TCB *tcb) {
    free(tcb->context.uc_stack.ss_sp);
    free(tcb);
}