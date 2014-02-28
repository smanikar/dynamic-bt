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
#include <stdio.h>
#include <signal.h>
//#include <asm/sigcontext.h>
#include <bits/sigcontext.h>
#include <sys/mman.h>
#include "switches.h"
#include "debug.h"
#include "util.h"
#include "machine.h"
#include "decode.h"
#include "emit.h"
#include "xlcore.h"
#include "perf.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef INLINE_EMITTERS
#define INLINE static inline
#else
#define INLINE static
#endif

#ifdef INLINE_EMITTERS
#include "emit-inline.c"

/* emit_normal() repeated here for inlining purposes */
/* WARNING: Changes should be done in emit.c as well */
inline static bool 
inline_emit_normal(machine_t *M, decode_t *d)
{
  unsigned i;
  unsigned count = M->next_eip - d->decode_eip;
#ifdef PROFILE
  M->ptState->s_normal_cnt++;
  bb_emit_inc(M, MFLD(M, ptState->normal_cnt));
#endif

  DEBUG(emits)
    fprintf(DBG, "%lu: Normal %lx %s!\n", M->nTrInstr, d->decode_eip, ((OpCode *)d->pEntry)->disasm);

#ifdef STATIC_PASS
  memcpy(M->bbOut, (unsigned char *)d->mem_decode_eip, count);
#else
  memcpy(M->bbOut, (unsigned char *)d->decode_eip, count);
#endif
  M->bbOut += count;

  return false;
}
#endif /* INLINE_EMITTERS */

#ifdef USE_SIEVE
#ifdef SEPARATE_SIEVES 
#include "chtable.c"
#endif
#endif


ENTRY_POINT bb_entry * 
lookup_bb_eip(machine_t *M, unsigned long src_eip)
{
#ifdef USE_DIFF_HASH									
  bb_entry *curr =  M->lookup_table[((src_eip-M->guest_start_eip) & (LOOKUP_TABLE_SIZE - 1))];
#else
  bb_entry *curr =  M->lookup_table[(src_eip & (LOOKUP_TABLE_SIZE - 1))];
#endif

  while ((curr != NULL) && (curr->src_bb_eip != src_eip))
    curr = curr->next;

  DEBUG(lookup) {
    if(curr == NULL)
      fprintf(DBG, "\nLooking up %lx Failed\n", src_eip);
    else 
      fprintf(DBG, "\nLooking up %lx Success\n", src_eip);
  }
  return curr;
}

#include "xlate-helper.c"


#ifdef USE_SIEVE
INLINE void
bb_setup_hash_table(machine_t *M)
{
  int i;
  for (i=0 ; i<NBUCKETS ; i++)  {
    bb_emit_jump(M, M->slow_dispatch_bb);
    M->bbOut += 3;
  }
}
#endif /* USE_SIEVE */ 


void 
xlate_for_sieve (machine_t *M)
{
  bb_entry *entry_node;
  bucket_entry *bucket;	/* Hash bucket onto which this basic-block 
			   will chain onto (at the head)   	*/
  unsigned char *next_instr; /* Sequentially next instr of the above - 
				used just to compute relative jump destination	*/
  unsigned long node; /* A node of the hash chain */

  M->comming_from_call_indirect = false;

  entry_node = xlate_bb(M);

#ifdef USE_SIEVE
  /*   bucket =  */
  /*     (bucket_entry *)(M->hash_table + (((unsigned long)entry_node->src_bb_eip) & SIEVE_HASH_MASK)); */

  bucket = (bucket_entry *) SIEVE_HASH_BUCKET(M->hash_table, ((unsigned long)entry_node->src_bb_eip));
  //  fprintf(DBG, "Hash bucket start at %lx, this = %lx\n", M->hash_table, bucket);

  /*** Sensitive to the size of Jump Instruction in Bucket ***/
  next_instr = ((unsigned char *)bucket) + 5  ;

  node = (unsigned long) (next_instr + bucket->rel);
  bucket->rel = M->bbOut - next_instr;

  /*   fprintf(DBG, "Bucket #%ld seip = %lx teip = %lx at %lx\n",  */
  /* 	 ((unsigned char*)bucket - (M->hash_table))/sizeof(bucket_entry), */
  /* 	 entry_node->src_bb_eip, */
  /* 	 entry_node->trans_bb_eip, */
  /* 	 M->bbOut); */
  /*   fflush(DBG); */
  
#ifdef SIEVE_WITHOUT_PPF
  /* mov 0x4(%esp),%ecx */
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x4cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

  /* lea -entry->src_bb_eip(%ecx),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x89u); // 10 001 001 
  bb_emit_w32(M, (-((long)(entry_node->src_bb_eip))));

  /* jecxz equal */
  bb_emit_byte(M, 0xe3u);
  bb_emit_byte(M, 0x05u);

  /* jmp $next_bucket */
  bb_emit_jump(M, (unsigned char *)node);

  /* equal: pop %ecx */
  bb_emit_byte(M, 0x59u);

  /* leal 4(%esp) %esp */
  bb_emit_byte(M, 0x8du); // 8d /r
  bb_emit_byte(M, 0x64u); // 01 100 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

  /* jmp $translated_block */
  bb_emit_jump (M, (unsigned char *)entry_node->trans_bb_eip);
#else

  /* cmpl $entry->src_bb_eip, (%esp) */
  bb_emit_byte(M, 0x81u); // 81 /7
  bb_emit_byte(M, 0x7cu); // 01 111 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);
  bb_emit_w32(M, entry_node->src_bb_eip);

  /* jne $(next_bb) */
  bb_emit_byte (M, 0x0Fu);
  bb_emit_byte (M, 0x85u);
  bb_emit_w32 (M, node - ((unsigned long)M->bbOut + 4));

  /* popf */
  bb_emit_byte (M, 0x9Du);

  /* leal 4(%esp) %esp */
  bb_emit_byte(M, 0x8du); // 8d /r
  bb_emit_byte(M, 0x64u); // 01 100 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

  /* jmp $translated_block */
  bb_emit_jump (M, (unsigned char *)entry_node->trans_bb_eip);
#endif /* SIEVE_WITHOUT_PPF */
 
  M->jmp_target = (unsigned char *)entry_node->trans_bb_eip;
#ifdef PROFILE
  M->ptState->hash_nodes_cnt++;
#endif
#endif /* USE_SIEVE */
}


#define BORDER_START  do {				\
    /* mov $200, M->border_esp */			\
    bb_emit_byte(M, 0xc7u); /* c7 /0 */			\
    bb_emit_byte(M, 0x05u);    /* 00 000 101 */		\
    bb_emit_w32(M, (unsigned long) &M->border_esp);	\
    bb_emit_w32(M, 0x200u);				\
  } while(0)

#define BORDER_END do {					\
    /* mov $0, M->border_esp */				\
    bb_emit_byte(M, 0xc7u); /* c7 /0 */			\
    bb_emit_byte(M, 0x05u);    /* 00 000 101 */		\
    bb_emit_w32(M, (unsigned long) &M->border_esp);	\
    bb_emit_w32(M, 0x0u);				\
  } while(0)


INLINE void 
bb_setup_post_xlate(machine_t *M) // [len = 30b]
{
  /* mov 36(esp) %eax */			       
  bb_emit_byte(M, 0x8bu);  // 8b /r
  bb_emit_byte(M, 0x44u);  // 01 000 100 
  bb_emit_byte(M, 0x24u);
  bb_emit_byte(M, 36u);
  
  /* mov %eax, (esp) */
  bb_emit_byte(M, 0x89u);  // 89 /r
  bb_emit_byte(M, 0x04u);  // 00 000 100 
  bb_emit_byte(M, 0x24u);
  
  /* mov M->jmp_target, %eax */
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x05u); // 00 000 101
  bb_emit_w32(M, MFLD(M, jmp_target)); 

  /* mov %eax 36(%esp) */
  bb_emit_byte(M, 0x89u);  // 89 /r
  bb_emit_byte(M, 0x44u);  // 01 000 100 
  bb_emit_byte(M, 0x24u);
  bb_emit_byte(M, 36u);
  
  BORDER_END;

  /* 2f: popf */
  bb_emit_byte(M, 0x9du);		
  
  /* popa */
  bb_emit_byte(M, 0x61u);		
  
  /* ret */
  bb_emit_byte(M, 0xc3u);  
}

INLINE void 
bb_setup_startup_slow_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that first translates the destination basic-block 
     and then transfers control into the basic block. */
  
  BORDER_START;
  
  /* PUSH imm32:M */
  bb_emit_byte(M, 0x68u);	
  bb_emit_w32(M, (unsigned long)M);     

  /* Call xlate_for_sieve */
  bb_emit_call(M, (unsigned char*)&xlate_for_sieve);

  bb_setup_post_xlate(M);
}


#ifdef USE_SIEVE
#ifdef SIEVE_WITHOUT_PPF

INLINE void 
bb_setup_slow_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that first translates the destination basic-block 
     Found and then transfers control into the basic block. */

  /*  fast_dispatch_bb who is my caller, would have done a Push %ecx */
  /* Pop %ecx */
  bb_emit_byte(M, 0x59u);

  /* pop M->fixregs.eip */
  bb_emit_byte(M, 0x8Fu); // 8F /0
  bb_emit_byte(M, 0x05u); // 00 000 101
  bb_emit_w32(M, MREG(M, eip));

  /* PUSHF */
  bb_emit_byte(M, 0x9cu);		
  /* PUSHA */
  bb_emit_byte(M, 0x60u);		

  BORDER_START;

  /* PUSH imm32:M */
  bb_emit_byte(M, 0x68u);	
  bb_emit_w32(M, (unsigned long)M);     

  /* Call xlate_for_sieve */
  bb_emit_call(M, (unsigned char*)&xlate_for_sieve);

  bb_setup_post_xlate(M);
}

#else

INLINE void 
bb_setup_slow_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that first translates the destination basic-block 
     Found and then transfers control into the basic block. */

  /* pop M->eflags */
  bb_emit_byte(M, 0x8Fu); // 8F /0
  bb_emit_byte(M, 0x05u); // 00 000 101
  bb_emit_w32(M, MFLD(M, eflags));

  /* pop M->fixregs.eip */
  bb_emit_byte(M, 0x8Fu); // 8F /0
  bb_emit_byte(M, 0x05u); // 00 000 101
  bb_emit_w32(M, MREG(M, eip));

  // Better than popf and then pushf and use of intermediate register?
  /* push M->eflags */
  bb_emit_byte(M, 0xFFu); // FF /6
  bb_emit_byte(M, 0x35u); // 00 110 101
  bb_emit_w32(M, MFLD(M, eflags));

  bb_emit_byte(M, 0x60u);		/* PUSHA */

  BORDER_START;

  /* PUSH imm32:M */
  bb_emit_byte(M, 0x68u);	
  bb_emit_w32(M, (unsigned long)M);     
 
  /* Call xlate_for_sieve */
  bb_emit_call(M, (unsigned char *)&xlate_for_sieve);

  bb_setup_post_xlate(M);
}
#endif /* SIEVE_WITHOUT_PPF */
#else /* USE_SIEVE */
INLINE void 
bb_setup_slow_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that first translates the destination basic-block 
     Found and then transfers control into the basic block. */

  /* pop M->fixregs.eip */
  bb_emit_byte(M, 0x8Fu); // 8F /0
  bb_emit_byte(M, 0x05u); // 00 000 101
  bb_emit_w32(M, MREG(M, eip));

  bb_emit_byte(M, 0x9cu);	        /* PUSHF */
  bb_emit_byte(M, 0x60u);		/* PUSHA */

  BORDER_START;

  /* PUSH imm32:M */
  bb_emit_byte(M, 0x68u);	
  bb_emit_w32(M, (unsigned long)M);     
 
  /* Call xlate_for_sieve */
  bb_emit_call(M, M->xlate_for_sieve);

  bb_setup_post_xlate(M);
}
#endif /* USE_SIEVE */

void
xlate_for_patch_block(machine_t *M) 
{
  // Setup the EIP
  M->fixregs.eip = *((unsigned long *)(M->backpatch_block));  

  // Note the Patch point
  M->patch_point = (unsigned char *)(*((unsigned long *)
				       ((M->backpatch_block) + 4)));
  M->comming_from_call_indirect = false;

  //Translate the target
  xlate_bb(M);

  // Patch at patch_point
  *((unsigned long *)(M->patch_point)) = (M->jmp_target - 
					  (M->patch_point + 4));    
}

INLINE void
bb_setup_backpatch_and_dispatch_bb (machine_t *M)
{
  //bb_emit_byte(M, 0x65u); /* GS Segment Override Prefix - for accessing the M structure */
  bb_emit_byte(M, 0x8Fu); /* POP M->backpatch_block */
  bb_emit_byte(M, 0x05u); /* 00 000 101 */
  bb_emit_w32(M, MFLD(M, backpatch_block));

  bb_emit_byte(M, 0x9cu);		/* PUSHF */
  bb_emit_byte(M, 0x60u);		/* PUSHA */

  BORDER_START;

  bb_emit_byte(M, 0x68u);		// PUSH imm32:M
  bb_emit_w32(M, (unsigned long)M);
  bb_emit_call(M, (unsigned char *)xlate_for_patch_block);

  bb_setup_post_xlate(M);  
}

#ifdef USE_SIEVE
INLINE void
bb_setup_call_calls_fast_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that transfers control from
     one basic-block to another within the basic block cache. */

#ifdef PROFILE_RET_MISS
  bb_emit_inc(M, MFLD(M, ptState->ret_miss_cnt));
#endif

  /* mov 0x4(%esp),%ecx */
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x4cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

#ifndef SMALL_HASH
  /* lea 0x0(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x4du); // 01 001 101
  bb_emit_w32(M, 0x0u);   // This 0 word is needed. 
                          // There is no other addressing mode.

  /* movzwl %cx,%ecx */
  bb_emit_byte(M, 0x0Fu); // 0F B7 /R
  bb_emit_byte(M, 0xB7u);
  bb_emit_byte(M, 0xC9u); // 11 001 001

  /* lea M->hash_table(,%ecx,4),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x8du); // 10 001 101
  bb_emit_w32(M, (unsigned long)M->hash_table);

#else
  /* lea 0x0(,%ecx,4),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x8du); // 10 001 101
  bb_emit_w32(M, 0x0u);   // This 0 word is needed. 
                          // There is no other addressing mode.

  /* movzwl %cx,%ecx */
  bb_emit_byte(M, 0x0Fu); // 0F B7 /R
  bb_emit_byte(M, 0xB7u);
  bb_emit_byte(M, 0xC9u); // 11 001 001

  /* lea M->hash_table(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x4du); // 01 001 101
  bb_emit_w32(M, (unsigned long)M->hash_table);
#endif


  //50:	ff e1                	jmp    *%ecx
  bb_emit_byte(M, 0xffu);
  bb_emit_byte(M, 0xe1u);
  /*   /\* jmp *M->hash_table(,%ecx,4) *\/ */
  /*   bb_emit_byte(M, 0xFFu); // FF /4 */
  /*   bb_emit_byte(M, 0x24u); // 00 100 100 */
  /*   bb_emit_byte(M, 0x8Du); // 10 001 101 */
  /*   bb_emit_w32(M, (unsigned long) M->hash_table); */
}

INLINE void
bb_setup_ret_calls_fast_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that transfers control from
     one basic-block to another within the basic block cache. */

#ifdef PROFILE_RET_MISS
  bb_emit_inc(M, MFLD(M, ptState->ret_ret_miss_cnt));
#endif

  /* push %ecx */
  bb_emit_byte(M, 0x51u);

  /* mov 0x4(%esp),%ecx */
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x4cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

#ifndef SMALL_HASH
  /* lea 0x0(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x4du); // 01 001 101
  bb_emit_w32(M, 0x0u);   // This 0 word is needed. 
                          // There is no other addressing mode.

  /* movzwl %cx,%ecx */
  bb_emit_byte(M, 0x0Fu); // 0F B7 /R
  bb_emit_byte(M, 0xB7u);
  bb_emit_byte(M, 0xC9u); // 11 001 001

  /* lea M->hash_table(,%ecx,4),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x8du); // 10 001 101
  bb_emit_w32(M, (unsigned long)M->hash_table);

#else
  /* lea 0x0(,%ecx,4),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x8du); // 10 001 101
  bb_emit_w32(M, 0x0u);   // This 0 word is needed. 
                          // There is no other addressing mode.

  /* movzwl %cx,%ecx */
  bb_emit_byte(M, 0x0Fu); // 0F B7 /R
  bb_emit_byte(M, 0xB7u);
  bb_emit_byte(M, 0xC9u); // 11 001 001

  /* lea M->hash_table(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x4du); // 01 001 101
  bb_emit_w32(M, (unsigned long)M->hash_table);
#endif


  //50:	ff e1                	jmp    *%ecx
  bb_emit_byte(M, 0xffu);
  bb_emit_byte(M, 0xe1u);
  /*   /\* jmp *M->hash_table(,%ecx,4) *\/ */
  /*   bb_emit_byte(M, 0xFFu); // FF /4 */
  /*   bb_emit_byte(M, 0x24u); // 00 100 100 */
  /*   bb_emit_byte(M, 0x8Du); // 10 001 101 */
  /*   bb_emit_w32(M, (unsigned long) M->hash_table); */
}

#ifdef SIEVE_WITHOUT_PPF

INLINE void
bb_setup_fast_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that transfers control from
     one basic-block to another within the basic block cache. */

  /* push %ecx */
  bb_emit_byte(M, 0x51u);

  /* mov 0x4(%esp),%ecx */
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x4cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

#ifndef SMALL_HASH
  /* lea 0x0(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x4du); // 01 001 101
  bb_emit_w32(M, 0x0u);   // This 0 word is needed. 
                          // There is no other addressing mode.

  /* movzwl %cx,%ecx */
  bb_emit_byte(M, 0x0Fu); // 0F B7 /R
  bb_emit_byte(M, 0xB7u);
  bb_emit_byte(M, 0xC9u); // 11 001 001

  /* lea M->hash_table(,%ecx,4),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x8du); // 10 001 101
  bb_emit_w32(M, (unsigned long)M->hash_table);

#else
  /* lea 0x0(,%ecx,4),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x8du); // 10 001 101
  bb_emit_w32(M, 0x0u);   // This 0 word is needed. 
                          // There is no other addressing mode.

  /* movzwl %cx,%ecx */
  bb_emit_byte(M, 0x0Fu); // 0F B7 /R
  bb_emit_byte(M, 0xB7u);
  bb_emit_byte(M, 0xC9u); // 11 001 001

  /* lea M->hash_table(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x4du); // 01 001 101
  bb_emit_w32(M, (unsigned long)M->hash_table);
#endif


  //50:	ff e1                	jmp    *%ecx
  bb_emit_byte(M, 0xffu);
  bb_emit_byte(M, 0xe1u);
  /*   /\* jmp *M->hash_table(,%ecx,4) *\/ */
  /*   bb_emit_byte(M, 0xFFu); // FF /4 */
  /*   bb_emit_byte(M, 0x24u); // 00 100 100 */
  /*   bb_emit_byte(M, 0x8Du); // 10 001 101 */
  /*   bb_emit_w32(M, (unsigned long) M->hash_table); */
}

#else

INLINE void
bb_setup_fast_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that transfers control from
     one basic-block to another within the basic block cache. */
  /* 0. PUSHF */
  bb_emit_byte (M, 0x9Cu);

  /* Pushl 4(%esp) */
  bb_emit_byte(M, 0xFFu); // FF /6
  bb_emit_byte(M, 0x74u); // 01 110 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);
  
  /* andl $(SIEVE_HASH_MASK), (%esp) */
  bb_emit_byte(M, 0x81u); // 81/4
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_w32(M, SIEVE_HASH_MASK);

  /* add $(M->hash_table), (%esp) */
  bb_emit_byte(M, 0x81u); // 81/0
  bb_emit_byte(M, 0x04u); // 00 000 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_w32(M, (unsigned long) M->hash_table);

  /* ret */
  bb_emit_byte(M, 0xC3u);
}

#endif /* SIEVE_WITHOUT_PPF */
#endif /*  USE_SIEVE  */


#ifdef PROFILE_BB_STATS 
#define SPECIAL_BB(bb) do { \
  curr_bb_entry = make_bb_entry(M, 0, (unsigned long) M->bbOut, (unsigned long) COLD_PROC_ENTRY); \
  curr_bb_entry->flags = IS_HAND_CONSTRUCTED | IS_START_OF_TRACE | IS_END_OF_TRACE | NEEDS_RELOC; \
  bb_setup_##bb(M); \
  curr_bb_entry->src_bb_end_eip = 0;			     \
  curr_bb_entry->trans_bb_end_eip = (unsigned long)M->bbOut; \
} while(0)
#else
#define SPECIAL_BB(bb) do { \
  bb_setup_##bb(M); \
} while(0)
#endif 

static void
bb_cache_init(machine_t *M)
{
#ifdef PROFILE_BB_STATS
  bb_entry *curr_bb_entry;
#endif
  int i;

  M->bbOut = M->bbCache;
  M->bbLimit = M->bbCache + BBCACHE_SIZE;

#ifdef USE_SIEVE
#ifdef SEPARATE_SIEVES
  M->slow_dispatch_bb = M->bbOut + (NBUCKETS * sizeof(bucket_entry))
    + (CNBUCKETS * sizeof(bucket_entry));
#else
  M->slow_dispatch_bb = M->bbOut + (NBUCKETS * sizeof(bucket_entry));
#endif  

#ifdef SEPARATE_SIEVES
  /*** WARNING: sensitive to size of SLOW_DISPATCH_BB ***/
  M->cslow_dispatch_bb = M->slow_dispatch_bb + 59;
#endif

  /* Set up Sieve */
  M->hash_table = M->bbOut;
  bb_setup_hash_table(M);
#ifdef SEPARATE_SIEVES
  /* Set up Call Sieve */
  M->chash_table = M->bbOut;
  bb_setup_chash_table(M);
#endif
#else /* USE_SIEVE */
  M->slow_dispatch_bb = M->bbOut;
#endif /* USE_SIEVE */

  /* Set up BB-Directory */
  M->no_of_bbs = 0; 
  for (i=0 ; i<LOOKUP_TABLE_SIZE ; i++)
    M->lookup_table[i] = NULL;

  SPECIAL_BB(slow_dispatch_bb);
    
#ifdef SEPARATE_SIEVES
  SPECIAL_BB(cslow_dispatch_bb);
#endif 
  
  M->backpatch_and_dispatch_bb = M->bbOut;
  SPECIAL_BB(backpatch_and_dispatch_bb);
  
#ifdef USE_SIEVE
  M->fast_dispatch_bb = M->bbOut;
  SPECIAL_BB(fast_dispatch_bb);
  
#ifdef SEPARATE_SIEVES
  M->cfast_dispatch_bb = M->bbOut;
  SPECIAL_BB(cfast_dispatch_bb);
#endif /* SEPARATE_SIEVES */
  
#ifdef PROFILE_RET_MISS
  M->call_calls_fast_dispatch_bb = M->bbOut;
  SPECIAL_BB(call_calls_fast_dispatch_bb);
  M->ret_calls_fast_dispatch_bb = M->bbOut;
  SPECIAL_BB(ret_calls_fast_dispatch_bb);
#else  /* PROFILE_RET_MISS */
#ifdef SIEVE_WITHOUT_PPF
  M->call_calls_fast_dispatch_bb = M->fast_dispatch_bb + 1;  // Past the push %ecx
#else
  M->call_calls_fast_dispatch_bb = M->fast_dispatch_bb; 
#endif
  M->ret_calls_fast_dispatch_bb = M->fast_dispatch_bb;
#endif /* PROFILE_RET_MISS */
#else /* USE_SIEVE */
  M->call_calls_fast_dispatch_bb = M->slow_dispatch_bb;
  M->ret_calls_fast_dispatch_bb = M->slow_dispatch_bb;
  M->fast_dispatch_bb = M->slow_dispatch_bb;
#endif /* USE_SIEVE */
  

  M->startup_slow_dispatch_bb = M->bbOut;
  SPECIAL_BB(startup_slow_dispatch_bb);

  M->bbCache_main = M->bbOut;
 
  for(i=0; i<CALL_TABLE_SIZE; i++)
    M->call_hash_table[i] = (unsigned long) M->ret_calls_fast_dispatch_bb;
}

static void
bb_cache_reinit(machine_t *M)
{
  int i;

#ifdef PROFILE_BB_STATS
  bb_cache_init(M);
  return;
#endif

  M->bbOut = M->bbCache;
#ifdef USE_SIEVE
  bb_setup_hash_table(M);
#ifdef SEPARATE_SIEVES
  bb_setup_chash_table(M);
#endif
#endif /* USE_SIEVE */

  /* Set up BB-Directory */
  M->no_of_bbs = 0;
  for (i=0 ; i<LOOKUP_TABLE_SIZE ; i++)
    M->lookup_table[i] = NULL;
  
  M->bbOut = M->bbCache_main;

  for(i=0; i<CALL_TABLE_SIZE; i++)
    M->call_hash_table[i] = (unsigned long) M->ret_calls_fast_dispatch_bb;
}


INLINE bool 
translate_instr(machine_t *M, decode_t *ds)
{

#ifdef INLINE_EMITTERS
  if ((void *)ds->emitfn == (void *)emit_normal){
    DEBUG(inline_emits)
      fprintf(DBG, "Tanslation Path - inline_emit_normal\n");
    return inline_emit_normal(M, ds);
  }
  else 	 
#endif /* INLINE_EMITTERS */
    {
      DEBUG(inline_emits)
	fprintf(DBG, "Tanslation Path - NOT inline_emit_normal\n");
      return (ds->emitfn)(M, ds);
    }
}

static void
panic_decode_fail(unsigned long eip, unsigned long bAddr)
{
  panic("Illegal Instruction -- Decode had failed! eip = %lx byte = %lx\n",
	eip, bAddr);  
}

#ifdef STATIC_PASS  
static bool 
update_mem_next_eip(machine_t *M)
{
  int i;
  i = M->curr_sec_index;
  bool found = false;

  /* Usually in the same section */
  if((M->sec_info[i].start <= M->next_eip) && (M->sec_info[i].end >= M->next_eip)) {
    found = true;
  }
  else {
    /* If not, find it the hard way */
    for(i=0;(!found) && (i<M->nsections); i++) 
      if((M->sec_info[i].start <= M->next_eip) && (M->sec_info[i].end >= M->next_eip)) {
	found = true;
	break;
      }
  }

  if(found) {
    DEBUG(static_pass_addr_trans)
      fprintf(DBG, "\t#%lx: [%s]\n",M->next_eip, M->sec_info[i].name);
    M->mem_next_eip = (unsigned long) (M->sec_info[i].inmem + 
				       (M->next_eip - (unsigned long)M->sec_info[i].start));
    M->curr_sec_index = i;
  }
  else {
    DEBUG(static_pass_addr_trans)
      fprintf(DBG, "\t@%lx: Not Found\n",M->next_eip);  
  }
  fflush(DBG);
  return !found;
}

static void
simple_patch(machine_t *M, unsigned long at, unsigned long addr)
{
  unsigned char *tmp = M->bbOut;
  M->bbOut = (unsigned char *)at;
  bb_emit_w32(M, addr - (at + 4));
  M->bbOut = tmp;
}
#endif  

#define PATCH_BLOCK_LEN 13 /* WARNING: Sensitive to size of Patch-block */
#define MAX_PATCH_BLOCK_BYTES (PATCH_ARRAY_LEN * PATCH_BLOCK_LEN)

/* Conservative estimate used to determine if there is more room for this BB.
   Space needed for all patch_blocks + space for at least one instruction 
   I guess no emitted sequence of instructions per single instruction currently
   exceeds 64 bytes. If it does, fix the next line */
#define BYTES_NEEDED_AT_THE_END (MAX_PATCH_BLOCK_BYTES + 64)

#define ROOM_FOR_BB(M) ((M->bbLimit - M->bbOut) > BYTES_NEEDED_AT_THE_END)
#define MORE_FREE_PATCH_BLOCKS(M) (M->patch_count <= (PATCH_ARRAY_LEN - 4))  /* Leave some extra room for cases like
										call that need multiple patch_blocks */

/* THE Translator -- Returns:
   - a pointer to the bb_entry of the required destination
   - M->jmp_target holds the bb address of the destunation
   (There is no peculiar reson to have this convention, it is just a 
   hack to avoid emitting code to get the jmp destination from the
   bb_entry. The translator has to be locked prior to the backpatcher
   anyway, so this does not present any new problems for locking */

bb_entry * 
xlate_bb(machine_t *M)
{
  decode_t ds;				  /* Decode Structure - filled up by do_decode and used by translate_instr   	*/
  bool isEndOfBB = true;		  /* Flag to indicate encountering of basic-block terminating instruction    	*/
  bool goingOutofElf = true;
  int i, j;
  unsigned char * tmp;
  bb_entry *prev_bb_entry = NULL;
  bb_entry *curr_bb_entry = lookup_bb_eip(M, M->fixregs.eip), *temp_entry;
  unsigned long long start_time;
  unsigned long long end_time;

#ifdef PROFILE_BB_CNT
  bool inc_emitted = false;
#endif
  //if(M->trigger) {
  //  fprintf(DBG, "Enter xlate_bb: %lx\n", M->fixregs.eip);
  //  fflush(DBG);
  //  M->trigger = false;
  //}

  /* If the required bb is already found, just return */
  if((curr_bb_entry != NULL) && 
     (curr_bb_entry->trans_bb_eip != NOT_YET_TRANSLATED)) {
    
    DEBUG(xlate) {
      fprintf(DBG, "Entering New translation in Proc %lx\n",
	      curr_bb_entry->proc_entry);
      fflush(DBG);
    }

    M->jmp_target = (unsigned char *)curr_bb_entry->trans_bb_eip;
    return curr_bb_entry;
  }
  
#ifdef PROFILE_TRANSLATION
  start_time = read_timer();
#endif

  if(curr_bb_entry == NULL) {	    
    if (M->comming_from_call_indirect) {
      curr_bb_entry = make_bb_entry(M, M->fixregs.eip, (unsigned long) M->bbOut, 
				    (unsigned long) CALL_HASH_BUCKET(M->call_hash_table, M->fixregs.eip));
    }
    else {
      curr_bb_entry = make_bb_entry(M, M->fixregs.eip, (unsigned long) M->bbOut,
				    (unsigned long) COLD_PROC_ENTRY);
      DEBUG(xlate) {
	fprintf(DBG, "Entering New translation $#COLD$# on %lx::%lx\n",M->fixregs.eip, 
		curr_bb_entry->trans_bb_eip);
	fflush(DBG);
      }
    }
  }
  else {    
    curr_bb_entry->trans_bb_eip = (unsigned long)M->bbOut;
  }

  
#ifdef DEBUG_ON
  M->nTrInstr = 0;
#endif
  
  //fprintf(DBG, "Enter xlate_bb: %lx\n", M->fixregs.eip);
  //fflush(DBG);


  /* Did not find the translated Basic Block, So need to Translate */

  M->comming_from_call_indirect = false;
  M->curr_bb_entry = curr_bb_entry;

#if 0
  struct bb_mapping_entry {
    unsigned short src_eip_offset;
    unsigned short bb_eip_offset;
  } bb_mappings[MAX_TRACE_INSTRS];	     /* src_eip_offset to bb_eip_offset
						mappings array that will be filled up
 						after translating each instruction */
#endif
 

  M->jmp_target = M->bbOut;
  M->next_eip = M->fixregs.eip;

  M->patch_count = 0;


  /* If basic block cache is full, wipe it and start over. */
  /* This is not done inside the loop on purpose. Dont wipe the BBCache
     Unless utterly necessary. We translate as far as possible and return 
     with the hope that the guest executes upto completion without need for 
     for translation */
  if ((!ROOM_FOR_BB(M)) || (M->no_of_bbs >= MAX_BBS)) {

#ifdef SIGNALS    
    sigset_t oldSet;
    sigprocmask(SIG_SETMASK, &allSignals, &oldSet);  
#endif
    
    DEBUG(xlate) 
    {
      fprintf(DBG, "Wiping basic block cache\n");
    }
    bb_cache_reinit(M);

#ifdef SIGNALS    
    sigprocmask(SIG_SETMASK, &allSignals, &oldSet);  
#endif    
    return xlate_bb(M);
  }

#ifdef PROFILE_BB_STATS
  bb_entry *this_bb_entry = M->curr_bb_entry;
  this_bb_entry->flags = IS_START_OF_TRACE;
  this_bb_entry->src_bb_end_eip = M->next_eip;
  this_bb_entry->trans_bb_end_eip = (unsigned long) M->bbOut;
#endif

  /* This loop executes once per instruction */  
  while (ROOM_FOR_BB(M) && MORE_FREE_PATCH_BLOCKS(M)) {
    
    /*If it is necessary to limit the trace length (I don't know why)
      use : M->nTrInstr < MAX_TRACE_INSTRS */
    
    /* See if the instruction needs to be decoded */
#ifdef STATIC_PASS
    goingOutofElf = update_mem_next_eip(M); /* Jump to a library etc, outside of the 
					       current elf file */
    if (goingOutofElf)
      break;
#endif
    
    /* Decode the Instruction */    
    if (do_decode(M, &ds) == false) {

      DEBUG(decode_eager_panic) 
	panic ("do_decode failed at instr: %lx byte: %lx\n",
	       ds.decode_eip, ds.pInstr);

      // Emit a call to panic ...
      bb_emit_byte(M, 0x68u);
      bb_emit_w32(M, (unsigned long) ds.pInstr);      
      bb_emit_byte(M, 0x68u);
      bb_emit_w32(M, (unsigned long) M->next_eip);
      bb_emit_call(M, (unsigned char *) panic_decode_fail);      
      break;
    }

    DEBUG(show_each_instr_trans) {
      unsigned long bbno = (M->curr_bb_entry - M->bb_entry_nodes);
      fprintf(DBG, "bb# %lu, ", bbno);
      do_disasm(&ds, DBG);
      fflush(DBG);
    }
 
#ifdef PROFILE_BB_CNT      
    if(prev_bb_entry != M->curr_bb_entry) {
      prev_bb_entry = M->curr_bb_entry;
      inc_emitted = false;
    }

    if(!inc_emitted) {
      OpCode *p = (OpCode *) ds.pEntry;
      if((SOURCES_FLAGS(p) == 0) && (MODIFIES_OSZAPF(p))) {
	bb_emit_lw_inc(M, (unsigned long)&(M->ptState->bb_cnt));
	inc_emitted = true;
      }
      else if(p->attr & DF_BRANCH) {
	// BB is about to end
	bb_emit_inc(M, (unsigned long)&(M->ptState->bb_cnt));
	inc_emitted = true;
      }      
    }
#endif

    unsigned char *begin_bbout = M->bbOut;
#ifdef DEBUG_ON
    M->nTrInstr++;
#endif

#ifdef PROFILE_BB_STATS
    this_bb_entry->nInstr++;
    /* Note:  this_bb_entry->src_bb_end_eip MUST be updated before 
       emitting. This is because, some emitters do change M->next_eip */
    this_bb_entry->src_bb_end_eip = M->next_eip;
#endif 

    /* Emit the Instruction using the appropriate emitter */
    isEndOfBB = translate_instr(M, &ds);
    
    DEBUG(show_each_trans_instr) {
      unsigned long saved_Meip = M->next_eip;
      M->next_eip = (unsigned long)begin_bbout;
      while(M->next_eip < (unsigned long)M->bbOut) {
 	decode_t dd;
	do_decode(M, &dd);
	do_disasm(&dd, DBG);
	fflush(DBG);
      }
      M->next_eip = saved_Meip;
      fprintf(DBG, "\n");
    }
  
#ifdef PROFILE_BB_STATS
    /* Note:  this_bb_entry->trans_bb_end_eip MUST be updated after 
       emitting, for obvious reasons. */
    this_bb_entry->trans_bb_end_eip = (unsigned long) M->bbOut;
    if(this_bb_entry != M->curr_bb_entry) {
      this_bb_entry->trace_next = M->curr_bb_entry;
      this_bb_entry = M->curr_bb_entry;
    }
#endif
 
    if (isEndOfBB)
      break;
  } /* Grand Translation Loop */
  
  if (!isEndOfBB) {
    if(M->curr_bb_entry->trans_bb_eip == (unsigned long)M->bbOut)
      M->curr_bb_entry->trans_bb_eip = NOT_YET_TRANSLATED;
    bb_emit_jump(M, 0);
    M->patch_array[M->patch_count].at = M->bbOut - 4;
    M->patch_array[M->patch_count].to = (unsigned char *)M->next_eip;
    M->patch_array[M->patch_count].proc_addr = M->curr_bb_entry->proc_entry;
    M->patch_count ++;
  }
#ifdef PROFILE_BB_STATS
  else {
    M->curr_bb_entry->flags |= IS_END_OF_TRACE;
    M->curr_bb_entry->trace_next = NULL;
  }
#endif

#ifdef STATIC_PASS
  /* I need not emit Patch blocks when statically translating. The
     driver will next add these items to the worklist, and will
     arrange for back-patching
     
     However, If I am going out of the current binary, 
     I must emit a patch block. hence the following ...   */

  if(goingOutofElf) {
    bb_entry *entry = lookup_bb_eip (M, M->next_eip);
    bb_emit_byte (M, 0xE8u);	/* CALL rel32 */
    bb_emit_w32 (M, (unsigned long) (M->backpatch_and_dispatch_bb - (unsigned long)(M->bbOut + 4)));
    bb_emit_w32 (M, M->next_eip);
    bb_emit_w32 (M, (unsigned long)M->bbOut - 4);
    if(entry == NULL) {
      temp_entry = make_bb_entry(M, M->next_eip, NOT_YET_TRANSLATED, M->curr_bb_entry->proc_entry);
    }
  }
#else
  /* Lastly, emit the Patch Blocks */
  for (i = 0 ; i < M->patch_count ; i ++) {
    bb_entry *entry = NULL; 
    unsigned long addr;
    unsigned long to = (unsigned long)M->patch_array[i].to;
    unsigned long at = (unsigned long)M->patch_array[i].at;

    entry = lookup_bb_eip (M, to);
    if ((entry != NULL) && (entry->trans_bb_eip != NOT_YET_TRANSLATED)) {
      //		if (entry != NULL)
      DEBUG(xlate_pb) {
	fprintf(DBG, "1. Patch Block: BB Already found\n");
	fflush(DBG);
      }
      addr = entry->trans_bb_eip;
      /* If found to be translated already, patch the jump destination right now to implement chaining */
      tmp = M->bbOut;
      M->bbOut = (unsigned char *)at;
      bb_emit_w32(M, addr - (at + 4));
      M->bbOut = tmp;
    }
    else {
      /* If not, patch the jump destination so that it jumps to its corresponding patch block */
      tmp = M->bbOut;
      M->bbOut = (unsigned char *)at;
      bb_emit_w32(M, tmp - (M->bbOut + 4));
      M->bbOut = tmp;
      
      /* Then, build the patch block */
      bb_emit_byte (M, 0xE8u);	/* CALL rel32 */
      bb_emit_w32 (M, (unsigned long) (M->backpatch_and_dispatch_bb - (unsigned long)(M->bbOut + 4)));
      bb_emit_w32 (M, to);
      bb_emit_w32 (M, at);
      if(entry == NULL) {
	temp_entry = make_bb_entry(M, to, NOT_YET_TRANSLATED, M->patch_array[i].proc_addr);
      }
    }
  }
#endif

#ifdef PROFILE_TRANSLATION
  end_time = read_timer();
  M->ptState->trans_time += (end_time - start_time);
#endif

  return(curr_bb_entry);;   
}

machine_t *MM;

void 
handle_sigsegv(int param, struct sigcontext ctx)
{
  struct sigcontext *context = &ctx;
  bb_entry *curr;
  int i;
  machine_t *M = MM;

  fprintf(DBG, "\nVDebug is handling SIGSEGV...\n");
 	
  fprintf(DBG, "eip: %lx\n", context->eip);
  fprintf(DBG, "Fault code = %ld\n", context->trapno); 
  fprintf(DBG, "If(Pagefault), cr2 = %lx\n", context->cr2); 
  
  fflush(DBG);
  
#if 0   
  for (i=0 ; i<LOOKUP_TABLE_SIZE ; i++) {
    curr =  M->lookup_table[i];
    while ((curr != NULL) && 
	   ((context->eip < curr->trans_bb_eip) || (context->eip >= curr->trans_bb_eip + curr->trans_bb_size)))
      curr = curr->next;
    if (curr != NULL) {
      int j;
      printf ("\nFound src_bb_start_eip to be: %08X\n", curr->src_bb_eip);
      printf ("\ntrans_bb_start_eip is: %08X\n", curr->trans_bb_eip);
      printf ("\nAnd, the Basic Block is as follows: \n");
      for (j=0 ; j<curr->trans_bb_size ; j++)
	printf ("%02X  ", *((unsigned char *)curr->trans_bb_eip + j));
      printf ("\n");
    }
  }
  fflush(DBG);
#endif
  
  fprintf(DBG, "BBCache is:\n");
  fflush(DBG);

  M->next_eip = (unsigned long)M->slow_dispatch_bb;
  while(M->next_eip < (unsigned long)M->backpatch_and_dispatch_bb) {
    decode_t dd;
    do_decode(M, &dd);
    do_disasm(&dd, stderr);
    fflush(stderr);
  }
  
  
  
  M->next_eip = (unsigned long)M->fast_dispatch_bb;
  while(M->next_eip < (unsigned long)M->bbCache_main) {
    decode_t dd;
    do_decode(M, &dd);
    do_disasm(&dd, DBG);
    fflush(DBG);
  }
  fprintf(DBG, "\n");
  exit(1);
}


#ifndef USE_STATIC_DUMP
machine_t theMachine;
pt_state thePtState;

#endif

#ifdef TOUCH_RELOADED_BBCACHE_PAGES
static void
touch_pages(machine_t *M)
{
  size_t n;
  for(n=0; n<450; n++) {
    size_t ndx = n*PAGE_SIZE + 1;
    volatile val;
    val = ((unsigned long *)M->bbCache)[ndx];
    ((unsigned long *)M->bbCache)[ndx] = val;
  }
}
#endif /* TOUCH_RELOADED_BBCACHE_PAGES */



machine_t * 
init_translator(unsigned long program_start)
{
  machine_t *M;
  int i;
  bb_entry *temp_entry;
  unsigned long long start_time;
  unsigned long long end_time;
  
#ifdef PROFILE_TRANSLATION
  start_time = read_timer();
#endif

#ifdef SIGNALS
  sigfillset(&allSignals);
  sigset_t oldSet;
  sigprocmask(SIG_SETMASK, &allSignals, &oldSet);  
#endif  

  if(debug_flags) {
    DBG = stderr;
    //DBG = fopen("vdbg.dbg", "w");
  }

  DEBUG(startup) {
    printf("VDebug rules!... \n");
    printf("Initial eip   = %lx\n", program_start);
  }

#ifndef USE_STATIC_DUMP
  M = &theMachine;
  M->ismmaped = false;
  M->ptState = &thePtState;

  bb_cache_init(M);
  temp_entry = make_bb_entry(M, program_start, NOT_YET_TRANSLATED, 
			     CALL_HASH_BUCKET(M->call_hash_table, program_start));
#ifdef PROFILE_BB_STATS
  temp_entry->flags = 0;
#endif

#else /* USE_STATIC_DUMP */

  char str[300];
  char arg[300];
  sprintf(arg, "/proc/%d/exe", getpid());
  i = readlink(arg,str,sizeof(str));
  str[i] = '\0';
  for(i--; i>=0 ; i--)
    if(str[i] == '/')
      str[i] = '_';
  
  sprintf(arg, "/tmp/vdebug-dump/%s-dump",str);
  int fd1 = open(arg, O_RDONLY, S_IRWXU);

  sprintf(arg, "/tmp/vdebug-dump/%s-addr",str);
  int fd2 = open(arg, O_RDONLY, S_IRWXU);

  size_t mapSize = sizeof(machine_t) + sizeof(pt_state);
  mapSize = mapSize + (mapSize % PAGE_SIZE);    
  if(fd1 == -1 || fd2 == -1) {
    M = (machine_t *) mmap(0, mapSize, 
			   PROT_READ | PROT_WRITE | PROT_EXEC,
			   MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, 0, 0);
    M->ismmaped = true;
    M->ptState = (pt_state *)(((unsigned char *)M) + sizeof(machine_t));

    if(M == MAP_FAILED)
      panic("Allocation of M failed err = %s", strerror(errno));
 
    bb_cache_init(M);
    temp_entry = make_bb_entry(M, program_start, NOT_YET_TRANSLATED, 
			       CALL_HASH_BUCKET(M->call_hash_table, program_start));
#ifdef PROFILE_BB_STATS
    temp_entry->flags = 0;
#endif
    M->dump = true;
  }
  else {
    machine_t *maddr;
    read(fd2, &maddr, sizeof(unsigned char *));
    close(fd2);

    DEBUG(dump_load)
      fprintf(DBG, "Came to File-open area of initializer\n");
    
    M = (machine_t *) mmap(maddr, mapSize, 
			   PROT_READ | PROT_WRITE | PROT_EXEC,
			   MAP_FIXED | MAP_PRIVATE | MAP_NORESERVE, fd1, 0);
    if(M == MAP_FAILED)
      panic("Allocation of M at %lx failed err = %s", maddr, strerror(errno));
    close(fd1);
    M->dump = false;
    M->ismmaped = true;
    M->ptState = (pt_state *)(((unsigned char *)M) + sizeof(machine_t));

#ifdef TOUCH_RELOADED_BBCACHE_PAGES
    fprintf(DBG, "%lu, %lu\n", (M->bbOut - M->bbCache)/PAGE_SIZE, BBCACHE_SIZE/PAGE_SIZE);
    fflush(DBG);
    touch_pages(M);
#endif /* TOUCH_RELOADED_BBCACHE_PAGES */
  }
#endif /* USE_STATIC_DUMP main */
  
#ifdef SIGNALS
  M->ptState->Mnode.pid = getpid();
  M->ptState->Mnode.M = M;
  M->ptState->Mnode.next = NULL;
  addToMlist(&M->ptState->Mnode);
  //M->sigQfront = 0;
#endif /* SIGNALS */
  
  M->fixregs.eip = program_start;
  M->guest_start_eip = program_start;
  M->comming_from_call_indirect = false;

#ifdef DEBUG_ON
  M->nTrInstr = 0;
#endif

  //M->trigger = false;

#ifdef PROFILE
  M->ptState->total_cnt = 0;
  M->ptState->normal_cnt = 0; 
  M->ptState->ret_cnt = 0;
  M->ptState->ret_Iw_cnt = 0;
  M->ptState->call_dir_cnt = 0;
  M->ptState->call_indr_cnt = 0;
  M->ptState->jmp_indr_cnt = 0;
  M->ptState->jmp_dir_cnt = 0;
  M->ptState->jmp_cond_cnt = 0;

  M->ptState->s_total_cnt = 0;
  M->ptState->s_normal_cnt = 0; 
  M->ptState->s_ret_cnt = 0;
  M->ptState->s_ret_Iw_cnt = 0;
  M->ptState->s_call_dir_cnt = 0;
  M->ptState->s_call_indr_cnt = 0;
  M->ptState->s_jmp_indr_cnt = 0;
  M->ptState->s_jmp_dir_cnt = 0;
  M->ptState->s_jmp_cond_cnt = 0;

  M->ptState->hash_nodes_cnt = 0;
  M->ptState->max_nodes_trav_cnt = 0;
#endif 
 
#ifdef PROFILE_RET_MISS  
  M->ptState->ret_miss_cnt = 0;
  M->ptState->ret_ret_miss_cnt = 0;
  M->ptState->cold_cnt = 0;
#endif

#ifdef PROFILE_BB_CNT  
  M->ptState->bb_cnt = 0;
#endif

  DEBUG(sigsegv) {
    MM = M;
    signal(SIGSEGV, (void *)handle_sigsegv);
  }
  
#ifdef SIGNALS
  sigprocmask(SIG_SETMASK, &oldSet, NULL);
#endif

#ifdef PROFILE_TRANSLATION
  end_time = read_timer();
  M->ptState->tot_time = start_time;
  M->ptState->trans_time = (end_time - start_time);  
#endif  
  return M;
}

 
#ifdef THREADED_XLATE 
machine_t *   
init_thread_trans(unsigned long program_start)
{
  machine_t *M;
  int i;
  bb_entry *temp_entry;

#ifdef SIGNALS
  sigset_t oldSet;
  sigprocmask(SIG_SETMASK, &allSignals, &oldSet);  
#endif

  DEBUG(thread_init) {
    fprintf(DBG, "Thread start eip   = %lx\n", program_start);
  }

  size_t mapSize = sizeof(machine_t) + sizeof(pt_state);
  mapSize = mapSize + (mapSize % PAGE_SIZE);    
  M = (machine_t *) mmap(0, mapSize, 
			 PROT_READ | PROT_WRITE | PROT_EXEC,
			 MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, 0, 0);
  if(M == MAP_FAILED)
    panic("Allocation of M for failed for a thread start = %lx err = %s", 
	  program_start,
	  strerror(errno));
  
  M->ismmaped = true;
  M->ptState = (pt_state *)(((unsigned char *)M) + sizeof(machine_t));
  
  DEBUG(thread_init) {
    fprintf(DBG, "M address = %lx\n", M);
  }

#ifdef SIGNALS
  M->ptState->Mnode.pid = getpid();
  M->ptState->Mnode.M = M;
  M->ptState->Mnode.next = NULL;
  addToMlist(&M->ptState->Mnode);
  //M->sigQfront = 0;
#endif /* SIGNALS */
  
  bb_cache_init(M);
  temp_entry = make_bb_entry(M, program_start, NOT_YET_TRANSLATED, 
			     CALL_HASH_BUCKET(M->call_hash_table, program_start));

#ifdef PROFILE_BB_STATS
  temp_entry->flags = 0;
#endif
  
  M->fixregs.eip = program_start;
  M->guest_start_eip = program_start;
  M->comming_from_call_indirect = false;

#ifdef DEBUG_ON
  M->nTrInstr = 0;
#endif

#ifdef PROFILE
  M->ptState->total_cnt = 0;
  M->ptState->normal_cnt = 0; 
  M->ptState->ret_cnt = 0;
  M->ptState->ret_Iw_cnt = 0;
  M->ptState->call_dir_cnt = 0;
  M->ptState->call_indr_cnt = 0;
  M->ptState->jmp_indr_cnt = 0;
  M->ptState->jmp_dir_cnt = 0;
  M->ptState->jmp_cond_cnt = 0;

  M->ptState->s_total_cnt = 0;
  M->ptState->s_normal_cnt = 0; 
  M->ptState->s_ret_cnt = 0;
  M->ptState->s_ret_Iw_cnt = 0;
  M->ptState->s_call_dir_cnt = 0;
  M->ptState->s_call_indr_cnt = 0;
  M->ptState->s_jmp_indr_cnt = 0;
  M->ptState->s_jmp_dir_cnt = 0;
  M->ptState->s_jmp_cond_cnt = 0;

  M->ptState->hash_nodes_cnt = 0;
  M->ptState->max_nodes_trav_cnt = 0;
#endif
 
#ifdef PROFILE_RET_MISS  
  M->ptState->ret_miss_cnt = 0;
  M->ptState->ret_ret_miss_cnt = 0;
  M->ptState->cold_cnt = 0;
#endif

#ifdef PROFILE_BB_CNT
  M->ptState->bb_cnt = 0;
#endif
  
  DEBUG(sigsegv) {
    MM = M;
    signal(SIGSEGV, (void *)handle_sigsegv);
  }
  
#ifdef SIGNALS
  sigprocmask(SIG_SETMASK, &oldSet, NULL);
#endif
  //fprintf(stderr, "[TH] M->ismmaped = %s\n", (M->ismmaped)?"true":"false");
  //fflush(stderr);
  return M;
}
#endif /* THREADED_XLATE */


#ifdef SIGNALS

/* Signals are MASKED when this function is called */
machine_t *   
init_signal_trans(unsigned long program_start, machine_t *parentM)
{
  machine_t *M;
  int i;
  bb_entry *temp_entry;

  DEBUG(sig_init_exit) {
    fprintf(DBG, "Signal start eip   = %lx\n", program_start);
  }

  size_t mapSize = sizeof(machine_t);
  mapSize = mapSize + (mapSize % PAGE_SIZE);    
  M = (machine_t *) mmap(0, mapSize, 
			 PROT_READ | PROT_WRITE | PROT_EXEC,
			 MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, 0, 0);
  if(M == MAP_FAILED)
    panic("Allocation of M for failed for a signal start = %lx, err = %s",
	  program_start, strerror(errno));
  
  M->ismmaped = true;
  M->prevM = parentM;
  M->ptState = parentM->ptState;
  DEBUG(sig_init_exit) {
    fprintf(DBG, "M address = %lx\n", M);
  }
  
  bb_cache_init(M);
  temp_entry = make_bb_entry(M, program_start, NOT_YET_TRANSLATED, 
			     CALL_HASH_BUCKET(M->call_hash_table, program_start));

#ifdef PROFILE_BB_STATS
  temp_entry->flags = 0;
#endif
  
  M->fixregs.eip = program_start;
  M->guest_start_eip = program_start;
  M->comming_from_call_indirect = false;

#ifdef DEBUG_ON
  M->nTrInstr = 0;
#endif
  
  //fprintf(stderr, "[TH] M->ismmaped = %s\n", (M->ismmaped)?"true":"false");
  //fflush(stderr);
  return M;
}

#endif /* SIGNALS */

#if 0
#ifdef PROFILE_BB_CNT      
    OpCode *p = (OpCode *) ds.pEntry;
    if(prev_bb_entry != M->curr_bb_entry) {
      // Deal with the previous entry
      if(pp_inc_addr && modifies_before_source) {
	unsigned char *temp_bbOut = M->bbOut;
	M->bbOut = (unsigned char *)pp_inc_addr;
	bb_emit_nop_inc(M, (unsigned long)&M->ptState->bb_cnt3);
	M->bbOut = temp_bbOut;
      }

      // New BB has started, emit an INC
      // SOURCES_FLAGS(p), MODIFIES_FLAGS(p)
      if((SOURCES_FLAGS(p) == 0) && (MODIFIES_FLAGS(p))) {
	pp_inc_addr = 0;
	bb_emit_lw_inc(M, (unsigned long)&M->ptState->bb_cnt2);
	modifies_before_source = true;	
      }
      else {
	pp_inc_addr = (unsigned long)M->bbOut;
	bb_emit_inc(M, (unsigned long)&M->ptState->bb_cnt1);
	sources = false;
	modifies_before_source = false;
      }      
      prev_bb_entry = M->curr_bb_entry;
    }

    if(SOURCES_FLAGS(p))
      sources = true;
    else if (MODIFIES_FLAGS(p)) {
      if(!sources)
	modifies_before_source = true;
    }
    
/*   unsigned long total = M->ptState->bb_cnt1 + M->ptState->bb_cnt2; */
/*   fprintf(f, "BB Count (ppf)    = %7lu (%0.2f%%)\n", M->ptState->bb_cnt1, */
/* 	 PERC(M->ptState->bb_cnt1, total)); */
/*   fprintf(f, "BB Count (no ppf) = %7lu (%0.2f%%)\n", M->ptState->bb_cnt2, */
/* 	 PERC(M->ptState->bb_cnt2, total)); */
/*   fprintf(f, "Total BB Count    = %7lu\n", total); */
/*   fprintf(f, "__________________________________\n\n"); */
/*   fclose(f); */
#endif
#endif
