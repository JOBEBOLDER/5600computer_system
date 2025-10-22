/*
 * file:        shell56.c
 * description: skeleton code for simple shell
 *
 * Peter Desnoyers, Northeastern CS5600 Fall 2025
 */

/* <> means don't check the local directory */ 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

/* " " means check the local directory */
#include "parser.h"

/* you'll need these includes later: */
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>	/* PATH_MAX */

#define MAX_TOKENS 32

/* Global variable to store exit status */
int last_exit_status = 0;

/* Function declarations */
void execute_command(char **tokens, int n_tokens);
int is_builtin_command(char *command);
int execute_builtin(char **tokens, int n_tokens);
void execute_external(char **tokens, int n_tokens);
void expand_dollar_question(char **tokens, int n_tokens);

int main(int argc, char **argv)
{
    bool interactive = isatty(STDIN_FILENO); /* see: man 3 isatty */
    FILE *fp = stdin;
    
    /* Step 1: Handle signals */
    if (interactive) {
        signal(SIGINT, SIG_IGN); /* ignore SIGINT=^C */
    }

    if (argc == 2) {
        interactive = false;
        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
            exit(EXIT_FAILURE); /* see: man 3 exit */
        }
    }
    if (argc > 2) {
        fprintf(stderr, "%s: too many arguments\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char line[1024], linebuf[1024];
    char *tokens[MAX_TOKENS];
    
    /* loop:
     *   if interactive: print prompt
     *   read line, break if end of file
     *   tokenize it
     *   print it out <-- your logic goes here
     */
    while (true) {
        if (interactive) {
            /* print prompt. flush stdout, since normally the tty driver doesn't
             * do this until it sees '\n'
             */
            printf("$ ");
            fflush(stdout);
        }

        /* see: man 3 fgets (fgets returns NULL on end of file)
         */
        if (!fgets(line, sizeof(line), fp))
            break;

        /* read a line, tokenize it, and print it out
         */
        int n_tokens = parse(line, MAX_TOKENS, tokens, linebuf, sizeof(linebuf));

        /* Expand $? variable */
        expand_dollar_question(tokens, n_tokens);

        /* Execute the command */
        if (n_tokens > 0) {
            execute_command(tokens, n_tokens);
        }
    }

    if (interactive)            /* make things pretty */
        printf("\n");           /* to see why, try deleting this and then quit with ^D */
}

/* Execute a command by checking if it's builtin or external */
void execute_command(char **tokens, int n_tokens) {
    if (n_tokens == 0) return;
    
    if (is_builtin_command(tokens[0])) {
        last_exit_status = execute_builtin(tokens, n_tokens);
    } else {
        execute_external(tokens, n_tokens);
    }
}

/* Check if command is a builtin command */
int is_builtin_command(char *command) {
    if (strcmp(command, "cd") == 0) return 1;
    if (strcmp(command, "pwd") == 0) return 1;
    if (strcmp(command, "exit") == 0) return 1;
    return 0;
}

/* Execute builtin commands */
int execute_builtin(char **tokens, int n_tokens) {
    if (strcmp(tokens[0], "cd") == 0) {
        if (n_tokens > 2) {
            fprintf(stderr, "cd: wrong number of arguments\n");
            return 1;
        }
        if (n_tokens == 1) {
            // cd with no arguments - go to home directory
            char *home = getenv("HOME");
            if (home == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
            if (chdir(home) != 0) {
                fprintf(stderr, "cd: %s\n", strerror(errno));
                return 1;
            }
        } else {
            if (chdir(tokens[1]) != 0) {
                fprintf(stderr, "cd: %s\n", strerror(errno));
                return 1;
            }
        }
        return 0;
    } else if (strcmp(tokens[0], "pwd") == 0) {
        if (n_tokens > 1) {
            fprintf(stderr, "pwd: too many arguments\n");
            return 1;
        }
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
            return 0;
        } else {
            perror("pwd");
            return 1;
        }
    } else if (strcmp(tokens[0], "exit") == 0) {
        if (n_tokens > 2) {
            fprintf(stderr, "exit: too many arguments\n");
            return 1;
        }
        if (n_tokens == 1) {
            exit(0);
        } else {
            exit(atoi(tokens[1]));
        }
    }
    return 0;
}


/* Execute external command using fork and exec */
void execute_external(char **tokens, int n_tokens) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - restore SIGINT to default behavior
        signal(SIGINT, SIG_DFL);
        execvp(tokens[0], tokens);
        // If execvp returns, there was an error
        fprintf(stderr, "%s: %s\n", tokens[0], strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        last_exit_status = WEXITSTATUS(status);
    } else {
        // Fork failed
        perror("fork");
        last_exit_status = 1;
    }
}

/* Expand $? variable in tokens */
void expand_dollar_question(char **tokens, int n_tokens) {
    static char qbuf[16];
    snprintf(qbuf, sizeof(qbuf), "%d", last_exit_status);
    
    for (int i = 0; i < n_tokens; i++) {
        if (strcmp(tokens[i], "$?") == 0) {
            tokens[i] = qbuf;
        }
    }
}

