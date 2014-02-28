/*
 * Copyright (c) 2005, Johns Hopkins University and The EROS Group, LLC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 *  * Neither the name of the Johns Hopkins University, nor the name
 *    of The EROS Group, LLC, nor the names of their contributors may
 *    be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include "switches.h"
#include "debug.h"
#include "util.h"
#include "machine.h"
#include "decode.h"
#include "emit.h"
#include "xlcore.h"
#include "emit-support.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <sched.h>
#include <asm/unistd.h>
#include "signals.h"
#include <assert.h>
#include <setjmp.h>

#ifdef INLINE_EMITTERS
#define INLINE static inline
#include "emit-inline.c"
#endif

#ifdef SIGNALS

// TODO?: SIGNAL handling is not tested if hybrid translation
// USE_STATIC_DUMP is enabled.

Mlist_t *Mlist;
pthread_mutex_t Mlist_mutex;

void
addToMlist(Mlist_t *node)
{
  assert(node != NULL);
  if(pthread_mutex_lock(&Mlist_mutex) != 0)
    panic("Mlist_mutex lock error: %s\n", strerror(errno));
 
  node->next = Mlist;
  Mlist = node;
 
  if(pthread_mutex_unlock(&Mlist_mutex) != 0)
    panic("Mlist_mutex unlock error: %s\n", strerror(errno));
}

void
remFromMlist(int pid)
{
  if(Mlist == NULL)
    return;

  if(pthread_mutex_lock(&Mlist_mutex) != 0)
    panic("Mlist_mutex lock error: %s\n", strerror(errno));
 
  if(Mlist->pid == pid) {
    Mlist = Mlist->next;
  }
  else {
    Mlist_t *curr = Mlist;
    Mlist_t *prev = Mlist;
    for(; curr != NULL; curr = curr->next)
      if(curr->pid == pid) {
	prev->next = curr->next;
	break;
      }
  }
  
  if(pthread_mutex_unlock(&Mlist_mutex) != 0)
    panic("Mlist_mutex unlock error: %s\n", strerror(errno));
}

machine_t *
getMfromMlist(int pid)
{ 
  Mlist_t *curr;
  for(curr = Mlist; curr != NULL; curr = curr->next)
    if(curr->pid == pid)
      return curr->M;
  return NULL;
}

sigset_t allSignals;
register unsigned long curr_esp asm("esp") __attribute_used__;

static void
maskAllSignals(sigset_t *oldSet)
{
  sigprocmask(SIG_SETMASK, &allSignals, oldSet);  
}

void
restoreMask(sigset_t *oldSet)
{
  sigprocmask(SIG_SETMASK, oldSet, NULL);
}

void
masterSigHandler(int signum, volatile struct sigcontext ctx)
{
  sigset_t oldSet;
  sigset_t *oldSet_addr = &oldSet;  
  maskAllSignals(&oldSet);
  
  DEBUG(signal_capture)
    fprintf(DBG, " - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n\n");
  
  DEBUG(signal_note) 
    fprintf(DBG, "Master Signal Handler for: [%d] %s\n",
  	    signum, sig_names[signum]);
  
  machine_t *M = getMfromMlist(getpid());
  if(M == NULL) {    
    extern machine_t theMachine;
    extern pt_state thePtState;
    M = &theMachine;
    M->ptState = &thePtState;
  }

  k_sigaction *sa = &(M->ptState->sa_table[signum].new);
  sighandler_t sh = sa->sa_handler;  
  
  if(sh == NULL) {
    fprintf(DBG, "For this signal: %s, handler is NULL\n", 
	    sig_names[signum]);
    panic("Segmentation Fault, Core NOT dumped\n");
  }
  else
    assert((sh != SIG_DFL) && (sh != SIG_IGN));

  /* Set up accessors to my frame */
  char *myFramePtr = (char *) &signum;
  myFramePtr -= sizeof(char *); // Move up the return address
  
  /* Figure out where I am, so that I can think of making arrangements
     to deliver the signal */
  
  unsigned char *eip = (unsigned char *) ctx.eip;  
  unsigned char *MSHaddr = (unsigned char *) masterSigHandler;
  unsigned char *MSHendAddr = (unsigned char *) &&end_of_masterSigHandler;

  if(M->border_esp == 0) {
    
    if(eip > M->bbCache_main && eip < M->bbOut) {    
      DEBUG(signal_capture) 
	fprintf(DBG, "Signal %s arrived Within BBCache\n",
		sig_names[signum]);
    }
    else if(eip > M->bbCache && eip < M->bbCache_main) {
      DEBUG(signal_capture) 
	fprintf(DBG, "Signal %s arrived Within BBCache, in one of the Special BBs\n",
		sig_names[signum]);        
    }
    else if(eip > MSHaddr && eip < MSHendAddr) {
      DEBUG(signal_capture) 
	fprintf(DBG, "Signal %s arrived inside MasterSignalHandler\n",
		sig_names[signum]);
    }
    else {
      DEBUG(signal_capture) 
	fprintf(DBG, "Signal %s arrived elsewhere: 0x%lx.\n", 
		sig_names[signum], eip);
    }
  }
  else {
    DEBUG(signal_capture) 
      fprintf(DBG, "Signal %s arrived Within the Translator\n",
	    sig_names[signum]);        
  }
  

 simple_delivery:
  ;
  machine_t *sigM = init_signal_trans((unsigned long)sh, M);
  sigM->border_esp = 500;
  
  asm volatile ("mov %0, %%esp\n\t" // my function "returned"
		"pushf\n\t" 
		"pusha\n\t"		  
		"pushl %1\n\t"
		"call restoreMask\n\t"
		"leal 4(%%esp), %%esp\n\t" 
		"jmp *%2\n\t" // Branch to the translator
		:
		: "m" (myFramePtr), "a" (oldSet_addr), 
		  "S" (sigM->startup_slow_dispatch_bb)
		);

  panic("MasterSigHandler: \"unreachable code\" reached\n");
  
 end_of_masterSigHandler:
  return;
}

#if 0  
  DEBUG(signal_capture) {
    extern bb_entry *xlate_bb(machine_t *M);
    fprintf(DBG, "bbCache = 0x%lx  ", M->bbCache);
    fprintf(DBG, "past-spl = 0x%lx  ", M->bbCache_main);
    fprintf(DBG, "bbOut = 0x%lx ", M->bbOut);
    fprintf(DBG, "xlator = 0x%lx\n", &xlate_bb);

    void sigaction_syscall_pre(machine_t *M, fixregs_t regs);
    bool do_decode(machine_t *M, decode_t *ds);
    struct sigframe *f = (struct sigframe *) myFramePtr;
    fprintf(DBG, "masterSigHandler start = 0x%lx,  ", masterSigHandler);
    fprintf(DBG, "masterSigHandler end   = 0x%lx\n", &&end_of_masterSigHandler);
    fprintf(DBG, "do_decode   = 0x%lx  Return = %lx\n\n", do_decode, 
	    f->pretcode);        
  }
#endif
   
#endif /* SIGNALS */

