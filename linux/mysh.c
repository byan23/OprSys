#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CLINE 512
// whitespace
#define WS " \n\t"
// TODO(byan23): Maybe add reallocation for cases larger than MAX_TOKEN or
// eventially come up with a dynamical way to allocate the mem.
#define MAX_TOKEN 50
#define HISTORY_POOL_SIZE 20

typedef enum CMD_MODE {
  EXIT_MODE	    = 1,
  HISTORY_MODE	    = 2,
  RUN_HISTORY_MODE  = 3,
  RUN_BIN_MODE	    = 4,
  SYN_ERR	    = 5,
  NULL_MODE	    = 6
} CMD_MODE;

// whatever error...
static char error_message[30] = "An error has occurred\n";
#define err_len 30
// The list of history cmds.
char *his_list[HISTORY_POOL_SIZE];
static int his_num = 0;
// current latest cmd index
static int oldest_idx = 0;

// Function for built-in cmd 'exit'.
void bin_exit();
// Function for built-in cmd 'history'.
void bin_history();
// Function for '!' cmd.
void run_his_cmd(char *str);
// Function to parse and exec built-in cmd.
void exec_cmd(char *str);

// helper functions that (some) may be deleted (sadly) later...
void print_args(char *args[]);
void add_to_history(const char *str);
int token_num(char *str, char c);
// Passes in the whole cmd string, returns status including syntax_err mode.
// TODO(byan23): Try to combine checking mode and tokenizing.
CMD_MODE get_mode(const char *s);


// Main for shell, syntax errors should be checked before calling any
// command.
int main(int argc, char *argv[]) {
  char rc_str[MAX_CLINE];
  while (1) {
    printf("mysh # ");
    memset(rc_str, 0, MAX_CLINE);
    fgets(rc_str, MAX_CLINE, stdin);
    // Switch mode.
    CMD_MODE mode = get_mode(rc_str);
    printf("Got mode: %d\n", mode); 
    if (mode != NULL_MODE) {
      if (mode == SYN_ERR) {
	printf("Syntax error!\n");
	write(STDERR_FILENO, error_message, err_len);
      } else {
	// Adds to history.
	printf("Adding cmd to history before doing anything.\n");
	add_to_history(rc_str);
	if (mode == EXIT_MODE) {
	  bin_exit();
	} else {
	  // Parse the cmd.
	  // TODO(byan23): Made it a dynamically allocated variable.
	  printf("a mode we need to do something...\n");
	  switch (mode) {
	    case HISTORY_MODE:
	      printf("his mode\n");
	      bin_history();
	      break;
	    case RUN_HISTORY_MODE:
	      printf("run his mode\n");
	      run_his_cmd(rc_str);
	      break;
	    case RUN_BIN_MODE:
	      printf("I just parse and pass...\n");
	      exec_cmd(rc_str);
	      break;
	    default:
	      printf("Unexpected mode: %d\n", mode);
	      break;
	  }
	}
      }
    }
  }
  return 0;
}

void bin_exit() {
  exit(0);
}

// TODO(byan23): Dynamically allocate char *argv[].
void exec_cmd(char *str) {
  char *rc_argv[MAX_TOKEN];
  str = strtok(str, WS);
  rc_argv[0] = strdup("/bin/");
  strcat(rc_argv[0], str);
  int t_idx = 1;
  while ((str = strtok(NULL, WS)) != NULL) {
    rc_argv[t_idx] = strdup(str);
    ++t_idx;
  }
  rc_argv[t_idx] = NULL;
  print_args(rc_argv);
  int rc = fork();
  if (rc == 0) {
    // child
    execvp(rc_argv[0], rc_argv);
    perror("Exec failure.\n");
    bin_exit();
  } else if (rc > 0) {
    // parent
    wait(NULL);
    printf("Parent process: %d\n", (int) getpid());
  } else {
    // failure
    perror("Fork failure.\n");
  }
}

// TODO(byan23): Implement history built-in cmd.
void bin_history() {
  return;
}

void run_his_cmd(char *str) {
  return;
}

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
  while (!(p = strchr(p, ' '))) {
    ++p;
    ++count;
  }
  return count;
}

void add_to_history(const char *str) {
  if (his_num < HISTORY_POOL_SIZE) {
    // Appends new cmd at the end of history array.
    printf("History pool is not full yet.\n");
    his_list[his_num++] = strdup(str);
    /* oldest_idx stay at 0 */
  } else {
    strcpy(his_list[oldest_idx], str);
    oldest_idx = (oldest_idx + 1) % HISTORY_POOL_SIZE;
  }
}

void print_args(char *args[]) {
  int i;
  printf("Printing args:");
  for (i = 0; args[i] != NULL; ++i) {
    printf(" %s", args[i]);
  }
  printf("\n");
}

CMD_MODE get_mode(const char *s) {
  // Works on a copy, since strtok changes the original string.
  printf("Getting mode...\n");
  char str[strlen(s) + 1];
  strcpy(str, s);
  printf("made a copy...\n");
  char *p;
  if ((p = strstr(str, ">"))) {
    printf("command needs redirection...\n");
    // can only have one '>'.
    if (strchr(++p, '>')) return SYN_ERR;
    // Checks if more than 1 arg appear after '>'.
    char str_cpy[strlen(str) + 1];
    strcpy(str_cpy, str);
    char *start = strchr(str_cpy, '>');
    start = strtok(++start, WS);
    if (strtok(NULL, WS)) {
      printf("Invalid command: %s\n", str);
      return SYN_ERR;
    }
  }
  p = strtok(str, WS);
  printf("1st token: %s\n", p);
  if (!p) return NULL_MODE;	// empty command
  if (p[0] == '>') {
    return SYN_ERR;
  } else if (p[0] == '!') {
    char *pleft;
    long temp = strtol(&p[1], &pleft, 10);
    printf("Use it since you are not happy...%lu\n", temp);
    if (*pleft == '\0' || p[1] == '\0') return RUN_HISTORY_MODE;
    else return SYN_ERR;
  } else if (strlen(p) > 4 && strncmp(p, "exit", 4) == 0) {
    if (p[4] == '\0' && !strtok(NULL, WS)) return EXIT_MODE;
    else return SYN_ERR;
  } else if (strlen(p) > 7 && strncmp(p, "history", 7) == 0) {
    if (p[7] == '\0' && !strtok(NULL, WS)) return HISTORY_MODE;
    else return SYN_ERR;
  } else {
    printf("built-in mode\n");
    return RUN_BIN_MODE;
  }
}
