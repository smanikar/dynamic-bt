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

#include<string.h>
#include<stdio.h>
#include<sys/time.h>
#include<sys/resource.h>
#include<unistd.h>
#include<wait.h>
#include "switches.h"
#include "debug.h"
#include "perf.h"

//#define DO_HYBRID_PERF
#define DO_DYNAMORIO_PERF
#define DO_NULGRIND_PERF
#define DO_PIN_PERF

#ifdef DETAIL
static void
put_stats(const char *str, struct rusage *usage)
{
  printf("%s Statistics:\n", str);
  printf("User Time = %ld\n",(usage->ru_utime).tv_usec / 1000);
  printf("System Time = %ld\n", (usage->ru_stime).tv_usec / 1000);
  printf("Maximum Resident Set Size = %ld\n", usage->ru_maxrss);
  printf("Integral Shared Memory Size = %ld\n", usage->ru_ixrss);
  printf("Integral Unshared Data Size = %ld\n", usage->ru_idrss);
  printf("Integral Unshared Stack Size = %ld\n", usage->ru_isrss);
  printf("Major Page Faults = %ld\n", usage->ru_majflt);
  printf("Minor Page Faults = %ld\n", usage->ru_minflt);
  printf("Swaps = %ld\n", usage->ru_nswap);
  printf("Block Input Operations = %ld\n", usage->ru_inblock);
  printf("Block Output Operations = %ld\n", usage->ru_oublock);
  printf("Messages Sent = %ld\n", usage->ru_msgsnd);
  printf("Messages Received = %ld\n", usage->ru_msgrcv);
  printf("Signals Received = %ld\n", usage->ru_nsignals);
  printf("Voulantary Context Switches = %ld\n", usage->ru_nvcsw);
  printf("Invoulantary Context Switches = %ld\n\n", usage->ru_nivcsw);
  fflush(stdout);
}
#endif

static unsigned long long
run(const char* runstr, struct rusage *usage, char *argv[])
{
  int status, pid;
  unsigned long long timeb, timea;
  timea = read_timer(); 

  if((pid = fork()) == 0)
    {
      execvp(runstr, argv);
      _exit(0);
    }
	
  wait4(pid, &status, 0, usage);

  timeb = read_timer();
  return((signed long long)timeb - (signed long long)timea); 
}


int
main(int argc, char *argv[])
{
  unsigned long long normal, nvdebug, nvdebug2, dynamorio, valgrind, pin;
  struct rusage nusage, vusage, v2usage, dusage, gusage, pusage; 
	
  system("clean-dumps.sh");
  wait(NULL);

  printf("Running Native:\n");
  normal = run("empty.sh", &nusage, argv);
  printf("-------------------------------------------------\n\n");
  printf("Running within HDTrans:\n");
  nvdebug = run("HDTrans", &vusage, argv);
  printf("-------------------------------------------------\n\n");
#ifdef DO_HYBRID_PERF
  printf("Running within n2VDebug:\n");
  nvdebug2 = run("N0vdbg.sh", &v2usage, argv);
  nvdebug2 = run("N0vdbg.sh", &v2usage, argv);
  printf("-------------------------------------------------\n\n");
#endif
#ifdef DO_DYNAMORIO_PERF
  printf("Running within DynamoRIO:\n");
  dynamorio = run("dynamorio", &dusage, argv);
  printf("-------------------------------------------------\n\n");
#endif
#ifdef DO_NULGRIND_PERF
  printf("Running within NulGrind:\n");
  valgrind = run("Val.sh", &gusage, argv);
  printf("-------------------------------------------------\n\n");
#endif
#ifdef DO_PIN_PERF
  printf("Running within Pin:\n");
  pin = run("Pin.sh", &pusage, argv);
  printf("-------------------------------------------------\n\n");
#endif
  printf("Without VDebug = %llu\n",(normal)); 
  printf("With nVDebug   = %llu\n",(nvdebug)); 
#ifdef DO_HYBRID_PERF
  printf("With n2VDebug  = %llu\n",(nvdebug2)); 
#endif
#ifdef DO_DYNAMORIO_PERF
  printf("With DynamoRIO = %llu\n",(dynamorio)); 
#endif
#ifdef DO_NULGRIND_PERF
  printf("With NulGrind = %llu\n",(valgrind)); 
#endif
#ifdef DO_PIN_PERF
  printf("With Pin = %llu\n",(pin)); 
#endif
  printf("Performance Overhead nVDebug = %.2Lf \n",(((long double) nvdebug)/normal)); 
#ifdef DO_HYBTID_PERF
  printf("Performance Overhead n2VDebug = %.2Lf \n",(((long double) nvdebug2)/normal)); 
#endif
#ifdef DO_DYNAMORIO_PERF
  printf("Performance Overhead DynamoRIO = %.2Lf \n",(((long double) dynamorio)/normal)); 
#endif
#ifdef DO_NULGRIND_PERF
  printf("Performance Overhead NulGrind = %.2Lf \n",(((long double) valgrind)/normal)); 
#endif
#ifdef DO_PIN_PERF
  printf("Performance Overhead Pin = %.2Lf \n",(((long double) pin)/normal)); 
#endif
  printf("-------------------------------------------------\n\n");
#ifdef DETAIL
  put_stats("Native   ", &nusage);	
  put_stats("nVDebug   ", &vusage);	

#ifdef DO_HYBRID_PERF
  put_stats("n2VDebug  ", &v2usage);	
#endif
#ifdef DO_DYNAMORIO_PERF
  put_stats("DynamoRIO ", &dusage);	
#endif
#ifdef DO_NULGRIND_PERF
  put_stats("NulGrind ", &gusage);	
#endif
#ifdef DO_PIN_PERF
  put_stats("Pin ", &pusage);	
#endif
#endif
}
