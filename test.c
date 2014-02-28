#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char* argv[])
{
  pid_t pid;
  int status;

  //fprintf(stderr, "hello, world!\n");
  //system("ls");
  system("cat \"it is run!\" > run.txt");
  pid = fork();
  if (pid == 0) {
    execv("/bin/ls", argv);
  } else {
    waitpid(pid, &status, 0);
  }
  return 0;
}
