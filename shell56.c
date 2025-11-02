/*
 * file:        shell56.c
 * description: Simple shell implementation for CS 5600 Lab 2
 *              Implements Steps 1-6: Signal handling, builtin commands,
 *              external commands, $? variable expansion, file redirection, and pipes
 *
 * Features implemented:
 * - Step 1: Signal handling (ignore SIGINT in interactive mode)
 * - Step 2: Builtin commands (cd, pwd, exit)
 * - Step 3: External command execution (fork/exec/wait)
 * - Step 4: $? variable expansion
 * - Step 5: File redirection (< and >)
 * - Step 6: Pipeline execution (|)
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

/* Global variable to store exit status for $? variable expansion */
int last_exit_status = 0;

/* Function declarations */
void execute_command(char **tokens, int n_tokens);
int is_builtin_command(char *command);
int execute_builtin(char **tokens, int n_tokens);
void execute_external(char **tokens, int n_tokens);
void expand_dollar_question(char **tokens, int n_tokens);
void execute_with_redirection(char **tokens, int n_tokens);
void execute_pipeline(char **tokens, int n_tokens);

int main(int argc, char **argv)
{
    bool interactive = isatty(STDIN_FILENO); /* see: man 3 isatty */
    FILE *fp = stdin;
    
    /* Step 1: Handle signals - ignore SIGINT in interactive mode */
    if (interactive) {
        signal(SIGINT, SIG_IGN); /* ignore SIGINT=^C so shell doesn't exit */
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

        /* Parse the input line into tokens using the provided parser */
        int n_tokens = parse(line, MAX_TOKENS, tokens, linebuf, sizeof(linebuf));

        /* Step 4: Expand $? variable before executing command
         * This replaces any $? tokens with the string representation
         * of the last command's exit status */
        expand_dollar_question(tokens, n_tokens);

        /* Execute the command if there are any tokens */
        if (n_tokens > 0) {
            execute_command(tokens, n_tokens);
        }
    }

    if (interactive)            /* make things pretty */
        printf("\n");           /* to see why, try deleting this and then quit with ^D */
}

/* Main command execution dispatcher - determines if command is builtin or external */
void execute_command(char **tokens, int n_tokens) {
    if (n_tokens == 0) return; /* Skip empty commands */
    
    if (is_builtin_command(tokens[0])) {
        /* Step 2: Execute builtin commands (cd, pwd, exit) */
        last_exit_status = execute_builtin(tokens, n_tokens);
    } else {
        /* Check for pipe operators first */
        bool has_pipes = false;
        for (int i = 0; i < n_tokens; i++) {
            if (strcmp(tokens[i], "|") == 0) {
                has_pipes = true;
                break;
            }
        }
        
        if (has_pipes) {
            /* Step 6: Execute pipeline of commands */
            execute_pipeline(tokens, n_tokens);
        } else {
            /* Check for redirection operators */
            bool has_redirection = false;
            for (int i = 0; i < n_tokens; i++) {
                if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
                    has_redirection = true;
                    break;
                }
            }
            
            if (has_redirection) {
                /* Step 5: Execute external commands with redirection */
                execute_with_redirection(tokens, n_tokens);
            } else {
                /* Step 3: Execute external commands using fork/exec */
                execute_external(tokens, n_tokens);
            }
        }
    }
}

/* Check if the given command is one of our builtin commands */
int is_builtin_command(char *command) {
    if (strcmp(command, "cd") == 0) return 1;    /* Change directory */
    if (strcmp(command, "pwd") == 0) return 1;     /* Print working directory */
    if (strcmp(command, "exit") == 0) return 1;   /* Exit shell */
    return 0; /* Not a builtin command */
}

/* Step 2: Execute builtin commands (cd, pwd, exit) */
int execute_builtin(char **tokens, int n_tokens) {
    if (strcmp(tokens[0], "cd") == 0) {
        /* cd command - change directory */
        char *target_dir;
        
        if (n_tokens == 1) {
            /* No arguments - chdir to HOME directory */
            target_dir = getenv("HOME");
            if (target_dir == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        } else if (n_tokens == 2) {
            /* One argument - chdir to specified directory */
            target_dir = tokens[1];
        } else {
            /* Too many arguments */
            fprintf(stderr, "cd: too many arguments\n");
            return 1;
        }
        
        if (chdir(target_dir) != 0) {
            fprintf(stderr, "cd: %s\n", strerror(errno));
            return 1;
        }
        return 0;
    } else if (strcmp(tokens[0], "pwd") == 0) {
        /* pwd command - print current directory */
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
        /* Handle exit command - terminate shell */
        if (n_tokens > 2) {
            fprintf(stderr, "exit: too many arguments\n");
            return 1;
        }
        if (n_tokens == 1) {
            exit(0); /* Exit with status 0 */
        } else {
            exit(atoi(tokens[1])); /* Exit with specified status */
        }
    }
    return 0;
}


/* Step 3: Execute external commands using fork/exec/wait pattern */
void execute_external(char **tokens, int n_tokens) {
    pid_t pid = fork();
    
    if (pid == 0) {
        /* Child process - restore SIGINT to default behavior for the command */
        signal(SIGINT, SIG_DFL);
        
        /* Execute the external command */
        execvp(tokens[0], tokens);
        
        /* If execvp returns, there was an error */
        fprintf(stderr, "%s: %s\n", tokens[0], strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        /* Parent process - wait for child to complete */
        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        
        /* Store the exit status for $? variable */
        last_exit_status = WEXITSTATUS(status);
    } else {
        /* Fork failed */
        perror("fork");
        last_exit_status = 1;
    }
}

/* Step 4: Expand $? variable in command tokens before execution
 * 
 * This function implements the $? special variable expansion as specified
 * in the assignment. It replaces any $? tokens with the string representation
 * of the last command's exit status.
 * 
 * Key features:
 * - Uses static buffer to avoid memory management issues
 * - Handles multiple $? occurrences in the same command
 * - Works with both builtin and external commands
 * - Safe null pointer checking
 */
void expand_dollar_question(char **tokens, int n_tokens) {
    static char qbuf[16]; /* Static buffer to hold the string representation of exit status */
    
    /* Convert exit status to string representation */
    snprintf(qbuf, sizeof(qbuf), "%d", last_exit_status);
    
    /* Replace any $? tokens with the actual exit status string */
    for (int i = 0; i < n_tokens; i++) {
        if (tokens[i] != NULL && strcmp(tokens[i], "$?") == 0) {
            tokens[i] = qbuf; /* Point to our static buffer */
        }
    }
}

/* Step 5: Execute external commands with input/output redirection
 * 
 * This function handles < and > redirection operators as specified in the assignment.
 * It uses dup2 system call to redirect stdin/stdout to/from files.
 * 
 * Key features:
 * - Input redirection: < filename (redirects stdin from file)
 * - Output redirection: > filename (redirects stdout to file)
 * - Proper error handling for file operations
 * - File descriptor management to avoid leaks
 */
void execute_with_redirection(char **tokens, int n_tokens) {
    char *input_file = NULL;
    char *output_file = NULL;
    char *clean_tokens[MAX_TOKENS];
    int clean_count = 0;
    
    /* Parse tokens to find redirection operators and build clean command */
    for (int i = 0; i < n_tokens; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            /* Input redirection */
            if (i + 1 < n_tokens) {
                input_file = tokens[i + 1];
                i++; /* Skip the filename */
            }
        } else if (strcmp(tokens[i], ">") == 0) {
            /* Output redirection */
            if (i + 1 < n_tokens) {
                output_file = tokens[i + 1];
                i++; /* Skip the filename */
            }
        } else {
            /* Regular command argument */
            clean_tokens[clean_count++] = tokens[i];
        }
    }
    clean_tokens[clean_count] = NULL;
    
    /* Fork process for command execution */
    pid_t pid = fork();
    
    if (pid == 0) {
        /* Child process - restore SIGINT to default behavior */
        signal(SIGINT, SIG_DFL);
        
        /* Handle input redirection */
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd == -1) {
                fprintf(stderr, "%s: %s\n", input_file, strerror(errno));
                exit(1);
            }
            dup2(fd, 0); /* Redirect stdin to file */
            close(fd);   /* Close original file descriptor */
        }
        
        /* Handle output redirection */
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                fprintf(stderr, "%s: %s\n", output_file, strerror(errno));
                exit(1);
            }
            dup2(fd, 1); /* Redirect stdout to file */
            close(fd);    /* Close original file descriptor */
        }
        
        /* Execute the command */
        execvp(clean_tokens[0], clean_tokens);
        
        /* If execvp returns, there was an error */
        fprintf(stderr, "%s: %s\n", clean_tokens[0], strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        /* Parent process - wait for child to complete */
        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        
        /* Store the exit status for $? variable */
        last_exit_status = WEXITSTATUS(status);
    } else {
        /* Fork failed */
        perror("fork");
        last_exit_status = 1;
    }
}

/* Step 6: Execute pipeline of commands
 * 
 * This function handles | pipe operators as specified in the assignment.
 * It supports up to 4 pipeline stages and uses pipe/dup2 system calls.
 * 
 * Key features:
 * - Supports up to 4 pipeline stages (as per assignment requirements)
 * - Uses pipe() to create communication channels between processes
 * - Uses dup2() to redirect stdin/stdout for each command
 * - Proper file descriptor management to avoid leaks
 * - Waits for all processes and sets status to last command's exit status
 */
void execute_pipeline(char **tokens, int n_tokens) {
    /* Split tokens into separate commands */
    char *commands[5][MAX_TOKENS]; /* Support up to 4 pipeline stages */
    int cmd_count = 0;
    int cmd_tokens[5] = {0};
    
    /* Parse tokens and split by pipe operators */
    int cmd_idx = 0;
    for (int i = 0; i < n_tokens; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            commands[cmd_idx][cmd_tokens[cmd_idx]] = NULL;
            cmd_idx++;
            cmd_count++;
        } else {
            commands[cmd_idx][cmd_tokens[cmd_idx]] = tokens[i];
            cmd_tokens[cmd_idx]++;
        }
    }
    commands[cmd_idx][cmd_tokens[cmd_idx]] = NULL;
    cmd_count++;
    
    /* Check pipeline stage limit */
    if (cmd_count > 4) {
        fprintf(stderr, "Too many pipeline stages (max 4)\n");
        last_exit_status = 1;
        return;
    }
    
    /* Create pipes for communication between processes */
    int pipes[3][2]; /* Support up to 3 pipes (4 commands) */
    pid_t pids[4];
    
    /* Create all necessary pipes */
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            last_exit_status = 1;
            return;
        }
    }
    
    /* Fork processes for each command in the pipeline */
    for (int i = 0; i < cmd_count; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) {
            /* Child process - restore SIGINT to default behavior */
            signal(SIGINT, SIG_DFL);
            
            /* Set up input redirection (except for first command) */
            if (i > 0) {
                dup2(pipes[i-1][0], 0); /* Redirect stdin from previous pipe */
            }
            
            /* Set up output redirection (except for last command) */
            if (i < cmd_count - 1) {
                dup2(pipes[i][1], 1); /* Redirect stdout to next pipe */
            }
            
            /* Close all pipe file descriptors in child (after dup2) */
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            /* Execute the command */
            execvp(commands[i][0], commands[i]);
            
            /* If execvp returns, there was an error */
            fprintf(stderr, "%s: %s\n", commands[i][0], strerror(errno));
            exit(EXIT_FAILURE);
        } else if (pids[i] < 0) {
            /* Fork failed */
            perror("fork");
            last_exit_status = 1;
            return;
        }
    }
    
    /* Close all pipe file descriptors in parent process */
    /* After forking all children, parent should close all pipe fds
     * since children have already dup2'd what they need */
    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);  /* Close read end */
        close(pipes[i][1]);  /* Close write end */
    }
    
    /* Wait for all processes to complete */
    for (int i = 0; i < cmd_count; i++) {
        int status;
        do {
            waitpid(pids[i], &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        
        /* Set status to the last command's status (as per assignment) */
        if (i == cmd_count - 1) {
            last_exit_status = WEXITSTATUS(status);
        }
    }
}