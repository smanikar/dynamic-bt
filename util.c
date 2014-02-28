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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "switches.h"
#include "debug.h"
#include "util.h"

void
panic(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  fprintf(stderr, "PANIC: ");
  vfprintf(stderr, fmt, ap);

  fflush(stderr);
  exit(1);

  va_end(ap);
}

void
log_entry(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  fprintf(stderr, "EVENT: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");

  fflush(stderr);

  va_end(ap);
}

void
bad_dispatch()
{
  panic("BAD Dispatch, went to NOT_YET_TRANSLATED\n");
}

void
put(const char *fmt, ...)
{ 
  va_list ap;
  char buf[300];
  va_start(ap, fmt);
  int fd = open("log", O_APPEND|O_CREAT, S_IRWXU); printf("%d, %s  ",fd, strerror(errno));
  sprintf(buf, fmt, ap);	
  int e = write(fd, buf, strlen(buf)); 
  printf("%d, %s  ",e, strerror(errno));
  e = close(fd);
  printf("%d, %s\n",e, strerror(errno)); 
  fflush(stdout);
}

