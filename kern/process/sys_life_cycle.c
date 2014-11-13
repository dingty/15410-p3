/** @file _life_cycle.c
 *
 *  @brief This file includes the implementation of the life cycle funcitons.
 *
 *  @author Xianqi Zeng (xianqiz)
 *  @author Tianyuan Ding (tding)
 *  @bug No known bugs
 */
#include <syscall.h>
#include "control_block.h"
#include "datastructure/linked_list.h"
#include "seg.h"
#include "cr.h"
#include "simics.h"
#include "malloc.h"
#include <elf/elf_410.h>
#include "common_kern.h"
#include "string.h"
#include "eflags.h"
#include "locks/mutex_type.h"
#include "simics.h"
#include "memory/vm_routines.h"
#include "scheduler.h"
extern TCB *current_thread;

int sys_thread_fork(void)
{
    unsigned int *kernel_stack =
        (unsigned int *)(current_thread -> stack_base +
                         current_thread->stack_size - 60);
    //lprintf("the kernel_stack is : %p", kernel_stack);
    current_thread -> registers.ss = kernel_stack[14];
    current_thread -> registers.esp = kernel_stack[13];
    current_thread -> registers.eflags = kernel_stack[12];
    current_thread -> registers.cs = kernel_stack[11];
    current_thread -> registers.eip = kernel_stack[10];
    current_thread -> registers.ebp = kernel_stack[9];
    current_thread -> registers.edi = kernel_stack[8];
    current_thread -> registers.esi = kernel_stack[7];
    current_thread -> registers.edx = kernel_stack[6];
    current_thread -> registers.ecx = kernel_stack[5];
    current_thread -> registers.ebx = kernel_stack[4];

    PCB *parent_pcb = current_thread -> pcb;
    list threads = parent_pcb -> threads;

    TCB *child_tcb = (TCB *)malloc(sizeof(TCB));

    child_tcb -> pcb = parent_pcb;
    child_tcb -> tid = next_tid;
    next_tid++;

    child_tcb -> state = THREAD_INIT;
    /*each thread has its own stack*/
    child_tcb -> stack_size = current_thread -> stack_size;
    child_tcb -> stack_base = memalign(4, child_tcb -> stack_size);
    child_tcb -> esp =
        (uint32_t)child_tcb->stack_base + (uint32_t)child_tcb->stack_size;
    child_tcb -> registers = current_thread -> registers;

    child_tcb -> registers.eax = 0;
    current_thread -> registers.eax = child_tcb -> tid;

    list_insert_last(&threads, &child_tcb->peer_threads_node);
    list_insert_last(&runnable_queue, &child_tcb->thread_list_node);

    return child_tcb -> tid;
}

void sys_set_status(int status)
{
    mutex_lock(&current_thread -> tcb_mutex);
    current_thread -> pcb -> return_state = status;
    mutex_unlock(&current_thread -> tcb_mutex);
    return;
}


void sys_vanish(void)
{
    lprintf("(x_x)_now sys_vanishs");
    // Get pcb for current process
    mutex_lock(&current_thread -> tcb_mutex);
    PCB *current_pcb = current_thread -> pcb;

    // display to the console by print()....

    list threads = current_pcb -> threads;
    lprintf("%d", threads.length);
    node *n;
    // Count how many peers haven't already exited
    int live_count = 0;
    for (n = list_begin (&threads); n != NULL; n = n -> next)
    {

        TCB *tcb = list_entry(n, TCB, peer_threads_node);
        if (tcb -> state != THREAD_EXIT)
        {
            lprintf("%d", live_count);
            live_count++;
        }
    }

    if (live_count == 1) // if this is the last thread
    {
        lprintf("(x_x)_in vanish: I am the last one");
        for (n = list_begin(&threads); n != NULL; n = n -> next)
        {
            TCB *tcb = list_entry(n, TCB, peer_threads_node);
            // We only reap threads that is not the current thread
            if (tcb -> tid != current_thread -> tid)
            {
                list_delete(&threads, n);
                list_delete(&blocked_queue, n);
                list_delete(&runnable_queue, n);
                // sfree(tcb -> stack_base, tcb -> stack_size);
                // free(tcb);
            }
        }

        current_pcb -> state = PROCESS_EXIT;
        mutex_lock(&process_queue_lock);
        list_delete(&process_queue, &current_pcb -> all_processes_node);
        mutex_unlock(&process_queue_lock);
        PCB *parent = current_pcb -> parent;

        // If the parent has already exited, wake up init
        if (parent == NULL || parent -> state == PROCESS_EXIT)
        {

            lprintf(" todo report the state to init");
            // MAGIC_BREAK;
        }
        else
        {
            // Because the parent is still alive
            // Find a thread that waits to reap this process(thread)
            // because we can ensure that there is only one thread left
            // for this process
            list parent_threads = parent -> threads;
            TCB *waiting_thread = NULL;
            for (n = list_begin(&parent_threads);
                    n != NULL;
                    n = n -> next)
            {
                waiting_thread = list_entry(n, TCB, peer_threads_node);
                // If we find a thread that is waiting, we delete it from the waiting
                // queue and make it runnable

                if (waiting_thread -> state == THREAD_WAITING)
                {
                    lprintf("I have found a parent %dthat is waiting me", waiting_thread->tid);
                    mutex_lock(&blocked_queue_lock);
                    list_delete(&blocked_queue, &waiting_thread->thread_list_node);
                    mutex_unlock(&blocked_queue_lock);

                    mutex_lock(&waiting_thread -> tcb_mutex);
                    waiting_thread -> state = THREAD_RUNNABLE;
                    mutex_unlock(&waiting_thread -> tcb_mutex);

                    mutex_lock(&runnable_queue_lock);
                    list_insert_last(&runnable_queue, &waiting_thread->thread_list_node);
                    mutex_unlock(&runnable_queue_lock);
                    break;
                }
            }
        }

    }
    // Set the current state to be exit
    current_thread -> state = THREAD_EXIT;

    mutex_unlock(&current_thread -> tcb_mutex);
    lprintf("(x_x)_vanish called schedule %d", current_thread -> tid);
    // context switch it back
    schedule(-1);

}

int sys_wait(int *status_ptr)
{
    // checkout ptr is not in kernel space, writable or not

    PCB *current_pcb = current_thread -> pcb;
    list *child_pros = &current_pcb -> children;
    lprintf("Note that %d has %d children", current_thread->tid, current_pcb -> children_count);
    if (current_pcb -> children_count == 0)
    {
        lprintf("%d, You can't wait", current_thread->tid);
        return -1;
    }
    node *n;

    for (n = list_begin (child_pros); n != NULL; n = n -> next)
    {
        PCB *pcb = list_entry(n, PCB, peer_processes_node);
        // Found one already exited child
        if (pcb -> state == PROCESS_EXIT)
        {
            int pid = pcb -> pid;
            // collects the return status
            if (status_ptr != NULL)
            {
                *status_ptr = pcb -> return_state;
            }
            // Reap this child
            lprintf("Reap this child %d", pid);
            list_delete(child_pros, &pcb -> peer_processes_node);
            current_pcb -> children_count--;

            // free(pcb -> PD);

            // free(pcb);
            return pid;
        }
    }
    // If no child_pros is exited, wait till one of them exits
    mutex_lock(&current_thread -> tcb_mutex);
    current_thread -> state = THREAD_WAITING;
    mutex_unlock(&current_thread -> tcb_mutex);
    lprintf("Not available, i %d call schedule", current_thread->tid);
    schedule(-1);
    lprintf("Someone wakes me %d up", current_thread->tid);


    // Do the thing again once one exited child wake me up
    for (n = list_begin (child_pros); n != NULL; n = n -> next)
    {
        PCB *pcb = list_entry(n, PCB, peer_processes_node);
        // Found one already exited child
        if (pcb -> state == PROCESS_EXIT)
        {
            int pid = pcb -> pid;
            // collects the return status
            if (status_ptr != NULL)
            {
                *status_ptr = pcb -> return_state;
            }
            // Reap this child
            lprintf("Reap the child after schedule");
            list_delete(child_pros, &pcb -> peer_processes_node);
            current_pcb -> children_count--;
            for (n = list_begin (&runnable_queue);
                    n != NULL;
                    n = n -> next)
            {
                TCB *tcb = list_entry(n, TCB, thread_list_node);
                lprintf("The tid in the runnable_queue is %d", tcb->tid);
            }
            // free(pcb -> PD);

            // free(pcb);
            return pid;
        }
    }
    return -1;

}

void destroy_pcb(PCB *pcb)
{

}

void destroy_tcb(TCB *tcb)
{

}