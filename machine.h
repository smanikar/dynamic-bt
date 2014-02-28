#ifndef MACHINE_H
#define MACHINE_H
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
#include "signals.h"

const char const * const* sig_names;
const char const * const* syscall_names;

#define BBCACHE_SIZE 		4096 * 1024
#define MAX_BBS			BBCACHE_SIZE / 32	/* 32 is the sizeof(bb_entry) */
							/* Used to hold BB-directory's buckets */
#define LOOKUP_TABLE_SIZE	BBCACHE_SIZE /128	/* BB-directory hash table size */
#define MAX_TRACE_INSTRS 	512                     /* Usually not enforced */
#define PATCH_ARRAY_LEN         256
#define COLD_PROC_ENTRY		&M->call_hash_table[0]
#define NOT_YET_TRANSLATED	((unsigned long) &bad_dispatch)

#ifdef USE_SIEVE
#ifndef SMALL_HASH
#define NBUCKETS		32768       /* Code Hash Table Size       */
#else
#define NBUCKETS		16384       /* Code Hash Table Size       */
#endif

#ifdef SIEVE_WITHOUT_PPF
#define SIEVE_HASH_MASK  (NBUCKETS-1)
#define SIEVE_HASH_BUCKET(m, c) ((unsigned long)(m) + (((c) & SIEVE_HASH_MASK)*8))
#else
#define SIEVE_HASH_MASK (((NBUCKETS) - 1) << 3)
#define SIEVE_HASH_BUCKET(m, c) ((unsigned long)(m) + (((c) & SIEVE_HASH_MASK)))
#endif

#ifdef SEPARATE_SIEVES
#ifndef SMALL_HASH
#define CNBUCKETS		32768       /* Code Hash Table for call infirect Size */
#else
#define CNBUCKETS		16384       /* Code Hash Table for call infirect Size */
#endif

#define CSIEVE_HASH_MASK  (CNBUCKETS-1)
#define CSIEVE_HASH_BUCKET(m, c) ((unsigned long)(m) + (((c) & CSIEVE_HASH_MASK)*8))
#endif
#endif /* USE_SIEVE */

#define CALL_TABLE_SIZE		256
/* #define CALL_HASH_MASK  (CALL_TABLE_SIZE-1) << 2 */
/* #define CALL_HASH_BUCKET(m, c) ((unsigned long)(m) + (((c) & CALL_HASH_MASK))) */

#define CALL_HASH_MASK  (CALL_TABLE_SIZE-1)
#define CALL_HASH_BUCKET(m, c) ((unsigned long)(m) + (((c) & CALL_HASH_MASK)*4))


/* modrm byte */
typedef union modrm_union modrm_union;
union modrm_union {
  struct {
    unsigned char rm : 3;
    unsigned char reg : 3;
    unsigned char mod : 2;
  } parts; 
  unsigned char byte;
};

/* sib byte */
typedef union sib_union sib_union;
union sib_union {
  struct {
    unsigned char base : 3;
    unsigned char index : 3;
    unsigned char ss : 2;
  } parts;
  unsigned char byte;
};

typedef struct bucket_entry bucket_entry;
struct bucket_entry {
  unsigned char jmp_byte;	/* 1 */
  unsigned long rel;		/* 4 */
  unsigned char filler[3];	/* 3 */
}__attribute__((packed));

typedef struct bb_header bb_header;
struct bb_header {
  unsigned char cmp_byte;	/* 1 */
  unsigned long start_eip;	/* 4 */
  unsigned char jne_byte1;	/* 1 */
  unsigned char jne_byte2;	/* 1 */
  unsigned long next_bb;	/* 4 */
  /*--------------------------*/
  unsigned char pop_ebx;	/* 1 */
  unsigned char pop_eax;	/* 1 */
  unsigned char popf_byte;	/* 1 */
  unsigned char jmp_byte;	/* 1 */
  unsigned long this_bb;	/* 4 */
}__attribute__((packed));

/* Declarations of values for flag-bits in
   bb_entry structure */
#define IS_START_OF_TRACE      0x01u
#define IS_END_OF_TRACE        0x02u
#define NEEDS_RELOC            0x04u
#define IS_HAND_CONSTRUCTED    0x08u

#define MARKED 0x100u /* Generic Marker, should be set and cleared by 
			 the procedure that uses it */


typedef struct bb_entry bb_entry;
struct bb_entry {
  unsigned long src_bb_eip;
  unsigned long trans_bb_eip;
  unsigned long src_bb_end_eip;
  unsigned long trans_bb_end_eip;
  bb_header* sieve_header;
  unsigned long proc_entry;
  bb_entry *next;
#ifdef PROFILE_BB_STATS
  bb_entry *trace_next;
  unsigned long flags;
  unsigned long nInstr;
#endif
};//__attribute__((packed));

typedef struct patch_entry patch_entry;
struct patch_entry {
  unsigned char *at;
  unsigned char *to;
  unsigned long proc_addr;
  unsigned long prev_patch_array;
};


typedef struct bb_link bb_link;
struct bb_link {
  unsigned char *prev_BBcache;
  bb_entry **prev_BBdirectory;
  unsigned long *prev_call_table;
  bb_entry *prev_bb_entry_nodes;
};

/*
#ifdef STATIC_PASS
This section has temporarily not been ifdefed 
ON PURPOSE.
The Dynamic loader has to map the same structure 
as dumped by the static pass.
*/

typedef struct sec_mem sec_mem;
struct sec_mem {
  unsigned int id; 
  const char *name;
  unsigned long start;
  unsigned long end;
  unsigned char *inmem;
};


/*
#endif
*/

/* Subset of the General-Purpose Registers */
typedef struct fixregs_s fixregs_t;
struct fixregs_s {
  unsigned long edi;
  unsigned long esi;
  unsigned long ebp;
  unsigned long esp;
  unsigned long ebx;
  unsigned long edx;
  unsigned long ecx;
  unsigned long eax;

  /* This EIP is the GUEST eip, not the one 
     that points into the basic block cache. */
  unsigned long eip;
}__attribute__((packed));

#define GP_REG_EAX      0x0u
#define GP_REG_ECX      0x1u
#define GP_REG_EDX      0x2u
#define GP_REG_EBX      0x3u
#define GP_REG_ESP      0x4u
#define GP_REG_EBP      0x5u
#define GP_REG_ESI      0x6u
#define GP_REG_EDI      0x7u

#define PREFIX_REPZ	0xf3u
#define PREFIX_REPNZ	0xf2u
#define PREFIX_LOCK	0xf0u

#define PREFIX_CS	0x2eu
#define PREFIX_DS	0x3eu
#define PREFIX_ES	0x26u
#define PREFIX_FS	0x64u
#define PREFIX_GS	0x65u
#define PREFIX_SS	0x36u

#define PREFIX_OPSZ	0x66u
#define PREFIX_ADDRSZ	0x67u

#ifdef SIGNALS
typedef struct sa_table_s sa_table_t;
struct sa_table_s {
  k_sigaction new;
  k_sigaction old;
  k_sigaction aux;
};
#endif /* SIGNALS */


/* Thread-wide persistent Mstate */
typedef struct pt_state pt_state;
struct pt_state {
#ifdef SIGNALS
  /* sigaction information */
  sa_table_t sa_table[NSIGNALS]; // sigaction corresponding to each signal
  Mlist_t Mnode;

  int curr_signo;
  
  k_sigaction *guest_saPtr; 
  sighandler_t *guest_shPtr;
  k_sigaction *guestOld_saPtr; 
  sighandler_t *guestOld_shPtr;
#endif /* SIGNALS */
 
#ifdef PROFILE
  unsigned long long total_cnt;
  unsigned long normal_cnt; 
  unsigned long ret_cnt;
  unsigned long ret_Iw_cnt;
  unsigned long call_dir_cnt;
  unsigned long call_indr_cnt;
  unsigned long jmp_indr_cnt;
  unsigned long jmp_dir_cnt;
  unsigned long jmp_cond_cnt;

  unsigned long long s_total_cnt;
  unsigned long s_normal_cnt; 
  unsigned long s_ret_cnt;
  unsigned long s_ret_Iw_cnt;
  unsigned long s_call_dir_cnt;
  unsigned long s_call_indr_cnt;
  unsigned long s_jmp_indr_cnt;
  unsigned long s_jmp_dir_cnt;
  unsigned long s_jmp_cond_cnt;

  unsigned long hash_nodes_cnt;
  unsigned long max_nodes_trav_cnt;
#endif
  
#ifdef PROFILE_RET_MISS  
  unsigned long cold_cnt;
  unsigned long ret_miss_cnt;
  unsigned long ret_ret_miss_cnt;
#endif  

#ifdef PROFILE_BB_CNT
  unsigned long bb_cnt;
#endif

#ifdef PROFILE_TRANSLATION
  unsigned long long trans_time;
  unsigned long long tot_time;
#endif

#ifdef USE_STATIC_DUMP
  bool dump;
#endif  

  bool trigger; // Keep the compiler happy
};

/* Virtual Machine State + Emulator's state required for hosting 
   this guest VM */
typedef struct machine_s machine_t;
struct machine_s {
  /* This field should be the first one, as UserEntry.s depends on the same. */
  unsigned char *startup_slow_dispatch_bb;
  bool ismmaped;
  unsigned long guest_start_eip;

  unsigned long eflags;
  fixregs_t fixregs;
  unsigned long next_eip;      /* Logical Next EIP for the decoder */

/*#ifdef STATIC_PASS
  This section has temporarily not been ifdefed ON PURPOSE.
  The Dynamic loader has to map the same structure 
  as dumped by the static pass.*/
  unsigned long mem_next_eip;  /* Actual  pointer to the in-memory copy
				  of the elf image to the position
				  corresponding to next_eip */
  unsigned int curr_sec_index;
  unsigned long nsections;

  sec_mem *sec_info;
/*#endif */

  unsigned char bbCache[BBCACHE_SIZE];	/* Basic-block Translated Code Cache	*/
  bb_entry* lookup_table[LOOKUP_TABLE_SIZE]; /* BBdirectory */
  unsigned long call_hash_table[CALL_TABLE_SIZE];
  bb_entry bb_entry_nodes[MAX_BBS];
  patch_entry patch_array[PATCH_ARRAY_LEN]; /* Patch array that will be filled up by all the
					     emit_jCC's, etc. and used later here for either patching 
					     them right away or for building patch blocks*/

  unsigned char *bbOut;	        /* next output position in BB Code cache 	*/
  unsigned char *bbCache_main;  /* The point beyond which the actual bb's get emitted */
  const unsigned char *bbLimit; /* BB cache limit 					*/

  unsigned char *hash_table;   /* Code hash table to perform indirect jumps */
  unsigned char *chash_table;   /* Code hash table to perform indirect calls */

  unsigned long border_esp;
  unsigned char *jmp_target;

  unsigned char *slow_dispatch_bb;
  unsigned char *sig_dispatch_bb;
  unsigned char *backpatch_and_dispatch_bb;
  unsigned char *fast_dispatch_bb;
  unsigned char *call_calls_fast_dispatch_bb;
  unsigned char *ret_calls_fast_dispatch_bb;
#ifdef SEPARATE_SIEVES
  unsigned char *cfast_dispatch_bb;
  unsigned char *cslow_dispatch_bb;
#endif /* SEPARATE_SIEVES */

  unsigned long int no_of_bbs;

  unsigned long patch_count;	/* No. of patch points encountered in this basic-block */
  unsigned char *backpatch_block;
  unsigned char *patch_point;
  bb_entry *curr_bb_entry;

  sigset_t syscall_sigset;

  unsigned long nTrInstr; /* No. of instructions in the current trace */
                          /* Needed for debugging output, not uptodate 
			     unless using DEGUB flag */
  
  unsigned long this_bbStart;
  unsigned long this_start_eip;
  bool comming_from_call_indirect;

  unsigned long temp;
  pt_state *ptState;
  machine_t *prevM; // Parent Mstate
};

/* Examining the next byte in the istream can lead to any number of
   faults, including page fault, segmentation violation, or wrong
   phase of the moon. In these cases, istream_peekByte returns a value
   greater than 255 (currently always ISTREAM_FAULT, but I may extend
   this). This is a signal to the decoder to abandon its attempt to
   decode the current instruction. */

#define ISTREAM_FAULT 256

#ifdef SUPERVISOR_MODE
#define IS_IFAULT(b) ((b) > 255u)
#else
#define IS_IFAULT(b) (0)
#endif

#ifndef STATIC_PASS
#define istream_peekByte(M)  	(* ((unsigned char *) M->next_eip)) 
#define istream_peekWord(M)    	(*((unsigned short *)M->next_eip))
#define istream_peekLong(M)    	(*((unsigned long *)M->next_eip))

#define istream_nextByte(M)  	((M->next_eip)++)
#define istream_nextWord(M)  	M->next_eip = (unsigned long) (((unsigned short *)M->next_eip) + 1)
#define istream_nextLong(M)  	M->next_eip = (unsigned long ) (((unsigned long *)M->next_eip) + 1)

#else

#define istream_peekByte(M)  	(*((unsigned char *) M->mem_next_eip)) 
#define istream_peekWord(M)    	(*((unsigned short *)M->mem_next_eip))
#define istream_peekLong(M)    	(*((unsigned long *)M->mem_next_eip))

#define istream_nextByte(M)  	do {                         \
                                     (M->next_eip)++;        \
                                     (M->mem_next_eip)++;    \
                                 }while(0) 
#define istream_nextWord(M)  	do{ \
                                      M->next_eip = (unsigned long) (((unsigned short *)M->next_eip) + 1); \
                                      M->mem_next_eip = (unsigned long) (((unsigned short *)M->mem_next_eip) + 1); \
			          }while(0)
#define istream_nextLong(M)  	do { \
                                      M->next_eip = (unsigned long ) (((unsigned long *)M->next_eip) + 1); \
                                      M->mem_next_eip = (unsigned long ) (((unsigned long *)M->mem_next_eip) + 1); \
				  }while(0)
#endif

#ifndef offsetof
#define offsetof(type,field) ((unsigned long)  &((type *)0)->field)
#endif

#define MFLD(M,nm) (((unsigned long)M) + offsetof(machine_t,nm))
#define MREG(M,nm) MFLD(M,fixregs.nm)

extern void mach_showregs(char c, machine_t *M);

#ifdef SIGNALS
#if 0
static inline bool
signals_pending(machine_t *M)
{
  return (M->sigQfront > 0);
}

static inline bool
sigQfull(machine_t *M)
{
  return (M->sigQfront >= SIGQ_DEPTH);
}
#endif
#endif
#endif /* MACHINE_H */
