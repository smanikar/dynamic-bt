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
#include <stdbool.h>
#include <signal.h>
//#include <asm/sigcontext.h>
#include <bits/sigcontext.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include "switches.h"
#include "debug.h"
#include "util.h"
#include "machine.h"
#include "decode.h"
#include "emit.h"
#include "xlcore.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <elf.h>
#include <bfd.h>

int main(int argc, char *argv[])
{
  unsigned long prog_start, prog_end;
  char buf[400]; /* Buffer overflow */
  FILE *F;
  unsigned long a, i=0, j=0;
  unsigned long cnt = 0;

  if(argc < 2) {
    printf("Arg(s) Missing\n");
    exit(1);
  }

  system("./clean-dumps.sh");

  sprintf(buf, "nm %s | grep '[0-9]* [A-Z] _start' | awk '{print $1}'\
                > /tmp/vdebug-dump/._start", argv[1]);
  system(buf);
  F = fopen("/tmp/vdebug-dump/._start", "r");
  fscanf(F, "%lx", &prog_start);
  fclose(F);

  sprintf(buf, "nm %s | grep '[0-9]* [A-Z] _end' | awk '{print $1}'\
                > /tmp/vdebug-dump/._end", argv[1]);
  system(buf);
  F = fopen("/tmp/vdebug-dump/._end", "r");
  fscanf(F, "%lx", &prog_end);
  fclose(F);
  printf("_start = %lx _end = %lx\n", prog_start, prog_end);

#if 0
  bfd *abfd = bfd_openr(argv[1], "i686-pc-linux-gnu");
  asection *sec;
  for (sec = abfd->sections; sec != (asection *) NULL; sec = sec->next) {
    		printf("Start = %lx, end = %lx\n", sec->vma, sec->vma + bfd_section_size(abfd, sec) / bfd_octets_per_byte(abfd));
  }
#endif

  sprintf(buf, "../../disasm-0.1/disasm -d -P ../../disasm-0.1/digraphs/istats %s | awk -f grab-blocks.awk > their", argv[1]);
  system(buf);

  sprintf(buf, "./Nvdbg.sh");
  for(i=1; i<argc; i++) {
	strcat(buf, " ");
	strcat(buf, argv[i]);
  }
  system(buf);
 
  F= fopen("bbstat", "r");
  FILE *FF = fopen("our", "w");
  while(fscanf(F, "%lx", &a) != -1) {
    if(a > prog_start && a < prog_end)
 	   fprintf(FF, "%lx\n", a);
  }
  fclose(F);
  fclose(FF);

  //system("sort our | uniq | awk '{if($1 != 0) print $1}'| cat - > our1");
  system("sort our | uniq | awk '{if ($1 != 0) print $1}' | cat - > our1");
  system("mv our1 our");
}
