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

#include <stdio.h>
#include "switches.h"
#include "debug.h"
#include "machine.h"

#if 0
register unsigned long EAX asm("eax") __attribute_used__;
register unsigned long EBX asm("ebx") __attribute_used__;
register unsigned long ECX asm("ecx") __attribute_used__;
register unsigned long EDX asm("edx") __attribute_used__;
register unsigned long EBP asm("ebp") __attribute_used__;
register unsigned long ESP asm("esp") __attribute_used__;
register unsigned long ESI asm("esi") __attribute_used__;
register unsigned long EDI asm("edi") __attribute_used__;
#endif

void
mach_showregs(char c, machine_t *M)
{
  printf("%c0x%08x:  eax 0x%08x  ebx 0x%08x  ecx 0x%08x  edx 0x%08x\n"
	 "%c0x%08x:  esi 0x%08x  edi 0x%08x  ebp 0x%08x  esp 0x%08x\n",
	 c,
	 M->fixregs.eip,
	 M->fixregs.eax, M->fixregs.ebx, M->fixregs.ecx, M->fixregs.edx,
	 c,
	 M->fixregs.eip,
	 M->fixregs.esi, M->fixregs.edi, M->fixregs.ebp, M->fixregs.esp);
  fflush(stdout);
}

