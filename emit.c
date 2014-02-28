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
#include "emit-support.h"
#include "xlcore.h"
#include "perf.h"
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

#ifdef INLINE_EMITTERS
#define INLINE static inline
#include "emit-inline.c"
#else
#define INLINE
#endif

#include "xlate-helper.c"

/*********************************************************************
Emit Normal - All other instructions
*********************************************************************/

bool
emit_normal(machine_t *M, decode_t *d)
{
  unsigned i;
  unsigned count = M->next_eip - d->decode_eip;

  DEBUG(emits)
    fprintf(DBG, "%lu: Normal %lx %s\n", M->nTrInstr, d->decode_eip, ((OpCode *) d->pEntry)->disasm);

#ifdef PROFILE
  M->ptState->s_normal_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->normal_cnt));
#endif

  /*   for (i = 0; i < count; i++) { */
  /* #ifdef STATIC_PASS */
  /*     bb_emit_byte(M, ((unsigned char *)d->mem_decode_eip)[i]);  */
  /* #else */
  /*     bb_emit_byte(M, ((unsigned char *)d->decode_eip)[i]);  */
  /* #endif */
  /*   } */

#ifdef STATIC_PASS
  memcpy(M->bbOut, (unsigned char *)d->mem_decode_eip, count);
#else
  memcpy(M->bbOut, (unsigned char *)d->decode_eip, count);
#endif
  M->bbOut += count;

  return false; 
}

/*********************************************************************
Emit INT - To capture exit only
*********************************************************************/

#define Aux1(x) (x), PERC((x), M->ptState->total_cnt)
#define Aux2(x) (x), PERC((x), M->ptState->s_total_cnt)

#if (defined(USE_STATIC_DUMP) || defined(STATIC_PASS))
#define ENABLE_DUMP_FUNCTION
#endif

#ifdef ENABLE_DUMP_FUNCTION
/* 1) Warning: Will modify str */
void
dump_to_file(machine_t *M, char *str)
{
  char arg[300]; 
  unsigned long i;
  struct stat buf;
  unsigned long len = strlen(str);

  /* This has to be mmaped eventually ...
     Better abort than buffer overflow */
  if(len > 300) {
    fprintf(DBG, "len > 300; Temporarily aborting; Did not dump.");
    return;
  }

  for(i=0; i<len; i++)
    if(str[i] == '/')
      str[i] = '_';
      
#ifdef CHECK_DUMP_DIR
  if(stat("/tmp/vdebug-dump", &buf) == -1 && errno == ENOENT)
    system("mkdir /tmp/vdebug-dump");
#endif
  int fd;

  extrabytes = (sizeof(machine_t) + sizeof(pt_state)) % PAGE_SIZE;
  
  sprintf(arg, "/tmp/vdebug-dump/%s-dump", str);
  fd = open(arg, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
  i = write(fd, M, sizeof(machine_t)); 
  i = write(fd, M->ptState, sizeof(pt_state)); 
  i = write(fd, M, extrabytes);  // waste
  close(fd);
  

  sprintf(arg, "/tmp/vdebug-dump/%s-addr", str);
  fd = open(arg, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
  i = write(fd, &M, sizeof(machine_t *)); 
  close(fd);
  
}
#endif

void 
exit_stub(machine_t *M)
{
  unsigned long i;
  /* This might cause some problems at least collisions due to
     the fixed size of buffers. However, I believe I 
     removed all cases of buffer overflow */

  char str[300];
  char arg[300];
  sprintf(arg, "/proc/%d/exe", getpid());
  i = readlink(arg,str,sizeof(str));
  str[i] = '\0';
  FILE *f;

#ifdef PROFILE_TRANSLATION
  f = fopen("timer", "a");
  unsigned long long start_time = M->ptState->tot_time;
  unsigned long long end_time = read_timer(); 
  M->ptState->tot_time = end_time - start_time;

  fprintf(f, "Process: %s\n", str);
  fprintf(f, "Start Time        = %llu\n", start_time);
  fprintf(f, "End Time          = %llu\n", end_time);  
  fprintf(f, "Translation Time  = %llu\n", M->ptState->trans_time);
  fprintf(f, "Total Time        = %llu\n", M->ptState->tot_time);
  fprintf(f, "Translation Time% = %0.3f\n", 
	  PERC(M->ptState->trans_time, M->ptState->tot_time));
  fprintf(f, "Total bytes       = %lu\n", (M->bbOut - M->bbCache));  
  fprintf(f, "Cycles per byte   = %0.3f\n\n\n", 
	  (float)M->ptState->trans_time / (M->bbOut - M->bbCache));  
  fclose(f);
  return;
#endif

#ifdef PROFILE_BB_CNT
  f = stdout;   
  fprintf(f, "Process         : %s\n", str);
  fprintf(f, "Total count     = %lu\n", M->ptState->bb_cnt);

  return;
#endif  

  
#if defined(PROFILE) || defined(PROFILE_RET_MISS)
  FILE *F = fopen("stats", "a");
#endif

#ifdef PROFILE 
  M->ptState->total_cnt = 
    (unsigned long long) M->ptState->normal_cnt + 
    (unsigned long long) M->ptState->jmp_cond_cnt + 
    (unsigned long long) M->ptState->jmp_dir_cnt +
    (unsigned long long) M->ptState->jmp_indr_cnt +
    (unsigned long long) M->ptState->call_dir_cnt +
    (unsigned long long) M->ptState->call_indr_cnt +
    (unsigned long long) M->ptState->ret_cnt +
    (unsigned long long) M->ptState->ret_Iw_cnt ;
 
  M->ptState->s_total_cnt = 
    (unsigned long long) M->ptState->s_normal_cnt + 
    (unsigned long long) M->ptState->s_jmp_cond_cnt + 
    (unsigned long long) M->ptState->s_jmp_dir_cnt +
    (unsigned long long) M->ptState->s_jmp_indr_cnt +
    (unsigned long long) M->ptState->s_call_dir_cnt +
    (unsigned long long) M->ptState->s_call_indr_cnt +
    (unsigned long long) M->ptState->s_ret_cnt +
    (unsigned long long) M->ptState->s_ret_Iw_cnt ;
 
  fprintf(F, "Process 			: %s\n", str);    
  fprintf(F, "         Static Statistics\n");
  fprintf(F, "Total No. of Instructions 	= %llu  100%\n", 
	  M->ptState->s_total_cnt);
  fprintf(F, "Normal Instructions 		= %lu  %0.3f%\n", 
	  Aux2(M->ptState->s_normal_cnt));
  fprintf(F, "Conditional Jump 		= %lu  %0.3f%\n", 
	  Aux2(M->ptState->s_jmp_cond_cnt)); 
  fprintf(F, "Direct Jump 			= %lu  %0.3f%\n", 
	  Aux2(M->ptState->s_jmp_dir_cnt)); 
  fprintf(F, "Indirect Jump 			= %lu  %0.3f%\n", 
	  Aux2(M->ptState->s_jmp_indr_cnt)); 
  fprintf(F, "Direct Call			= %lu  %0.3f%\n", 
	  Aux2(M->ptState->s_call_dir_cnt)); 
  fprintf(F, "Indirect Call 			= %lu  %0.3f%\n", 
	  Aux2(M->ptState->s_call_indr_cnt)); 
  fprintf(F, "Return 				= %lu  %0.3f%\n", 
	  Aux2(M->ptState->s_ret_cnt)); 
  fprintf(F, "Return IW 			= %lu  %0.3f%\n", 
	  Aux2(M->ptState->s_ret_Iw_cnt)); 

  fprintf(F, "         Dynamic Statistics\n");
  fprintf(F, "Total No. of Instructions 	= %lu  100%\n", 
	  M->ptState->total_cnt);
  fprintf(F, "Normal Instructions 		= %lu  %0.3f%\n", 
	  Aux1(M->ptState->normal_cnt));
  fprintf(F, "Conditional Jump 		= %lu  %0.3f%\n", 
	  Aux1(M->ptState->jmp_cond_cnt)); 
  fprintf(F, "Direct Jump 			= %lu  %0.3f%\n", 
	  Aux1(M->ptState->jmp_dir_cnt)); 
  fprintf(F, "Indirect Jump 			= %lu  %0.3f%\n", 
	  Aux1(M->ptState->jmp_indr_cnt)); 
  fprintf(F, "Direct Call 			= %lu  %0.3f%\n", 
	  Aux1(M->ptState->call_dir_cnt)); 
  fprintf(F, "Indirect Call 			= %lu  %0.3f%\n", 
	  Aux1(M->ptState->call_indr_cnt)); 
  fprintf(F, "Return 				= %lu  %0.3f%\n", 
	  Aux1(M->ptState->ret_cnt)); 
  fprintf(F, "Return IW 			= %lu  %0.3f%\n", 
	  Aux1(M->ptState->ret_Iw_cnt)); 

#endif

#ifdef PROFILE
  fprintf(F, "         Basic-Block Statistics\n");
#endif

#ifdef PROFILE_RET_MISS  
  unsigned long p=0, q=0;
  for(p=0; p<CALL_TABLE_SIZE; p++)
    if(M->call_hash_table[p] != (unsigned long)M->ret_calls_fast_dispatch_bb)
      q++;

#ifdef PROFILE
  fprintf(F, "Returns going to wrong call site = %lu [%.3f%%]\n", 
	  M->ptState->ret_miss_cnt, 
	  PERC(M->ptState->ret_miss_cnt, 
	       M->ptState->ret_cnt + M->ptState->ret_Iw_cnt));
  fprintf(F, "Returns going to fast-dispatch   = %lu [%.3f%%]\n", 
	  M->ptState->ret_ret_miss_cnt, 
	  PERC(M->ptState->ret_ret_miss_cnt, 
	       M->ptState->ret_cnt + M->ptState->ret_Iw_cnt));
  M->ptState->ret_miss_cnt += M->ptState->ret_ret_miss_cnt;
  fprintf(F, "Returns missed Optimization = %lu [%.3f%%]\n", 
	  M->ptState->ret_miss_cnt, 
	  PERC(M->ptState->ret_miss_cnt, 
	       M->ptState->ret_cnt + M->ptState->ret_Iw_cnt));  
#else
  fprintf(F, "Returns going to wrong call site = %lu\n", M->ret_miss_cnt);
  fprintf(F, "Returns going to fast-dispatch   = %lu\n", M->ret_ret_miss_cnt);
  M->ret_miss_cnt += M->ret_ret_miss_cnt;
  fprintf(F, "Returns missed Optimization = %lu\n", M->ret_miss_cnt);  
#endif
  fprintf(F, "%lu / %lu = %0.3f%% buckets changed\n", q, p, ((float)q*100)/((float)p));

  fprintf(F, "BBs with COLD pe = %lu / %lu [%0.3f%%]\n", 
	  M->ptState->cold_cnt, 
	  M->no_of_bbs, 
	  PERC(M->ptState->cold_cnt, M->no_of_bbs));  
  
  fflush(F);
#endif  

#ifdef PROFILE
  fprintf(F, "Total No. of BBs 		= %lu\n", M->no_of_bbs);
  fprintf(F, "No. of BBs with cmp-jmp headers = %lu %0.3f%\n", 
	  M->ptState->hash_nodes_cnt, 
	  PERC(M->ptState->hash_nodes_cnt, M->no_of_bbs));
  fprintf(F, "BBCache: Total size 		= %lu\n", BBCACHE_SIZE);
  fprintf(F, "BBCache: No. of Bytes used 	= %lu %0.3f%\n", (M->bbOut-M->bbCache), 
	  PERC((M->bbOut-M->bbCache), BBCACHE_SIZE));

  /*   bucket_entry *b = (bucket_entry *)M->hash_table;  */
  /*   unsigned long ch_used_cnt = 0; */
  /*   unsigned long max_len = 0; */
  /*   unsigned long len = 0; */
  /*   bb_header *h; */
  /*   for(i=0; i<NBUCKETS; i++, b++) */
  /*     if((unsigned long)(b->rel + &(b->filler[0])) != (unsigned long)(M->slow_dispatch_bb)) */
  /*       { */
  /* 	ch_used_cnt++; */
  /* 	len = 0; */
  /* 	h = (bb_header *)(b->rel + &(b->filler[0])); */
  /* 	while((unsigned long)h != (unsigned long)(M->slow_dispatch_bb)) */
  /* 	  { */
  /* 	    len++; */
  /* 	    h = (bb_header *) ((unsigned long)(h->next_bb) + (unsigned long) (&(h->pop_ebx))); */
  /* 	  } */
  /* 	if(len > max_len) */
  /* 	  max_len = len; */
  /*       } */
		
  /*   fprintf(F, "Total (Code) Hash_Table buckets	= %lu\n", NBUCKETS);  */
  /*   fprintf(F, "Hash Buckets used		= %lu %0.3f%\n", ch_used_cnt, PERC(ch_used_cnt, NBUCKETS));  */
  /*   fprintf(F, "Maximim length of any chain	= %lu\n", max_len); */
  /*   fprintf(F, "\n\n\n");  */
#endif  

#if defined(PROFILE) || defined(PROFILE_RET_MISS)
  fclose(F);
#endif


#ifdef USE_STATIC_DUMP 
  if(M->dump == true) {
    dump_to_file(M, str);
  }
#endif

#ifdef OUTPUT_BB_STAT
  {
    FILE *F = fopen("bbstat", "w");
    for(i=0; i<M->no_of_bbs; i++) {
      fprintf(F, "%lx\n", M->bb_entry_nodes[i].src_bb_eip);
    }
  }
#endif

#ifdef PROFILE_BB_STATS
  {
    unsigned long nrelocbbs=0, ntrelocbbs=0, nactualbbs=0;
    unsigned long nrelocbytes=0, ntrelocbytes=0, nactualbytes = M->bbOut - M->bbCache;
    unsigned long ginstrs=0, tinstrs=0, gbytes = 0, tbytes=0;

    /* Finish off the last BB */
    M->curr_bb_entry->trans_bb_end_eip = (unsigned long) M->bbOut;
    M->curr_bb_entry->flags |= IS_END_OF_TRACE;
    
    /* No of actual BBs */
    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i];
      if(curr->trans_bb_eip != NOT_YET_TRANSLATED)
	nactualbbs++;
    }

    /* BB Analysis */
    unsigned long ilongest_bb = 0;
    unsigned long ilongest_bb_len = 0;
    double iavg_bb_len = 0;
    double ibb_sd = 0;

    unsigned long blongest_bb = 0;
    unsigned long blongest_bb_len = 0;
    unsigned long bavg_bb_len = 0;
    double bbb_sd = 0;

    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i];
      if(curr->nInstr > ilongest_bb_len) {
	ilongest_bb_len = curr->nInstr;
	ilongest_bb = i;
      }
      iavg_bb_len += curr->nInstr;
    }
    iavg_bb_len /= nactualbbs;

    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i];
      ibb_sd += ((double)curr->nInstr - iavg_bb_len) * ((double)curr->nInstr - iavg_bb_len);
    }
    ibb_sd /= nactualbbs;
    ibb_sd = sqrt(ibb_sd);
    
    /* Trace Analysis */
    unsigned long longest_trace_len = 0;
    unsigned long longest_trace_ilen = 0;
    unsigned long longest_trace_start_bb = 0;
    unsigned long longest_trace_bytes = 0;
    double avg_trace = 0;
    double avg_itrace = 0;    
    double avg_trbytes = 0;
    double sd_trace = 0;
    double sd_itrace = 0;
    double sd_trbytes = 0;
    unsigned long ntraces = 0;

    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i];
      curr->flags &= ~MARKED; 
    }
    
    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i]; 
      unsigned long trace_len=0;
      unsigned long trace_instrs=0;
      unsigned long trace_bytes = 0;

      if(curr->flags & MARKED)
	continue;

      ntraces++;
      while((curr != NULL) && (curr->trans_bb_eip != NOT_YET_TRANSLATED)) {
	if(curr->flags & MARKED) {
	  //fprintf(DBG, "@@MARKED in Trace\n");
	}
	else
	  curr->flags |= MARKED;

	trace_len++;
	trace_instrs += curr->nInstr;
	trace_bytes += curr->trans_bb_end_eip - curr->trans_bb_eip;
	curr = curr->trace_next;
      }

      if(trace_len > longest_trace_len) {
	longest_trace_len = trace_len;
	longest_trace_ilen = trace_instrs;
	longest_trace_start_bb = i;
	longest_trace_bytes = trace_bytes;
      }
      avg_trace += trace_len;
      avg_itrace += trace_instrs;
      avg_trbytes += trace_bytes;
    }
    avg_trace /= ntraces;
    avg_itrace /= ntraces;
    avg_trbytes /= ntraces;
    
    /* Now calculate SD */
    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i];
      curr->flags &= ~MARKED; 
    }

    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i]; 
      unsigned long trace_len=0;
      unsigned long trace_bytes = 0;

      if(curr->flags & MARKED)
	continue;

      while((curr != NULL) && (curr->trans_bb_eip != NOT_YET_TRANSLATED)) {
	if(curr->flags & MARKED) {
	  //fprintf(DBG, "##MARKED in Trace\n");
	}
	else
	  curr->flags |= MARKED;

	trace_len++;
	trace_bytes += curr->trans_bb_end_eip - curr->trans_bb_eip;
	curr = curr->trace_next;
      }
      sd_trace += (trace_len - avg_trace) * (trace_len - avg_trace);
      sd_trbytes += (trace_bytes - avg_trbytes) * (trace_bytes - avg_trbytes);
    }

    sd_trace /= ntraces; sd_trace = sqrt(sd_trace);
    sd_trbytes /= ntraces; sd_trbytes = sqrt(sd_trbytes);
    
    /* No. of BBs needing Relocation */
    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i];
      if((curr->trans_bb_eip != NOT_YET_TRANSLATED) && (curr->flags & NEEDS_RELOC)) {
	nrelocbbs++;
	nrelocbytes += curr->trans_bb_end_eip - curr->trans_bb_eip;
      }
    }

    /* Propagate the RELOC Flag Forward and backward */
    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i];
      bool reloc = false;
      while((curr != NULL) && (curr->trans_bb_eip != NOT_YET_TRANSLATED)) {
	if(curr->flags & NEEDS_RELOC) {
	  reloc = true;
	  break;
	}
	curr = curr->trace_next;
      }
      if(reloc) {
	curr = &M->bb_entry_nodes[i];
	while((curr != NULL) && (curr->trans_bb_eip != NOT_YET_TRANSLATED)) {
	  curr->flags |= NEEDS_RELOC;
	  curr = curr->trace_next;	  
	}
      }
    }

    /* Recalculate No. of BBs needing Relocation after propagation of RELOC Flag*/
    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *curr = &M->bb_entry_nodes[i];
      if((curr->trans_bb_eip != NOT_YET_TRANSLATED) && (curr->flags & NEEDS_RELOC)) {
	ntrelocbbs++;
	ntrelocbytes += curr->trans_bb_end_eip - curr->trans_bb_eip;
      }
    }
    FILE *F = fopen("bbdetails", "w");
    fprintf(F, "Process: %s\n\n\n", str);

    unsigned long neip = M->next_eip;
#ifdef STATIC_PASS
    unsigned long mneip = M->mem_next_eip;
#endif
    decode_t ds;
    
    /*WARNING: Sieve is the first thing in the BBcache as of now */
    unsigned long sieve_bytes = M->bb_entry_nodes[0].trans_bb_eip - (unsigned long)M->bbCache;
    
    for(i=0; i<M->no_of_bbs; i++) {
      bb_entry *entry = &M->bb_entry_nodes[i];
      fprintf(F, "#ID = %lu\n", i);
      if(entry->trans_bb_eip == NOT_YET_TRANSLATED)
	fprintf(F, "Not yet translated\n\n");
      else {
	
	if(entry->sieve_header != NULL)
	  fprintf(F, "HAS_SIEVE  ");
	if (entry->flags & IS_HAND_CONSTRUCTED)
	  fprintf(F, "HAND_CONSTRUCTED  ");
	if (entry->flags & IS_START_OF_TRACE)
	  fprintf(F, "STARTS_TRACE  ");
	if (entry->flags & IS_END_OF_TRACE)
	  fprintf(F, "ENDS_TRACE  ");
	if (entry->flags & NEEDS_RELOC)
	  fprintf(F, "NEEDS_RELOCATION  ");
	fprintf(F, "\n");

	fprintf(F, "Proc-Index   = 0x%08lx  ", entry->proc_entry);
	if(entry->trace_next != NULL)
	  fprintf(F, "Trace_Next = %lu\n", ((((unsigned long)entry->trace_next) 
					     -((unsigned long)M->bb_entry_nodes))
					    /((unsigned long)sizeof(bb_entry))));
	else
	  fprintf(F, "\n");
 
	fprintf(F, "-----------------------------------------------------------\n\n");

	M->next_eip = entry->src_bb_eip;
#ifdef STATIC_PASS
	if(update_mem_next_eip(M))
	  fprintf(F, "OUT");
#endif
	int j;
	fprintf(F, "Guest Start  = 0x%08lx, End = 0x%08lx, Size  = %lu\n",
		entry->src_bb_eip, entry->src_bb_end_eip,
		entry->src_bb_end_eip - entry->src_bb_eip);
	
	for(j=0; j<entry->nInstr; j++) {
	  do_decode(M, &ds);
#ifdef PROFILE_BB_STATS_DISASM
	  do_disasm(&ds, F);
#endif	
	}
	ginstrs += entry->nInstr;
	gbytes += entry->src_bb_end_eip - entry->src_bb_eip;
	fprintf(F, "No. Guest Instructions = %lu\n", entry->nInstr);
	fprintf(F, "-----------------------------------------------------------\n\n");

	M->next_eip = entry->trans_bb_eip;
	unsigned long trans_instrs = 0;
	fprintf(F, "Trans Start  = 0x%08lx, End = 0x%08lx, Size  = %lu\n", 
		entry->trans_bb_eip, entry->trans_bb_end_eip,
		entry->trans_bb_end_eip - entry->trans_bb_eip);
	while(M->next_eip < entry->trans_bb_end_eip) {
	  do_decode(M, &ds);
#ifdef PROFILE_BB_STATS_DISASM
	  do_disasm(&ds, F);
#endif	
	  trans_instrs++;
	}	  
	tinstrs += trans_instrs;
	tbytes += entry->trans_bb_end_eip - entry->trans_bb_eip;
	fprintf(F, "No. Translated Instructions = %lu\n", trans_instrs);
	fprintf(F, "===========================================================\n\n");
      }
    }
    
    M->next_eip = neip; /* Restore the old value */
#ifdef STATIC_PASS
    M->mem_next_eip = mneip;
#endif

    /* Print the Summary */
    fprintf(F, "No: of BBs                         = %lu\n", M->no_of_bbs);
    fprintf(F, "No: of Translated BBs              = %lu\n", nactualbbs);
    fprintf(F, "Total No. of Guest Instrs          = %lu\n", ginstrs);
    fprintf(F, "Total No. of Trans Instrs          = %lu\n", tinstrs);
    fprintf(F, "Total No. of Guest Instr Bytes     = %lu\n", gbytes);
    fprintf(F, "Total No. of Trans Instrs Bytes    = %lu\n", tbytes);
    fprintf(F, "No. of Bytes used in BBCache       = %lu  [%0.3f%% of BBCache]\n", 
	    nactualbytes, PERC(nactualbytes, BBCACHE_SIZE));
    fprintf(F, "No. of Bytes in sieve-hash-table   = %lu\n", sieve_bytes);
    fprintf(F, "No. of Bytes ignoring sieve table  = %lu  [%0.3f%% of BBCache]\n", 
	    nactualbytes-sieve_bytes, PERC(nactualbytes-sieve_bytes, BBCACHE_SIZE));
    fprintf(F, "No: of actual RELOC BBs            = %lu\n", nrelocbbs);
    fprintf(F, "No: of RELOC BBs due to trace      = %lu\n", ntrelocbbs);
    fprintf(F, "No: of actual RELOC BB bytes       = %lu\n", nrelocbytes);
    fprintf(F, "No: of RELOC BB bytes due to trace = %lu\n", ntrelocbytes);
    fprintf(F, "% RELOC BBs                        = %0.3f\n", PERC(nrelocbbs, nactualbbs));
    fprintf(F, "% Trace RELOC BBs                  = %0.3f\n", PERC(ntrelocbbs, nactualbbs));
    fprintf(F, "% RELOC BB bytes                   = %0.3f\n", PERC(nrelocbytes, tbytes));
    fprintf(F, "% Trace RELOC BB bytes             = %0.3f\n", PERC(ntrelocbytes, tbytes));
    fprintf(F, "LONGEST BB                         = %lu instrs [bb# %lu]\n",
	    ilongest_bb_len, ilongest_bb);
    fprintf(F, "Average Length of BBs              = %0.3lf instrs\n", iavg_bb_len);
    fprintf(F, "SD of BB Length                    = %0.3lf instrs\n", ibb_sd);
    fprintf(F, "LONGEST Trace Len                  = %lu bbs    [Starting at bb# %lu]\n", 
	    longest_trace_len, longest_trace_start_bb);
    fprintf(F, "LONGEST Trace Len (instrs)         = %lu\n", longest_trace_ilen);
    fprintf(F, "LONGEST Trace (above) bytes        = %lu\n",longest_trace_bytes);
    fprintf(F, "Average Trace Length               = %0.3lf bbs\n", avg_trace);
    fprintf(F, "Average Trace Length               = %0.3lf instrs\n", avg_itrace);
    fprintf(F, "Average Trace Bytes                = %0.3lf \n", avg_trbytes);
    fprintf(F, "SD Trace Length                    = %0.3lf bbs\n", sd_trace);
    fprintf(F, "SD Trace Bytes                     = %0.3lf \n", sd_trbytes);
    fprintf(F, "No. of Traces                      = %lu\n", ntraces);

    fclose(F); 

    printf("No: of BBs                         = %lu\n", M->no_of_bbs); 
    printf("No: of Translated BBs              = %lu\n", nactualbbs);
    printf("Total No. of Guest Instrs          = %lu\n", ginstrs);
    printf("Total No. of Trans Instrs          = %lu\n", tinstrs);
    printf("Total No. of Guest Instr Bytes     = %lu\n", gbytes);
    printf("Total No. of Trans Instrs Bytes    = %lu\n", tbytes);
    printf("No. of Bytes used in BBCache       = %lu  [%0.3f%% of BBCache]\n", 
	   nactualbytes, PERC(nactualbytes, BBCACHE_SIZE));
    printf("No. of Bytes in sieve-hash-table   = %lu\n", sieve_bytes);
    printf("No. of Bytes ignoring sieve table  = %lu  [%0.3f%% of BBCache]\n", 
	   nactualbytes-sieve_bytes, PERC(nactualbytes-sieve_bytes, BBCACHE_SIZE));
    printf("No: of actual RELOC BBs            = %lu\n", nrelocbbs);
    printf("No: of RELOC BBs due to trace      = %lu\n", ntrelocbbs);
    printf("No: of actual RELOC BB bytes       = %lu\n", nrelocbytes);
    printf("No: of RELOC BB bytes due to trace = %lu\n", ntrelocbytes);
    printf("% RELOC BBs                        = %0.3f\n", PERC(nrelocbbs, nactualbbs));
    printf("% Trace RELOC BBs                  = %0.3f\n", PERC(ntrelocbbs, nactualbbs));
    printf("% RELOC BB bytes                   = %0.3f\n", PERC(nrelocbytes, tbytes));
    printf("% Trace RELOC BB bytes             = %0.3f\n", PERC(ntrelocbytes, tbytes));
    printf("LONGEST BB                         = %lu instrs [bb# %lu]\n",
	   ilongest_bb_len, ilongest_bb);
    printf("Average Length of BBs              = %0.3lf instrs\n", iavg_bb_len);
    printf("SD of BB Length                    = %0.3lf instrs\n", ibb_sd);
    printf("LONGEST Trace Len                  = %lu bbs    [Starting at bb# %lu]\n", 
	   longest_trace_len, longest_trace_start_bb);
    printf("LONGEST Trace Len (instrs)         = %lu\n", longest_trace_ilen);
    printf("LONGEST Trace (above) bytes        = %lu\n",longest_trace_bytes);
    printf("Average Trace Length               = %0.3lf bbs\n", avg_trace);
    printf("Average Trace Length               = %0.3lf instrs\n", avg_itrace);
    printf("Average Trace Bytes                = %0.3lf \n", avg_trbytes);
    printf("SD Trace Length                    = %0.3lf bbs\n", sd_trace);
    printf("SD Trace Bytes                     = %0.3lf \n", sd_trbytes);
    printf("No. of Traces                      = %lu\n", ntraces);

    /* Dump longest Trace */
    neip = M->next_eip;
    unsigned long max_tr_instrs = 0;
#ifdef STATIC_PASS
    mneip = M->mem_next_eip;
#endif
    F = fopen("longest_trace", "w");
    fprintf(F, "Process: %s\n\n\n", str);
    fprintf(F, "LONGEST Trace Len                  = %lu   [Starting at bb# %lu]\n", 
	    longest_trace_len, longest_trace_start_bb);
    bb_entry *curr = &M->bb_entry_nodes[longest_trace_start_bb];
    unsigned long j;
    while((curr != NULL) && (curr->trans_bb_eip != NOT_YET_TRANSLATED)) {
      fprintf(F, "Guest Start  = 0x%08lx, End = 0x%08lx, Size  = %lu, nInstr = %lu\n",
	      curr->src_bb_eip, curr->src_bb_end_eip,
	      curr->src_bb_end_eip - curr->src_bb_eip,
	      curr->nInstr);
      M->next_eip = curr->src_bb_eip;
#ifdef STATIC_PASS
      if(update_mem_next_eip(M))
	fprintf(F, "OUT");
#endif
      for(j=0; j<curr->nInstr; j++) {
	do_decode(M, &ds);
	do_disasm(&ds, F);
      }
	max_tr_instrs += curr->nInstr;
	curr = curr->trace_next;
	fprintf(F, "\n\n");
    }
    M->next_eip = neip;
#ifdef STATIC_PASS
    M->mem_next_eip = mneip;
#endif
    fprintf(F, "Total No of Instructions = %lu", max_tr_instrs);
    fclose(F);
  }    
#endif
  //if(debug_flags) {
  //  DBG = fopen("vdbg.dbg", "w");
  //}
}


#ifndef STATIC_PASS
#if (defined(PROFILE)          ||		\
     defined(PROFILE_RET_MISS) ||		\
     defined(PROFILE_BB_CNT)   ||		\
     defined(USE_STATIC_DUMP)  ||		\
     defined(PROFILE_BB_STATS) ||               \
     defined(PROFILE_TRANSLATION))

#define EXIT_HANDLING_NECESSARY
#endif
#endif

#ifdef THREADED_XLATE 

machine_t* init_thread_trans(unsigned long program_start);
  
 void 
  bb_setup_child_startup(machine_t *M) //[len = 22]
{
  // This code is similar to Userentry.s
  // It runs on the clild stack, cals init_thread_trans, 
  // which allocates and initialized the bbCache for this thread.
  // Then jump into startup-slow-dispatch-bb in that bbCache

  // pushf already performed by caller.
  
  // pusha [len 1b]
  bb_emit_byte(M, 0x60u);
  
  // push $M->next_eip [len 5b]
  bb_emit_byte(M, 0x68u);
  bb_emit_w32(M, M->next_eip);

  // call init_therad_trans [len 5b]
  bb_emit_call(M, (unsigned char *)(&init_thread_trans)); 

  // esp += 4; leal 4(%esp), %esp [len 7b]
  bb_emit_byte(M, 0x8du);
  bb_emit_byte(M, 0xA4u); /* 10 100 100 */
  bb_emit_byte(M, 0x24u); /* 00 100 100 */
  bb_emit_w32(M, 0x4u);

  // mov (%eax), %eax [len 2b]
  bb_emit_byte(M, 0x8bu);  /* 8b /r */
  bb_emit_byte(M, 0x00u);  /* 00 000 000 */

  // jmp *%eax [len 2b]
  bb_emit_byte(M, 0xffu);  // ff /4
  bb_emit_byte(M, 0xe0u);  // 11 100 000
}

void
exit_unmapper(machine_t *M)
{  
  if(!M->ismmaped)
    return;
  
  DEBUG(thread_exit) {
    fprintf(DBG, "UNMAPPING -- exit()\n");
    fflush(DBG);
  }
  
  munmap(M, sizeof(machine_t));
  
  
  asm volatile ("int $0x80\n\t"
		:
		: "a" (__NR_exit)
		);  
  /* Not reached */
}
 
 void
   execve_unmapper(machine_t *M, fixregs_t regs)
 {
  unsigned long retVal;
  unsigned long next_eip = M->next_eip;
  unsigned char *Maddr = (unsigned char *)&M;  
  unsigned char *retAddr = Maddr - sizeof(void *);
  unsigned char *pastM = Maddr + sizeof(void *);
  
  DEBUG(thread_exit) {
    fprintf(DBG, "UNMAPPING -- execve()\n");
    fflush(DBG);
  }  
  munmap(M, sizeof(machine_t));    

  asm volatile ("pusha\n\t"
		"mov %2, %%ebx\n\t"
		"int $0x80\n\t"
		: "=a"  (retVal)
		: "0" (__NR_execve), "m" (regs.ebx),
  		  "c" (regs.ecx), "d" (regs.edx),
  		  "S" (regs.esi), "D" (regs.edi)
		);
  
  
  //panic("execve() failed\n");
  /* If we reach here, execve() must have failed */
  //assert(retVal != 0);
  
  // Now that we are here, execve() failed for whatever reason
  // but we *may* have removed the M state; now too late to
  // find out. So, start anew anyway ... 
  
  asm volatile ("movl %%eax, %0\n\t"
		"push %1\n\t"
		"call init_thread_trans\n\t"
		"leal 4(%%esp), %%esp\n\t"
		"mov (%%eax), %%eax\n\t"
		"movl %%eax, %2\n\t"  
		"popa\n\t"
		"mov %3, %%esp\n\t"
		"retl\n\t"
		:
		: "m" (regs.eax), "m" (next_eip), "m" (M), "m" (Maddr)  
		);
  panic("In function execve_unmapper, \"Unreachable\" code reached\n");  
}
 
#endif /* THREADED_XLATE */
 
 static void 
   maskAllSignals(machine_t *M)
  {
  //fprintf(stderr, "Masked\n");
  sigprocmask(SIG_SETMASK, &allSignals, &M->syscall_sigset);  
}

static void 
restoreSignals(machine_t *M)
{
  //fprintf(stderr, "UnMasked\n");
  sigprocmask(SIG_SETMASK, &M->syscall_sigset, NULL);  
}

#ifdef SIGNALS
void
signal_syscall_pre(machine_t *M, fixregs_t regs)
{
  // I am yet to see this system call used.
  size_t signo = regs.ebx;
  k_sigaction sa;

  //maskAllSignals(M);
  sigprocmask(SIG_SETMASK, &allSignals, &M->syscall_sigset);

  if(regs.ecx == (unsigned long)NULL) {
    M->ptState->guest_saPtr = NULL;
    DEBUG(signal_registry) {
      fprintf(DBG, "PRE: SigNo = %ld, Signal %s {sa=NULL}\n", 
	      signo, sig_names[signo]);
      fflush(DBG);
    }
    return;
  }

  M->ptState->guest_saPtr = &M->ptState->sa_table[signo].aux;
  sa.sa_handler = (void *)regs.ecx;
  sa.sa_flags = SA_ONESHOT | SA_NOMASK;    
  
  M->ptState->curr_signo = signo;
  
  if((sa.sa_handler != NULL) &&
     (sa.sa_handler != SIG_IGN) && 
     (sa.sa_handler != SIG_DFL)) {    
    regs.ecx = (unsigned long) &masterSigHandler;
  }   
  
  memcpy(&M->ptState->sa_table[signo].aux, &sa, sizeof(k_sigaction));
  *(M->ptState->guestOld_shPtr) = (sighandler_t) regs.edx;
  M->ptState->curr_signo = signo;
  
  DEBUG(signal_registry) {
    fprintf(DBG, "Pre-SIGNAL SigNo = %ld, Signal %s\n", signo, sig_names[signo]);
    fflush(DBG);
  }
}

void
signal_syscall_post(machine_t *M, fixregs_t regs)
{
  // I am yet to see this system call used.
  int saved_errno = errno;
  size_t signo = M->ptState->curr_signo;
  int retVal = regs.eax;
  
  if(retVal == 0) {
    if(M->ptState->guestOld_shPtr != NULL)
      *M->ptState->guestOld_shPtr = 
	(sighandler_t) M->ptState->sa_table[signo].old.sa_handler;
    
    if(M->ptState->guest_saPtr != NULL) {      
      memcpy(&M->ptState->sa_table[signo].old, &M->ptState->sa_table[signo].new, 
	     sizeof(k_sigaction));
      memcpy(&M->ptState->sa_table[signo].new, &M->ptState->sa_table[signo].aux, 
	     sizeof(k_sigaction));
    }
  }
  
  DEBUG(signal_registry) {
    fprintf(DBG, "POST: SigNo = %ld, Signal %s : fn = %lx\n", 
	    signo, sig_names[signo], 
	    M->ptState->sa_table[signo].new.sa_handler);
    fflush(DBG);
  }
    
  errno = saved_errno;
  sigprocmask(SIG_SETMASK, &M->syscall_sigset, NULL);
  //restoreSignals(M);
}


/* Parameters to sigaction: 
   %eax: syscall no.
   %ebx: signal no.
   %ecx: Curent Sigaction structure
   %edx: Old Sigaction structure */

void
sigaction_syscall_pre(machine_t *M, fixregs_t regs)
{
  size_t signo = regs.ebx;
  k_sigaction *sa = (k_sigaction *)regs.ecx;  
  k_sigaction *old_sa = (k_sigaction *)regs.edx;  

  //maskAllSignals(M);
  sigprocmask(SIG_SETMASK, &allSignals, &M->syscall_sigset);

  M->ptState->guestOld_saPtr = old_sa;
  M->ptState->curr_signo = signo;

  if(sa == NULL) {
    M->ptState->guest_saPtr = NULL;
    DEBUG(signal_registry) {
      fprintf(DBG, "PRE: SigNo = %ld, Signal %s {sa=NULL}\n", 
	      signo, sig_names[signo]);
      fflush(DBG);
    }
    return;
  }  
  
  M->ptState->guest_saPtr = sa;
  memcpy(&M->ptState->sa_table[signo].aux, sa, sizeof(k_sigaction));
  
  if((sa->sa_handler != NULL) &&
     (sa->sa_handler != SIG_IGN) && 
     (sa->sa_handler != SIG_DFL)) {    
    sa->sa_handler = (void (*)(int)) &masterSigHandler;
  }
   
  DEBUG(signal_registry) {
    fprintf(DBG, "PRE: SigNo = %ld, Signal %s %s\n", 
	    signo, sig_names[signo], 
	    (sa->sa_flags & SA_ONESHOT)?" {oneshot}":"");
    fflush(DBG);
  }  
}

void
sigaction_syscall_post(machine_t *M, fixregs_t regs)
{
  int saved_errno = errno;
  size_t signo = M->ptState->curr_signo;
  int retVal = regs.eax;
  
  if(retVal == 0) {
    if(M->ptState->guestOld_saPtr != NULL) {
      memcpy(M->ptState->guestOld_saPtr, &M->ptState->sa_table[signo].old,
	     sizeof(k_sigaction));      
    }

    if(M->ptState->guest_saPtr != NULL) {
      memcpy(&M->ptState->sa_table[signo].old, &M->ptState->sa_table[signo].new, 
	     sizeof(k_sigaction));
      memcpy(&M->ptState->sa_table[signo].new, &M->ptState->sa_table[signo].aux, 
	     sizeof(k_sigaction));
    }
  }

  DEBUG(signal_registry) {
    fprintf(DBG, "POST: SigNo = %ld, Signal %s : fn = %lx\n", 
	    signo, sig_names[signo], 
	    M->ptState->sa_table[signo].new.sa_handler);
    fflush(DBG);
  }

  errno = saved_errno;
  
  //restoreSignals(M);
  sigprocmask(SIG_SETMASK, &M->syscall_sigset, NULL);
}

/* Parameters:
   ebx: new stack
   ecx: old stack */

void
sigaltstack_syscall_pre(machine_t *M, fixregs_t regs)
{
}

void
sigaltstack_syscall_post(machine_t *M, fixregs_t regs)
{
}
 
//static register unsigned long curr_esp asm("esp") __attribute_used__;

void
sigreturn_syscall(machine_t *M, fixregs_t regs)
{
  unsigned long esp = (unsigned long)&M;
  esp += sizeof(machine_t *) + sizeof(pushaf_t);
  
  if(!M->ismmaped)
    panic("Signal's Mstate is NOT mmaped\n");
  
  DEBUG(sig_init_exit) {
    fprintf(DBG, "UNMAPPING -- sigreturn\n");
    fflush(DBG);
  }
  
  munmap(M, sizeof(machine_t));  
  
  asm volatile ("mov %1, %%esp\n\t"
		"int $0x80\n\t"
		:
		: "a" (__NR_sigreturn),
		  "m" (esp)
		);
  panic("sigreturn: \"unreachable\' code reached\n");  
}

void
rt_sigreturn_syscall(machine_t *M, fixregs_t regs)
{
  unsigned long esp = (unsigned long)&M;
  esp += sizeof(machine_t *) + sizeof(pushaf_t);
  
  if(!M->ismmaped)
    panic("Signal's Mstate is NOT mmaped\n");
  
  DEBUG(sig_init_exit) {
    fprintf(DBG, "UNMAPPING -- sigreturn\n");
    fflush(DBG);
  }
  
  munmap(M, sizeof(machine_t));
  
  asm volatile ("mov %1, %%esp\n\t"
		"int $0x80\n\t"
		:
		: "a" (__NR_rt_sigreturn),
		  "m" (esp)		  
		);  
  panic("rt_sigreturn: \"unreachable\' code reached\n");
}


#endif /* SIGNALS */


void 
emit_pusha_pushM_call(machine_t *M, void *proc) //[len 19b]
{
  // pusha [len 1b]
  bb_emit_byte(M, 0x60u);

  // Push M [len 5b]
  bb_emit_byte(M, 0x68u);
  bb_emit_w32(M, (unsigned long) M);
  
  // call stub [len 5b]
  bb_emit_call(M, (unsigned char *) proc);
  
  // esp += 4; leal 4(%esp), %esp [len 7b]
  bb_emit_byte(M, 0x8du);
  bb_emit_byte(M, 0xA4u); /* 10 100 100 */
  bb_emit_byte(M, 0x24u); /* 00 100 100 */
  bb_emit_w32(M, 0x4u);
  
  // popa [len 1b]
  bb_emit_byte(M, 0x61u);  
}


void
syscall_stub(machine_t *M, fixregs_t regs)
{
  fprintf(DBG, "syscall# = %ld, syscall = %s\n", 
	  regs.eax, syscall_names[regs.eax]);
  fprintf(DBG, "eax = %lx\n", regs.eax);
  fprintf(DBG, "ebx = %lx\n", regs.ebx);
  fprintf(DBG, "ecx = %lx\n", regs.ecx);
  fprintf(DBG, "edx = %lx\n", regs.edx);
  fprintf(DBG, "esi = %lx\n", regs.esi);
  fprintf(DBG, "edi = %lx\n", regs.edi);
  fprintf(DBG, "M  mapped? = %s\n", 
	  ((M->ismmaped) ? "true": "false"));        
  
  fprintf(DBG, " - - - - - - - - - - - - - - - - - - - - -\n\n");
  fflush(DBG);
}

void
emit_syscall_handler(machine_t *M, unsigned long which_syscall)
{  
  unsigned char b[2] = {0, 0};
  if(which_syscall == EMIT_INT80_SYSCALL) {
    // int 0x80
    b[0] = 0xCDu;
    b[1] = 0x80u;
  }
  else {    
    // sysenter
    b[0] = 0x0fu;
    b[1] = 0x34u;
  }

#if (!defined(EXIT_HANDLING_NECESSARY) &&	\
     !defined(THREADED_XLATE)         &&	\
     !defined(SIGNALS))				
  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  return;
#else
  
  // Pushf
  bb_emit_byte (M, 0x9Cu);
  
  DEBUG(show_all_syscalls)
    emit_pusha_pushM_call(M, ((void *)syscall_stub));
  
#ifdef THREADED_XLATE
#define CL_SKIP 56u
#define EXIT_SKIP 34u
#define EXEC_SKIP 56u
#else
#define CL_SKIP 0u
#define EXEC_SKIP 0u
#define EXIT_SKIP 0u
#endif   


#ifdef SIGNALS
#define RT_SA_SKIP 53u
#define SA_SKIP 53u
#define SIGNAL_SKIP 53u
#define SRET_SKIP 34u
#define RT_SRET_SKIP 34u
#else
#define RT_SA_SKIP 0u
#define SA_SKIP 0u
#define SIGNAL_SKIP 0u
#define SRET_SKIP 0u
#define RT_SRET_SKIP 0u
#endif
    
#ifdef EXIT_HANDLING_NECESSARY
  /***********************************************************/
  // exit_group  0xfc [len 34b]
  /***********************************************************/
  //1f:
  //cmp %eax, $__NR_exit_group [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_exit_group);

  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 27u);

  emit_pusha_pushM_call(M, ((void *)exit_stub)); //[len 19b]

  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  
  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, RT_SRET_SKIP + SRET_SKIP + RT_SA_SKIP + SA_SKIP + 
	      SIGNAL_SKIP + EXIT_SKIP + EXEC_SKIP + CL_SKIP + 3u);
#endif /* EXIT_HANDLING_NECESSARY */


#ifdef SIGNALS

  /***********************************************************/
  // sigaction ==   [len 34b]
  /***********************************************************/
  //cmp %eax, $__NR_sigreturn [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_sigreturn);

  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 27u);

  emit_pusha_pushM_call(M, ((void *)sigreturn_syscall)); 
  //[len 19b]

  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  
  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, SRET_SKIP + RT_SA_SKIP + SA_SKIP + SIGNAL_SKIP + 
	      EXIT_SKIP + EXEC_SKIP + CL_SKIP + 3u);


  /***********************************************************/
  // sigaction ==   [len 34b]
  /***********************************************************/
  //cmp %eax, $__NR_sigreturn [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_sigreturn);

  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 27u);

  emit_pusha_pushM_call(M, ((void *)sigreturn_syscall)); 
  //[len 19b]

  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  
  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, RT_SA_SKIP + SA_SKIP + SIGNAL_SKIP + EXIT_SKIP + 
	      EXEC_SKIP + CL_SKIP + 3u);


  /***********************************************************/
  // rt_sigaction ==   [len 53b]
  /***********************************************************/
  //cmp %eax, $__NR_rt_sigaction [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_rt_sigaction);

  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 46u);

  emit_pusha_pushM_call(M, ((void *)sigaction_syscall_pre)); 
  //[len 19b]

  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);

  emit_pusha_pushM_call(M, ((void *)sigaction_syscall_post)); 
  //[len 19b]
  
  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, SA_SKIP + SIGNAL_SKIP + EXIT_SKIP + EXEC_SKIP + 
	      CL_SKIP + 3u);


  /***********************************************************/
  // sigaction ==   [len 53b]
  /***********************************************************/
  //cmp %eax, $__NR_sigaction [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_sigaction);

  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 46u);

  emit_pusha_pushM_call(M, ((void *)sigaction_syscall_pre)); 
  //[len 19b]

  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  
  emit_pusha_pushM_call(M, ((void *)sigaction_syscall_post)); 
  //[len 19b]

  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, SIGNAL_SKIP + EXIT_SKIP + EXEC_SKIP + CL_SKIP + 3u);

  /***********************************************************/
  // signal ==   [len 53b]
  /***********************************************************/
  //cmp %eax, $__NR_signal [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_signal);

  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 46u);

  emit_pusha_pushM_call(M, ((void *)signal_syscall_pre)); 
  //[len 19b]

  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  
  emit_pusha_pushM_call(M, ((void *)signal_syscall_post)); 
  //[len 19b]

  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, EXIT_SKIP + EXEC_SKIP + CL_SKIP + 3u);


#if 0
  /***********************************************************/
  // sigaltstack ==   [len 53b]
  /***********************************************************/
  //cmp %eax, $__NR_sigaltstack [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_sigaltstack);

  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 46u);

  emit_pusha_pushM_call(M, ((void *)sigaltstack_syscall_pre)); 
  //[len 19b]

  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  
  emit_pusha_pushM_call(M, ((void *)sigaltstack_syscall_post)); 
  //[len 19b]

  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, EXIT_SKIP + EXEC_SKIP + CL_SKIP + 3u);
#endif

#endif /* SIGNALS */


#ifdef THREADED_XLATE
  /***********************************************************/
  // exit ==  0x01 [len 34b]
  /***********************************************************/
  /* **** MUST FIX EXIT_SKIP IF THE SIZE OF THIS BLOCK CHANGES **** */

  //cmp %eax, $__NR_exit [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_exit);

  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 27u);

  emit_pusha_pushM_call(M, ((void *) exit_unmapper)); //[len 19b]
  
  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);    

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  
  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, CL_SKIP + EXEC_SKIP + 3u);

  /***********************************************************/
  // execve ==  0x0B [len 56b]
  /***********************************************************/
  /* **** MUST FIX EXEC_SKIP IF THE SIZE OF THIS BLOCK CHANGES **** */
  
  //cmp %eax, $__NR_execve [len 5b] 
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_execve);
    
  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 49u);

  //[len 19b]
  if(M->ismmaped) {
    emit_pusha_pushM_call(M, ((void *) execve_unmapper));
  }
  else {
    // jmp out [len 5b]
    bb_emit_byte(M, 0xe9u);
    bb_emit_w32(M, CL_SKIP + 30u + 14);    

    int i=0;
    for(i=0; i < 14; i++)
      bb_emit_byte (M, 0x90u); // nop
  }
  
  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);    
  
  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);
  
  // If execve() succeeds, we will NOT REACH HERE.
  // Now that we are here, execve() failed for whatever reason
  // but we *may* have removed the M state; now too late to
  // find out. So, start anew anyway ... 
  // If this was that thread with a static mmap() -- too bad?
  
  bb_setup_child_startup(M); //[len 22b]

  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, CL_SKIP + 3u);

  /***********************************************************/
  // clone 0x78 == 120 [len 56b]
  /***********************************************************/
  /* **** MUST FIX CL_SKIP IF THE SIZE OF THIS BLOCK CHANGES **** */

  /*   //1f: */
  //cmp %eax, $__NR_clone [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, __NR_clone);
  
  //jne 1f: [len 2b]
  bb_emit_byte(M, 0x75u);
  bb_emit_byte(M, 49u);
 
  // push %ebx [len 1b]
  bb_emit_byte(M, 0x53u);

  // and $CLONE_VM, %ebx [len 6b]  
  bb_emit_byte(M, 0x81u); // 81 / 4
  bb_emit_byte(M, 0xE3u); // 11 100 011
  bb_emit_w32(M, CLONE_VM);
  
  // pop %ebx [len 1b]
  bb_emit_byte(M, 0x5bu);
  
  // jz normal (other-syscalls)  [len 2b]
  bb_emit_byte(M, 0x74u);
  bb_emit_byte(M, 39u);  
  
  // mask all signals [len 19b]
  //emit_pusha_pushM_call(M, (void *)maskAllSignals);
  
  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);

  // Pushf [len 1b]
  bb_emit_byte (M, 0x9Cu);

  //cmp %eax, $0x0u [len 5b]
  bb_emit_byte(M, 0x3Du);
  bb_emit_w32(M, 0x00u);
  
  // jz parent  [len 2b]
  bb_emit_byte(M, 0x74u);
  bb_emit_byte(M, 22u);  
  
  bb_setup_child_startup(M); //[len 22b]
  
  // parent:
  // Restore masked signals //[len 19b]
  //emit_pusha_pushM_call(M, (void *)restoreSignals);
  
  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);  

  // jmp out [len 5b]
  bb_emit_byte(M, 0xe9u);
  bb_emit_w32(M, 3u);
  
#endif /* THREADED_XLATE */
  
  /***********************************************************/
  // All other syscalls [len 3b]
  /***********************************************************/
  // popf [len 1b]
  bb_emit_byte (M, 0x9Du);
  
  // the sys_call [len 2b]
  bb_emit_byte(M, b[0]);
  bb_emit_byte(M, b[1]);

  /***********************************************************/
  // out
  /***********************************************************/
  //1f:  
#endif /* No threading or exit Handling */
  
}

bool
emit_int(machine_t *M, decode_t *d)
{
  DEBUG(emits)
    fprintf(DBG, "Int ");

#ifdef PROFILE
  // Calls emit_normal.  
#endif

  unsigned i;
  
  if(d->instr[1] == 0x80u)  {   	
    emit_syscall_handler(M, EMIT_INT80_SYSCALL);
  }
  else {
    emit_normal(M, d);
  }
  return false;
}

bool
emit_sysenter(machine_t *M, decode_t *d)
{
  DEBUG(emits)
    fprintf(DBG, "sysenter ");
  
#ifdef PROFILE
  // Calls emit_normal.  
#endif
  
  emit_syscall_handler(M, EMIT_SYSENTER_SYSCALL);
  
  return false;
}


/*********************************************************************
Control-flow instructions I - Jumps, Calls, Returns, etc.
*********************************************************************/

bool
emit_jcond(machine_t *M, decode_t *d)
{
  unsigned char cond;
  unsigned long jmp_destn;
  
  DEBUG(emits)
    fprintf(DBG, "%lu: Jcond\n", M->nTrInstr);
  
#ifdef PROFILE
  M->ptState->s_jmp_cond_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->jmp_cond_cnt));
#endif
  
  /* M->next_eip points to following instruction. Destination of
   * jump is M->next_eip + d->immediate.
   *
   * Solve this by setting M->next_eip to the target address and
   * then performing a conditional jump (in fact, the *same*
   * conditional jump) to the basic block exit point.
   */
  jmp_destn = (unsigned long) (M->next_eip + d->immediate); 
  /* Now, AND jmp_destn with 0x0000FFFFu if 16-bit mode instruction */
  if (!THIRTY_TWO_BIT_INSTR(d))					
    jmp_destn = jmp_destn & 0x0000FFFFu;
    
  /* Now issue a (the same) conditional but always 32-bit jump either to the 
     Patch Block or to the Destination 
     Basic Block (if already present). The destination of this jump will be 
     decided and filled up by the 
     translator after completing the translation of the entire basic block.
     The short versions are all of the form 0x7?. 
     The corresponding long versions are all of the form 0f 8? */
  cond = (d->instr[0] == 0x0fu) ? d->instr[1] : d->instr[0];
  if (d->flags & DSFL_GROUP2_PREFIX)
    bb_emit_byte(M, d->Group2_Prefix);
  bb_emit_byte(M, 0x0fu);
  bb_emit_byte(M, (cond & 0x0fu) | 0x80u);
  bb_emit_w32(M, 0);

  note_patch(M, M->bbOut - 4, (unsigned char *)jmp_destn, M->curr_bb_entry->proc_entry);
  /*
    M->patch_array[M->patch_count].to = (unsigned char *) jmp_destn;
    M->patch_array[M->patch_count].at = M->bbOut - 4;
    M->patch_array[M->patch_count].proc_addr = M->curr_bb_entry->proc_entry;
    M->patch_count ++;
  */

#ifdef BUILD_BBS_FOR_JCOND_TRACE
  return continue_trace(M, d, M->next_eip);
#else
  return false;			/* continue with extended basic block! */
#endif
}

bool
emit_other_jcond(machine_t *M, decode_t *d)
{
  unsigned long jmp_destn = (unsigned long) (M->next_eip + d->immediate); 
  DEBUG(emits)
    fprintf(DBG, "%lu: Other-Jcond\n", M->nTrInstr);

#ifdef PROFILE
  M->ptState->s_jmp_cond_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->jmp_cond_cnt));
#endif

  /* Now, AND jmp_destn with 0x0000FFFFu if 16-bit mode instruction */
  if (!THIRTY_TWO_BIT_INSTR(d))
    jmp_destn = jmp_destn & 0x0000FFFFu;

  if (d->flags & DSFL_GROUP2_PREFIX)
    bb_emit_byte(M, d->Group2_Prefix);
  if (d->opstate & OPSTATE_ADDR16)
    bb_emit_byte(M, 0x67u);		/* Address-size Override Prefix - to indicate CX instead of ECX */
  bb_emit_byte(M, ((OpCode *)(d->pEntry))->index);	/* jcxz, loop, loope, loopne    */
  bb_emit_byte(M, 0x2u);		/* Skip the following "skipping jump" instruction */

  bb_emit_byte(M, 0xEBu);	/* jmp rel8 */
  bb_emit_byte(M, 5); 		/* len (Jump rel32) = 5 */
	
  bb_emit_jump (M, 0);		/* Dummy jump instruction which would be patched later by the translator */
  note_patch(M, M->bbOut - 4, (unsigned char *)jmp_destn, M->curr_bb_entry->proc_entry);


#ifdef BUILD_BBS_FOR_JCOND_TRACE
  return continue_trace(M, d, M->next_eip);
#else
  return false;			/* continue with extended basic block! */
#endif
}

bool
emit_jmp(machine_t *M, decode_t *d)
{
  unsigned long jmp_destn = (unsigned long) (M->next_eip + d->immediate); 
  DEBUG(emits)
    fprintf(DBG, "%lu: Jmp-Dir\n", M->nTrInstr);

#ifdef PROFILE
  M->ptState->s_jmp_dir_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->jmp_dir_cnt));
#endif

  return continue_trace(M, d, jmp_destn);
}

bool
emit_call_disp(machine_t *M, decode_t *d)
{
  unsigned long jmp_destn = (unsigned long) (M->next_eip + d->immediate); 
  /*   unsigned long callee_index = jmp_destn & (CALL_HASH_MASK); */
  /*   unsigned long hash_entry_addr = ((unsigned long) &M->call_hash_table) + callee_index; */
  /*   unsigned long callee_index = jmp_destn & (CALL_HASH_MASK); */
  unsigned long hash_entry_addr = CALL_HASH_BUCKET(M->call_hash_table, jmp_destn);

  bb_entry *entry, *temp_entry;

  DEBUG(emits)			/*  */
    fprintf(DBG, "%lu: Call-Dir\n", M->nTrInstr);

#ifdef PROFILE
  M->ptState->s_call_dir_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->call_dir_cnt));
#endif
  
  /* Now, AND jmp_destn with 0x0000FFFFu if 16-bit mode instruction */
  if (!THIRTY_TWO_BIT_INSTR(d))					
    jmp_destn = jmp_destn & 0x0000FFFFu;
  
  /* M->next_eip points to following instruction.  Push M->next_eip */
  /* We just Push */
  bb_emit_byte(M, 0x68u);	/* PUSH */
  bb_emit_w32(M, M->next_eip);

#ifdef CALL_RET_OPT
  /* MOV M->proc_hash_table[callee_index], expected_return_address */
  //  fprintf(DBG, "Came in Disp 1\n");
  bb_emit_store_immediate_to(M, (unsigned long)(M->bbOut + 10 + 5), hash_entry_addr);  
#endif

  bb_emit_jump (M, 0);		/* Dummy jump instruction which would be patched later by the translator */
  note_patch(M, M->bbOut - 4, (unsigned char *)jmp_destn, hash_entry_addr);

  DEBUG(call_ret_opt) 
    fprintf(DBG, "Encountered a CALL(%lx); set Patch Block[%d]'s proc_addr to %lx\n", jmp_destn, M->patch_count, hash_entry_addr);
  
#ifdef CALL_RET_OPT
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  //fprintf(DBG, "Came in Disp 2\n");
  /* push %ecx */
  bb_emit_byte(M, 0x51u);
  /* mov 4(%esp) %ecx */
  bb_emit_byte(M, 0x8bu);
  bb_emit_byte(M, 0x4cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

  /* lea -(M->next_eip)(%ecx) , %ecx */
  bb_emit_byte(M, 0x8du); // 8D /r
  bb_emit_byte(M, 0x89u); // 10 001 001
  bb_emit_w32(M, (-((long)M->next_eip)));

#ifdef SIEVE_WITHOUT_PPF
  /* jecxz equal */
  bb_emit_byte(M, 0xe3u);
  bb_emit_byte(M, 0x5u);

#else  
  /* jecxz equal */
  bb_emit_byte(M, 0xe3u);
  bb_emit_byte(M, 0x6u);

  /* pop ecx */
  bb_emit_byte(M, 0x59u);
#endif /* SIEVE_WITHOUT_PPF */

  /* jmp fast_dispatch */
  bb_emit_jump(M, M->call_calls_fast_dispatch_bb);

  /* equal: pop ecx */
  bb_emit_byte(M, 0x59u);

  // esp += 4; leal 4(%esp), %esp
  bb_emit_byte(M, 0x8du);
  bb_emit_byte(M, 0xA4u); /* 10 100 100 */
  bb_emit_byte(M, 0x24u); /* 00 100 100 */
  bb_emit_w32(M, 0x4u);

#endif /* CALL_RET_OPT */

#ifdef PROFILE_BB_STATS
  M->curr_bb_entry->flags |= NEEDS_RELOC;
#endif

  return continue_trace(M, d, M->next_eip);
}

bool
emit_jmp_near_mem(machine_t *M, decode_t *d)
{
#ifdef PROFILE
  M->ptState->s_jmp_indr_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->jmp_indr_cnt));
#endif
  DEBUG(emits)
    fprintf(DBG, "%lu: Jump-Indir\n", M->nTrInstr);
  
  //  fprintf(DBG, "Jmp near mem at %lx %lx\n", M->next_eip, M->bbOut);
  bb_emit_push_rm(M, d);
  bb_emit_jump (M, M->fast_dispatch_bb);

  return true;
}


bool
emit_call_near_mem(machine_t *M, decode_t *d)
{
  bool dest_based_on_esp = false;
  
#ifdef PROFILE
  M->ptState->s_call_indr_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->call_indr_cnt));
#endif
  DEBUG(emits)
    fprintf(DBG, "%lu: Call-Indir\n", M->nTrInstr);
  
  if(d->modrm.parts.reg == 0x4u)
    dest_based_on_esp = true;
  else if(d->modrm.parts.mod == 0x3u) {
    if(d->modrm.parts.rm == 0x4u)
      dest_based_on_esp = true;
  }
  else if(d->modrm.parts.rm == 0x4u && d->sib.parts.base == 0x4u)
    dest_based_on_esp = true;
  
  if(!dest_based_on_esp) {
    /* Push M->next_eip */
    bb_emit_byte(M, 0x68u);	/* PUSH */
    bb_emit_w32(M, M->next_eip);

    bb_emit_push_rm(M, d);
  }
  else {

    /* Push the computed destination */
    /*** has to be done without touching the value of %esp ***/
    bb_emit_push_rm(M, d);

    /* xchg %eax, (%esp) */
    bb_emit_byte(M, 0x87u); // 87 /r
    bb_emit_byte(M, 0x04u); // 00 000 100
    bb_emit_byte(M, 0x24u); // 00 100 100
    
    /* push %eax */
    bb_emit_byte(M, 0x50u);

    /* mov 4(%esp), %eax */
    bb_emit_byte(M, 0x8bu); // 8b /r
    bb_emit_byte(M, 0x44u); // 01 000 100
    bb_emit_byte(M, 0x24u); // 00 100 100
    bb_emit_byte(M, 0x04u); 
    
    /* mov $M->next_eip, 4(%esp) */
    bb_emit_byte(M, 0xc7u); // C7 /0 
    bb_emit_byte(M, 0x44u); // 01 000 100
    bb_emit_byte(M, 0x24u); // 00 100 100
    bb_emit_byte(M, 0x04u);
    bb_emit_w32(M, M->next_eip);    
  }

#ifdef CALL_RET_OPT
  //fprintf(DBG, "Came in mem 1\n");

  /* PUSH %ecx */
  bb_emit_byte (M, 0x51u);

  /* mov 4(%esp), %ecx */
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x4Cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x04u);

  /* AND ECX with (CALL_HASH_MASK = CALL_TABLE_SIZE -1) */
  /* achieved by
     movzx %cl %ecx
     This is because CALL_TABLE SIZE is 2^8 = 256
  */
  bb_emit_byte(M, 0x0Fu); // 0F B6 /r
  bb_emit_byte(M, 0xB6u); // 10 110 110 
  bb_emit_byte(M, 0xC9u); // 11 001 001
   
#ifdef SIEVE_WITHOUT_PPF
  /* MOV $(M->bbOut + past_jump), M->call_hash_table(,%ecx,4) */
  bb_emit_byte(M, 0xC7u);  // C7 /0
  bb_emit_byte(M, 0x04u);  /* 00 000 100 */
  bb_emit_byte(M, 0x8Du);  /* 10 001 101 */
  bb_emit_w32 (M, (unsigned long) M->call_hash_table);
  bb_emit_w32 (M, (((unsigned long)M->bbOut) +  4 + 5));

#ifdef USE_SIEVE
#ifdef SEPARATE_SIEVES
  /* JMP M->fast_dispatch */
  bb_emit_jump (M, M->cfast_dispatch_bb);
#else
  bb_emit_jump (M, (unsigned char *) ((unsigned long)M->fast_dispatch_bb + 1));
#endif
#else
  bb_emit_jump (M, (unsigned char *) ((unsigned long)M->fast_dispatch_bb));
#endif /* USE_SIEVE */

#else
  /* MOV $(M->bbOut + past_jump), M->call_hash_table(,%ecx,4) */
  bb_emit_byte(M, 0xC7u);  // C7 /0
  bb_emit_byte(M, 0x04u);  /* 00 000 100 */
  bb_emit_byte(M, 0x8Du);  /* 10 001 101 */
  bb_emit_w32 (M, (unsigned long) M->call_hash_table);
  bb_emit_w32 (M, (((unsigned long)M->bbOut) +  4 + 6));
  
  /* POP %ecx  */
  bb_emit_byte (M, 0x59u);

  /* JMP M->fast_dispatch */
  bb_emit_jump (M, M->fast_dispatch_bb);

#endif /* SIEVE_WITHOUT_PPF */

#else
  /* JMP M->fast_dispatch */
  bb_emit_jump (M, M->fast_dispatch_bb);

#endif /* CALL_RET_OPT */


#ifdef CALL_RET_OPT
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  //  fprintf(DBG, "Came in mem 2\n");

  /* push %ecx */
  bb_emit_byte(M, 0x51u);

  /* mov 4(%esp) %ecx */
  bb_emit_byte(M, 0x8bu);
  bb_emit_byte(M, 0x4cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

  /* lea -(M->next_eip)(%ecx) , %ecx */
  bb_emit_byte(M, 0x8du); // 8D /r
  bb_emit_byte(M, 0x89u); // 10 001 001
  bb_emit_w32(M, (-((long)M->next_eip)));

#ifdef SIEVE_WITHOUT_PPF
  /* jecxz equal */
  bb_emit_byte(M, 0xe3u);
  bb_emit_byte(M, 0x5u);

#else  
  /* jecxz equal */
  bb_emit_byte(M, 0xe3u);
  bb_emit_byte(M, 0x6u);

  /* pop ecx */
  bb_emit_byte(M, 0x59u);
#endif
  
  /* jmp fast_dispatch */
  bb_emit_jump(M, M->call_calls_fast_dispatch_bb);

  /* pop ecx */
  bb_emit_byte(M, 0x59u);

  // esp += 4; leal 4(%esp), %esp
  bb_emit_byte(M, 0x8du);
  bb_emit_byte(M, 0xA4u); /* 10 100 100 */
  bb_emit_byte(M, 0x24u); /* 00 100 100 */
  bb_emit_w32(M, 0x4u);


#endif /* CALL_RET_OPT */

#ifdef PROFILE_BB_STATS
  M->curr_bb_entry->flags |= NEEDS_RELOC;
#endif

  return continue_trace(M, d, M->next_eip);
}

bool
emit_ret(machine_t *M, decode_t *d)
{
#ifdef PROFILE
  M->ptState->s_ret_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->ret_cnt));
#endif
  DEBUG(emits)
    fprintf(DBG, "%lu: Ret\n", M->nTrInstr);

#ifdef CALL_RET_OPT
  //  fprintf(DBG, "Came in ret\n");
  /* Jmp *M->curr_bb_entry->proc_entry */
  bb_emit_byte(M, 0xFFu);
  bb_emit_byte(M, 0x25u);   /* 00 100 101 */
  bb_emit_w32(M, M->curr_bb_entry->proc_entry);

#else

  /* JMP M->fast_dispatch */
  bb_emit_jump (M, M->fast_dispatch_bb);
#endif

#ifdef PROFILE_BB_STATS
  M->curr_bb_entry->flags |= NEEDS_RELOC;
#endif

  return true;
}

bool
emit_ret_Iw(machine_t *M, decode_t *d)
{
#ifdef PROFILE
  M->ptState->s_ret_Iw_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->ret_Iw_cnt));
#endif

  DEBUG(emits)
    fprintf(DBG, "%lu: Ret-IW\n", M->nTrInstr);

  /* Will there be a problem if d->imm16 < 4 ?? 
     I think this should not be the case */

  /* Pop (d->imm16-4)(%esp) */
  if (d->opstate & OPSTATE_ADDR16)
    bb_emit_byte(M, 0x66u);		/* Operand-size Override Prefix - to indicate 16 bit Return address */
  bb_emit_byte(M, 0x8Fu);  // 8F /0
  bb_emit_byte(M, 0x84u);  // 10 000 100 
  bb_emit_byte(M, 0x24u);  // 00 100 100
  bb_emit_w32(M, (unsigned long)d->imm16-4);
  
  if(d->imm16 - 4 != 0) {
    /* leal (d->imm16-4)(%esp), %esp */
    bb_emit_byte(M, 0x8du);
    bb_emit_byte(M, 0xA4u); /* 10 100 100 */
    bb_emit_byte(M, 0x24u); /* 00 100 100 */
    bb_emit_w32(M, (unsigned long)d->imm16 - 4);
  }

  
#ifdef CALL_RET_OPT
  /* Jmp *M->curr_bb_entry->proc_entry */
  //  fprintf(DBG, "Came in ret IW \n");
  bb_emit_byte(M, 0xFFu);
  bb_emit_byte(M, 0x25u);   /* 00 100 101 */
  bb_emit_w32(M, M->curr_bb_entry->proc_entry);

#else
  /* JMP M->fast_dispatch */
  bb_emit_jump (M, M->fast_dispatch_bb);
#endif

#ifdef PROFILE_BB_STATS
  M->curr_bb_entry->flags |= NEEDS_RELOC;
#endif

  return true;
}



/*   asm volatile ("movl %0, %%eax\n\t" */
/* 		"push %1\n\t" */
/* 		"call init_thread_trans\n\t" */
/* 		"mov (%%eax), %%eax\n\t" */
/* 		"movl %%eax, %2\n\t"   */
/* 		"leal 4(%%esp), %%esp\n\t" */
/* 		"popa\n\t" */
/* 		"mov %2, %%esp\n\t" */
/* 		"leave\n\t" */
/* 		"retl\n\t" */
/* 		: */
/* 		: "m" (regs.eax), "m" (next_eip), "m" (Maddr)   */
/* 		); */


/*   asm volatile ("mov %0, %%esi\n\t" */
/* 		"mov %1, %%esp\n\t" */
/* 		"push %%esi\n\t" */
/* 		"call init_thread_trans\n\t" */
/* 		"leal 4(%%esp), %%esp\n\t" */
/* 		"mov (%%eax), %%eax\n\t" */
/* 		"jmp *%%eax\n\t" */
/* 		: */
/* 		: "m" (next_eip), "m" (pastM)  */
/* 		: "%eax" */
/* 		); */
  /* Not reached */
