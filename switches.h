
#ifndef SWITCHES_H
#define SWITCHES_H
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


/********************************************************/
/*                Functionality                         */
/********************************************************/

/* Multi-threading */
#define THREADED_XLATE

/* Signal-handling */
#define SIGNALS

/********************************************************/
/*                  Optimizations                       */
/********************************************************/

/* Turn on Return cache optimizations*/
#define CALL_RET_OPT

/* Build BBHeaders for Conditional Jumps: This will also
   avoid code-duplication if (straight line) target has already been 
   translated */

#define BUILD_BBS_FOR_JCOND_TRACE 

/* Turn on sieve Optimization */
#define USE_SIEVE

#ifdef USE_SIEVE
/* Condition code optimizations for the sieve */
#define SIEVE_WITHOUT_PPF

/* Separate Sieves for call indirect and all others */
/* Currently only supported when SIEVE_WITHOUT_PPF is used */
#define SEPARATE_SIEVES 

/* Use half of the original size of hash table if using
   SEPARATE_SIEVES */
#define SMALL_HASH
#endif /* USE_SIEVE */

/********************************************************/
/*              Profiling Options                       */
/********************************************************/

/* Do Profile Measurements */
//#define PROFILE             // Profile information in stat file
//#define PROFILE_RET_MISS   
//#define PROFILE_BB_CNT
//#define PROFILE_BB_STATS  // BB profile in bbstats file, 
                            // DO NOT Use in conjunction 
			    // with PROFILE flag.
//#define PROFILE_BB_STATS_DISASM  //    ,, 
//#define PROFILE_TRANSLATION
 
/* Output the Basic Block directory in the end */
//#define OUTPUT_BB_STAT

/* Note: All profiling measurements involving counters are
   single-threaded versions. */

/*********************************************************/
/*               Hybrid Translation                      */
/*********************************************************/

/* Use Static Pre-warming by dumping whole M-State */
//#define USE_STATIC_DUMP 

/* Check for existance of /tmp/vdebug-dump? 
    used with STATIC_DUMP option */
/* #define CHECK_DUMP_DIR */

/* Static Trace Generation Pass */
/* This option should almost always be provided from the Makefile */
/* This onl generates traces given a set of entry
   points. Use of these traces for execution after relocation is not
   supported. */
/*#define STATIC_PASS*/

/*******************************************************/
/*               Extra Debugging support               */          
/*******************************************************/

/* #define DEBUG_ON */
/* #define DEBUG_PRN printf */

/******************************************************/
/*                DEPRICATED                          */
/*****************************************************/

/* Enable static mode: When used for disassembly 
   but not execution -- currently unused*/
/* #define STATIC_MODE_SUPPORT */

/* #define 16_BIT_SUPPORT */

/* Use relative Hashes for bb-directory (should not be necessary) */
/* #define USE_DIFF_HASH	*/

/* Enable Relocation -- used with STATIC_PASS */
/* #define RELOC */

/* Turn on Condition-code Optimizations */
/* #if (defined(CALL_DISP_RET_OPT) || defined(CALL_MEM_RET_OPT)) */
/* #define CALL_DISP_CC_OPT */
/* #endif */

/* #define TOUCH_RELOADED_BBCACHE_PAGES */

#endif /* SWITCHES_H */
