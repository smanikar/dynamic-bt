#ifndef XLCORE_H
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

#define XLCORE_H

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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define THIRTY_TWO_BIT_INSTR(d)  ( (d->opstate & OPSTATE_DATA32) ? 0x0001u : 0x0000u )
bb_entry * xlate_bb(machine_t *M);

/* The only purpose of these macros is to document the role that
   different functions are taking, but they are useful to have. */
#define TARGET_CALLED
#define ENTRY_POINT

void xlate_for_sieve (machine_t *M);
void xlate_for_patch_block(machine_t *M);
bb_entry *xlate_bb(machine_t *M);
machine_t *init_translator(unsigned long program_start);
machine_t *init_thread_trans(unsigned long program_start);
machine_t *init_signal_trans(unsigned long program_start, machine_t *parentM);

#endif /* XLCORE_H */
