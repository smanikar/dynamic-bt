#ifndef DEBUG_H
#define DEBUG_H
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "switches.h"

#define debug_show_start_addr        0x0000001u     
#define debug_sigsegv                0x0000002u
#define debug_startup                0x0000004u
#define debug_bb_directory           0x0000008u
#define debug_input_scan             0x0000010u
#define debug_decode                 0x0000020u
#define debug_lookup                 0x0000040u
#define debug_inline_emits           0x0000080u
#define debug_emits                  0x0000100u
#define debug_dump                   0x0000200u
#define debug_call_ret_opt           0x0000400u
#define debug_static_pass_addr_trans 0x0000800u
#define debug_static_pass_trans      0x0001000u
#define debug_xlate                  0x0002000u  
#define debug_xlate_pb               0x0004000u
#define debug_show_each_instr_trans  0x0008000u
#define debug_show_each_trans_instr  0x0010000u
#define debug_dump_load              0x0020000u
#define debug_thread_init            0x0080000u 
#define debug_signal_registry        0x0100000u
#define debug_signal_note            0x0200000u
#define debug_signal_capture         0x0400000u
#define debug_signal_delivery        0x0800000u
#define debug_decode_eager_panic     0x1000000u
#define debug_show_all_syscalls      0x2000000u
#define debug_thread_exit            0x4000000u
#define debug_sig_init_exit          0x8000000u

#define debug_all                   0xFFFFFFFFu

/* Following should be an OR of some of the above */
#define debug_flags   ( debug_all)

#define CND_DEBUG(x) ((debug_##x) & (debug_flags))
#define DEBUG(x) if (CND_DEBUG(x))

/* For the sake of Debugging Output */
FILE *DBG;

#endif /* DEBUG_H */
