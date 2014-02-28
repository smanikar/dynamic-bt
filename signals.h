#ifndef SIGNALS_H
#define SIGNALS_H 
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

#include <pthread.h>
#include <signal.h>
#include <bits/signum.h>

#define __USE_GNU
#include <ucontext.h>
#undef __USE_GNU


#define NSIGNALS _NSIG
#define _NSIG_WORDS ((_NSIG)/32)
typedef struct sigaction k_sigaction;
typedef struct sigcontext k_sigcontext;
typedef void (*sighandler_t)(int);

struct machine_s;

typedef struct pushaf_s pushaf_t;
struct pushaf_s {
  unsigned long edi;
  unsigned long esi;
  unsigned long ebp;
  unsigned long esp;
  unsigned long ebx;
  unsigned long edx;
  unsigned long ecx;
  unsigned long eax;
  unsigned long eflags;
}__attribute__((packed));


void masterSigHandler (int signum, struct sigcontext ctx);
void deliverSignal(struct machine_s *M, pushaf_t pt_regs);

/* We need a global table of pid->Mstate mappings, so that we can find
   it from within the signal handler */

/* FIX: I am using a linked list here. It is the WORST possible
   datastructure that works. Replace this with a hash table whenever
   someone has time. */
struct machine_s;
typedef struct Mlist_s Mlist_t;
struct Mlist_s {
  int pid;
  struct machine_s *M;
  struct Mlist_s *next;
};

extern Mlist_t *Mlist;
extern pthread_mutex_t Mlist_mutex;
extern sigset_t allSignals;

/* sigframe structures of the Linux kernel */

struct sigframe
{
  char *pretcode;
  int sig;
  struct sigcontext sc;
  struct _fpstate fpstate;
  unsigned long extramask[_NSIG_WORDS-1];
  char retcode[8];
};

struct rt_sigframe
{
  char *pretcode;
  int sig;
  struct siginfo *pinfo;
  void *puc;
  struct siginfo info;
  struct ucontext uc;
  struct _fpstate fpstate;
  char retcode[8];
};

/* Generic tagged union of these structures */

typedef struct sigframe_s sigframe_t;
struct sigframe_s {
  void *frameAddr;
  union {
    struct sigframe sf;
    struct rt_sigframe rtsf;
  } frame;
};

#define SIGQ_DEPTH 64 

void addToMlist(Mlist_t *node);
void remFromMlist(int pid);
struct machine_s *getMfromMlist(int pid);

#endif /* SIGNALS_H */
