#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CLINE 512

// TODO(byan23): Maybe add reallocation for cases larger than MAX_TOKEN or
// eventially come up with a dynamical way to allocate the mem.
#define MAX_TOKEN 50
#define HISTORY_POOL_SIZE 20

typedef enum {
  EXIT_MODE	    = 1;
  HISTORY_MODE	    = 2;
  RUN_HISTORY_MODE  = 3;
  RUN_BIN_MODE	    = 4;
  SYNTAX_ERR	    = 5;
} CMD_MODE;

// The list of history cmds.
char *his_list[HISTORY_POOL_SIZE];
int his_num = 0;
// current latest cmd index
int oldest_idx = 0;

// Function for built-in cmd 'exit'.
void bin_exit();
// Function for built-in cmd 'history'.
void bin_history();
// Function for '!' cmd.
void run_his_cmd(int num);
// Function to parse and exec built-in cmd.
void exec_cmd(char *str);

// helper functions that may be deleted (sadly) later...
void print_args(char *args[]);
int token_num(char *str, char c);
// Use the 1st token to get the run mode.
CMD_MODE get_mode(char *str);


// TODO(byan23): Decide whether or not to keep "/bin/" prefix.
// Main for shell, syntax errors should be checked before calling any
// command.
int main(int argc, char *argv[]) {
  char rc_str[MAX_CLINE];
  while (1) {
    printf("mysh # ");
    memset(rc_str, 0, MAX_CLINE);
    fgets(rc_str, MAX_CLINE, stdin);
    char *cmd = strdup(rc_str);
    // Adds into history list, increments 'his_num' if it's not yet full.
    if (his_num < HISTORY_POOL_SIZE) {
      his_list[his_num] = cmd;
      ++his_num;
    } else {
      his_list[oldest_idx] = cmd;
      oldest_idx = (oldest_idx + 1) % HISTORY_POOL_SIZE;
    }
    char *pch = strtok(rc_str, " \n\t");
    if (pch != NULL) {
      //int num_of_token = token_num(rc_str, pch[0]);
      //char *rc_argv[num_of_token + 1];
      // Switch mode.
      CMD_MODE mode = get_mode(pch);
      if (mode == 1) bin_exit();
      else {
	// Parse the cmd.
	char *rc_argv[MAX_TOKEN];
	rc_argv[0] = strdup("/bin/");
	strcat(rc_argv[0], pch);
	int t_idx = 1;
	//printf("%d tokens.\n", num_of_token);
	while ((pch = strtok(NULL, " \n\t")) != NULL) {
	  //assert(t_idx < num_of_token);
	  rc_argv[t_idx] = strdup(pch);
	  ++t_idx;
	}   
	rc_argv[t_idx] = NULL;	
	int rc = fork();
	if (rc == 0) {
	  // child
	  switch (mode) {
	    case 2:
	      bin_history();
	      break;
	    case 3:
	      run_his_cmd();
	      break;
	    case 4:

	  }
	} else if (rc > 0) {
	  // parent
	  wait(NULL);
	  //printf("Parent process: %d\n", (int) getpid());
	} else {
	  // failure
	  perror("Fork failure.\n");
	}
      }
      /*char *rc_argv[MAX_TOKEN];
      rc_argv[0] = strdup("/bin/");
      strcat(rc_argv[0], pch);
      int t_idx = 1;
      while ((pch = strtok(NULL, " \n\t")) != NULL) {
	rc_argv[t_idx] = strdup(pch);
	++t_idx;
      }
      rc_argv[t_idx] = NULL;*/
      //print_args(rc_argv);
      //char *rcargv[] = {"ls", "../", NULL};
      //assert(t_idx == num_of_token);
      int rc = fork();
      if (rc == 0) {
	// child
	//printf("Child process: %d\n", (int) getpid());
	//printf("Exec command: %s\n", rc_argv[0]);
	execvp(rc_argv[0], rc_argv);
	perror("Exec failure!\n");
	bin_exit();
      } else if (rc > 0){
	// parent
	wait(NULL);
	//printf("Parent process: %d\n", (int) getpid());
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

// TODO(byan23): Parsing finished. Finish exec part.
void exec_cmd(char *str) {
  char *rc_argv[MAX_TOKEN];
  rc_argv[0] = strdup("/bin/");
  strcat(rc_argv[0], pch);
  int t_idx = 1;
  while ((pch = strtok(NULL, " \n\t")) != NULL) {
  rc_argv[t_idx] = strdup(pch);
    ++t_idx;
  }
  rc_argv[t_idx] = NULL;
}

// TODO(byan23): Implement history built-in cmd.

// It is caller's responsibility to ensure 'str' is NOT null and 'c' is the
// 1st non-whitespace character.
int token_num(char *str, char c) {
  printf("original string: %s\n", str);
  assert(str != NULL);
  char *p = strchr(str, c);
  printf("First non-whitespace character is: %c\n", p[0]);
  printf("local string: %s\n", p);
  int count = 1;  // 'str' is already pointing at the beginning of 1st token
  ++p;
  while ((p = strchr(p, ' ')) != NULL) {
    ++p;
    ++count;
  }
  return count;
}

void print_args(char *args[]) {
  int i;
  printf("Printing args:");
  for (i = 0; args[i] != NULL; ++i) {
    printf(" %s", args[i]);
  }
  printf("\n");
}

CMD_MODE get_mode(char *str) {
  if (strncmp(str, "exit", 4) == 0) {
    if (str[4] == '\0') return CMD_MODE.EXIT_MODE;
    else return CMD_MODE.SYNTAX_ERR;
  } else if {strncmp(str, "history", 7) == 0} {
    if (str[7] == '\0') return CMD_MODE.HISTORY_MODE;
    else return CMD_MODE.SYNTAX_ERR;
  } else if (str[0] == '!') {
    char *p;
    long temp = strtol(&str[1], &p, 10);
    if (*p == '\0' || str[1] == '\0') return CMD_MODE.RUN_HISTORY_MODE;
    else return CMD_MODE.SYNTAX_ERR;
  } else {
    return CMD_MODE.RUN_BIN_MODE;
  }
}
