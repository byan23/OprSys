#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"
#include "fcntl.h"

static void printpinfo(struct pstat *st, int fd) {
  if (!st) {
    printf(1, "NULL pstat ptr.\n");
    return;
  }
  int i, j;
  printf(fd, "pid: %d\n", getpid());
  printf(fd, "pid, priority, pri0, pri1, pri2, pri3\n");
  for (i = 0; i < NPROC; ++i) {
    if (st->inuse[i]) {
      printf(fd, "%d, %d", st->pid[i], st->priority[i]);
      for (j = 0; j < 4; ++j) {
        printf(fd, " ,%d", st->ticks[i][j]);
      }
      printf(fd, "\n");
    }
  }
}

int main (int argc, char *argv[]) {
  
  struct pstat st;
  getpinfo(&st);
  printpinfo(&st, 1);
  int i;
  int x = 0;
  for (i = 0; i < 1000; ++i) x += i;
  getpinfo(&st);
  printpinfo(&st, 1);
  
  int pd = fork();
  if (pd == 0) {
    // child
    getpinfo(&st);
    printpinfo(&st, 1);
    //sleep(3);
    int counter = 50000;
    int i = 0;
    while (counter > 0) {
      if (counter % 1000 == 0) {
	getpinfo(&st);
	printpinfo(&st, 1);
	++i;
	if (i > 7) exit();
      }
      --counter;
    }
  } else if (pd > 0) {
    //getpinfo(&st);
    //printpinfo(&st, 1);
    wait();
    getpinfo(&st);
    printpinfo(&st, 1);
    int counter = 5000;
    int i = 0;
    while (counter > 0) {
      if (counter % 100 == 0) {
	getpinfo(&st);
	printpinfo(&st, 1);
	++i;
	if (i > 5) exit();
      }
      --counter;
    }
  } else {
    printf(1, "fork failure.\n");
  }
  exit();
}
