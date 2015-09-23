#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define MAX_CLINE 512

void bin_exit();

int token_num(char *str);

int main(int argc, char *argv[]) {
  printf("Mysh gets runnning :)\n");
  
  while (1) {
    printf("mysh # ");
    char rc_str[MAX_CLINE];
    fgets(rc_str, MAX_CLINE, stdin);
    char *pch = strtok(rc_str, " ");
    if (pch != NULL) {
      int rc = fork();
      if (rc == 0) {
	// child
	printf("Child process: %d\n", (int) getpid());
	int num_of_token = token_num(pch);
	char *rc_argv[num_of_token];
	// TODO(byan23): 
	// 1. Insert "/bin/" before the first token if the cmd is
	// neither "exit" nor "history".
	// 2. Fix the bug: not back to print "mysh # ".
	rc_argv[0] = strdup(pch);
	int i = 1;
	while ((pch = strtok(NULL, " ")) != NULL) {
	  assert(i < num_of_token);
	  rc_argv[i] = strdup(pch);
	  ++i;
	}
	//printf("******%d\n", num_of_token);
	assert(i == num_of_token);
	execvp(rc_argv[0], rc_argv);
	perror("Exec failure!\n");
	bin_exit();
      } else if (rc > 0){
	// parent
	printf("Parent process: %d\n", (int) getpid());
      } else {
	// failure
	perror("Fork failure!\n");
      }
    }
  }
  return 0;
}

void bin_exit() {
  exit(0);
}

// TODO(byan23): Implement history built-in cmd.

// It is caller's responsibility to ensure 'str' is NOT null and point at a
// non-whitespace character.
int token_num(char *str) {
  assert(str != NULL);
  int count = 1;
  char *pch = str;
  while ((pch = strchr(pch, ' ')) != NULL) {
    ++pch;
    ++count;
  }
  return count;
}
