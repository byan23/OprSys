#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#define MAX_CLINE 513
#define MAX_BUFFER MAX_CLINE + 5
// whitespace
#define WS " \n\t"
// prompt
#define PROMPT "mysh # "
// TODO(byan23): Maybe add reallocation for cases larger than MAX_TOKEN or
// eventially come up with a dynamical way to allocate the mem.
#define MAX_TOKEN 50
#define HIS_POOL_SIZE 20

typedef enum CMD_MODE {
  EXIT_MODE	    = 1,
  HIS_MODE	    = 2,
  RUN_HIS_MODE	    = 3,
  RUN_BIN_MODE	    = 4,
  SYN_ERR	    = 5,
  NULL_MODE	    = 6
} CMD_MODE;

// whatever error...
static char error_message[30] = "An error has occurred\n";
#define err_len strlen(error_message) 
// The list of history cmds.
char *his_list[HIS_POOL_SIZE];
// NOT an idx.
static int his_num = 0;

// Function for built-in cmd 'exit'.
void bin_exit();
// Function for built-in cmd 'history'.
void bin_history();
// Function for '!' cmd.
void run_his_cmd(char *str, char **tokens, int *flag);
// Function to parse and exec built-in cmd.
void exec_cmd(char **tokens);

// helper functions that (some) may be deleted (sadly) later...
char **tokenize(const char *str);
void print_args(char **args);
void add_to_history(const char *str);
// Passes in the tokenized cmd, returns status including syntax_err mode.
CMD_MODE get_mode(char **tokens);

// Main for shell, syntax errors should be checked before calling any
// command.
// Note that for batch mode, once it reaches the end of the batch file, the
// batch file stream is set to NULL, and thus jumps out of batch mode.
int main(int argc, char *argv[]) {
  //printf("# of args: %d\n", argc); 
  if (argc > 2) {
    //perror("argc err\n");
    write(STDERR_FILENO, error_message, err_len);
    exit(1);
  }
  char rc_str[MAX_BUFFER];
  int his_argv = 0;
  FILE *fs = stdin;
  if (argc == 2) {
    fs = fopen(argv[1], "r");
    if (!fs) {
      //perror("fopen err\n");
      write(STDERR_FILENO, error_message, err_len);
      exit(1);
    }
  }
  while (1) {
    // In run_his_mode, don't get new cmd from input.
    if (!his_argv) {
      memset(rc_str, 0, MAX_BUFFER);
      if (fs == stdin) write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
      // 1 for '\0', 1 for length overflow.
      if (!fgets(rc_str, MAX_BUFFER, fs)) {
	fclose(fs);
	exit(0);
      }
      if (strlen(rc_str) > MAX_CLINE) {
	//char err[10];
	//sprintf(err, "%d", (int)strlen(rc_str));
	write(STDERR_FILENO, error_message, err_len);
	rc_str[MAX_CLINE - 1] = '\0';
	write(STDOUT_FILENO, rc_str, strlen(rc_str));
	write(STDOUT_FILENO, "\n", 1);
	/*while (rc_str[strlen(rc_str) - 1] != '\n') {
	  fgets(rc_str, MAX_BUFFER, fs);
	  printf("%s", rc_str);
	}
	printf("\n!!%s", rc_str);*/
	continue;
      }
      if (fs != stdin) write(STDOUT_FILENO, rc_str, strlen(rc_str));
    }
    his_argv = 0;
    // Switch mode.
    char **tokens = tokenize(rc_str);
    //print_args(tokens);
    CMD_MODE mode = get_mode(tokens);
    // printf("Got mode: %d\n", mode); 
    if (mode != NULL_MODE) {
      if (mode == SYN_ERR) {
	//perror("Syntax error!\n");
	write(STDERR_FILENO, error_message, err_len);
      } else {
	// Adds to history.
	//printf("Adding cmd to history before doing anything.\n");
	if (mode != RUN_HIS_MODE) add_to_history(rc_str);
	if (mode == EXIT_MODE) {
	  bin_exit();
	} else {
	  //printf("a mode we need to do something...\n");
	  switch (mode) {
	    case HIS_MODE:
	      //printf("his mode\n");
	      bin_history();
	      break;
	    case RUN_HIS_MODE:
	      //printf("run his mode\n");
	      run_his_cmd(rc_str, tokens, &his_argv);
	      break;
	    case RUN_BIN_MODE:
	      //printf("I just parse and pass...\n");
	      exec_cmd(tokens);
	      break;
	    default:
	      //printf("Unexpected mode: %d\n", mode);
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
void exec_cmd(char **tokens) {
  //print_args(tokens);
  int rc = fork();
  if (rc == 0) {
    // child
    int i;
    char *redir_ptr = NULL;
    for (i = 0; tokens[i] != NULL; ++i) {
      //printf("%s\n", tokens[i]);
      if ((redir_ptr = strstr(tokens[i], ">"))) {
	//printf("redirecting to...\n");
	break;
      }
    } 
    if (redir_ptr) {
      //printf("redorecting ...\n");
      char *output;
      if (strlen(redir_ptr) == 1) {
	output = tokens[i+1];
      } else {
	output = redir_ptr + 1;
      }
      close(STDOUT_FILENO);
      int fd = open(output, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
      if (fd < 0) {
	//perror("file open err\n");
	write(STDERR_FILENO, error_message, err_len);
	return;
      }
      tokens[i] = NULL;
    }
    execvp(tokens[0], tokens);
    //perror("exec err\n");
    write(STDERR_FILENO, error_message, err_len);
    bin_exit();
  } else if (rc > 0) {
    // parent
    wait(NULL);
    //printf("Parent process: %d\n", (int) getpid());
  } else {
    // failure
    //write(STDERR_FILENO, error_message, err_len);
  }
}

void bin_history() {
  int i = his_num > HIS_POOL_SIZE ? (his_num - HIS_POOL_SIZE + 1) : 1;
  char buffer[MAX_CLINE];
  for (; i <= his_num; ++i) {
    sprintf(buffer, "%d %s", i, his_list[(i - 1) % HIS_POOL_SIZE]);
    write(STDOUT_FILENO, buffer, strlen(buffer));
  }
}

// TODO(byan23): Make sure Check mode ensures that nothing else than whitespace
// appears before '!'.
// Note that nothing is added to history under this mode, history of the
// corresponding cmd will be added in the next loop round.
// 'flag' is passed in as to notify the main outer loop whether a history cmd
// needs to run (false when '!' cmd invalid).
void run_his_cmd(char* str, char **tokens, int *flag) {
  int idx;
  //printf("Expo cmd: %s\n", str);
  if (!tokens[1] && strlen(tokens[0]) == 1) {
    //printf("Last history...\n");
    idx = (his_num - 1) % HIS_POOL_SIZE;
  } else {
    long num;
    if (!tokens[1]) num = strtol(&(tokens[0][1]), NULL, 10);
    else	    num = strtol(tokens[1], NULL, 10);
    // history number out of current bound
    if (num > his_num || num < his_num - HIS_POOL_SIZE + 1 || num <= 0) {
      //perror("out of bounds err\n");
      write(STDERR_FILENO, error_message, err_len);
      return;  
    } else {
      idx = ((int)num - 1) % HIS_POOL_SIZE;
    }
    //printf("%lu history at index: %d\n", num, idx);
  }
  *flag = 1;
  memset(str, 0, MAX_BUFFER);
  strcpy(str, his_list[idx]);
}

// Returns NULL if empty/whitespace str.vtokenize(const char *str) {
char **tokenize(const char *str) {
  // work on a copy of 'str'
  char copy[strlen(str) + 1];
  strcpy(copy, str);
  char *c;
  char **result = (char**) malloc (MAX_TOKEN * sizeof(char*));
  if (!(c = strtok(copy, WS))) return NULL;
  result[0] = strdup(c);
  int i;
  for (i = 1; (c = strtok(NULL, WS)) != NULL; ++i) {
    result[i] = strdup(c);
  }
  result[i] = NULL;
  return result;
}

void add_to_history(const char *str) {
  ++his_num;
  if (his_num <= HIS_POOL_SIZE) {
    // Appends new cmd at the end of history array.
    //printf("History pool is not full yet.\n");
    his_list[his_num - 1] = strdup(str);
    /* oldest stays at 0 */
  } else {
    int idx = (his_num - 1) % HIS_POOL_SIZE;
    free(his_list[idx]);
    his_list[idx] = strdup(str);
  }
}

void print_args(char **args) {
  int i;
  printf("Printing args:");
  for (i = 0; args[i] != NULL; ++i) {
    printf(" %s", args[i]);
  }
  printf("\n");
}

CMD_MODE get_mode(char **tokens) {
  // Works on a copy, since strtok changes the original string.
  //printf("Getting mode...\n");
  if (!tokens) return NULL_MODE;  // empty cmd
  //print_args(tokens);
  char *p;
  int i;
  for (i = 0; tokens[i] != NULL; ++i){
    if ((p = strchr(tokens[i], '>'))) {
      // printf("command needs redirection...\n");
      // can only have one '>'.
      // can have one or no tokens afterwards
      if (strlen(p) == 1) {
	if (!tokens[i+1] || (p = strchr(tokens[i+1], '>')) ||
	    tokens[i+2]) return SYN_ERR;
      } else {
	assert(strlen(p) > 1);
	if ((p = strchr(++p, '>')) || tokens[i+1]) return SYN_ERR;
      }
    }
  }
  p = tokens[0];
  //printf("1st token: %s\n", p);
  if (p[0] == '>') {
    return SYN_ERR;
  } else if (p[0] == '!') {
    char *pleft;
    strtol(&p[1], &pleft, 10);
    //printf("Use it since you are not happy...%lu\n", temp);
    if ((p[1] == '\0' || *pleft == '\0') && !tokens[1]) {
      return RUN_HIS_MODE;
    } else {
      return SYN_ERR;
    }
  } else if (strlen(p) >= 4 && strncmp(p, "exit", 4) == 0) {
    if (p[4] == '\0' && tokens[1] == NULL) return EXIT_MODE;
    else return SYN_ERR;
  } else if (strlen(p) >= 7 && strncmp(p, "history", 7) == 0) {
    if (p[7] == '\0' && tokens[1] == NULL) return HIS_MODE;
    else return SYN_ERR;
  } else {
    //printf("built-in mode\n");
    return RUN_BIN_MODE;
  }
}
