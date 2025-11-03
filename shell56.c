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

/* 
 * 头文件包含部分
 * 这些头文件提供了我们编写shell程序所需的各种功能
 * 
 * <> 表示从系统标准库中查找文件，不在当前目录查找
 */
// 标准输入输出功能（如printf, fprintf等）
#include <stdio.h>
// 标准库功能（如exit, malloc等）
#include <stdlib.h>
// Unix系统调用（如fork, exec, chdir等）
#include <unistd.h>
// 字符串处理功能（如strcmp, strlen等）
#include <string.h>
// 错误处理功能（如errno, strerror等）
#include <errno.h>
// 布尔类型支持（true/false）
#include <stdbool.h>

/* " " 表示先检查当前目录，再检查系统库 */
// 本地解析器，用于将用户输入的字符串分割成命令和参数
#include "parser.h"

/* 
 * 以下头文件提供系统级功能：
 */
// 系统数据类型（如pid_t等）
#include <sys/types.h>
// 进程等待功能（如waitpid）
#include <sys/wait.h>
// 信号处理功能（如signal, SIGINT等）
#include <signal.h>
// 文件操作功能（如open, O_RDONLY等）
#include <fcntl.h>
// 系统限制常量（如PATH_MAX，表示路径的最大长度）
#include <limits.h>	/* PATH_MAX */

/*
 * 常量定义
 */
// 定义最大token数（token就是命令被分割后的每个单词）
// 例如命令 "ls -l /home" 会被分割成3个token: "ls", "-l", "/home"
#define MAX_TOKENS 32

/* 
 * 全局变量
 * 
 * last_exit_status: 存储上一个命令执行后的退出状态码
 * 这个变量用于支持 $? 特殊变量，用户可以在命令中使用 $? 来获取
 * 上一个命令是否成功执行（0表示成功，非0表示失败）
 */
int last_exit_status = 0;

/* 
 * 函数声明
 * 这些函数在main函数之后定义，所以需要先声明它们的存在
 * 这样main函数才能调用它们
 */
// 主命令执行函数：决定命令是内置命令还是外部命令
void execute_command(char **tokens, int n_tokens);
// 判断一个命令是否是内置命令（shell自己实现的命令）
int is_builtin_command(char *command);
// 执行内置命令（如cd, pwd, exit）
int execute_builtin(char **tokens, int n_tokens);
// 执行外部命令（如ls, cat等系统命令）
void execute_external(char **tokens, int n_tokens);
// 展开 $? 变量（将 $? 替换为实际的上一个命令的退出状态码）
void expand_dollar_question(char **tokens, int n_tokens);
// 执行带重定向的命令（处理 < 和 > 操作符）
void execute_with_redirection(char **tokens, int n_tokens);
// 执行管道命令（处理 | 操作符，如 "ls | grep test"）
void execute_pipeline(char **tokens, int n_tokens);

/*
 * main函数：程序的入口点
 * 
 * 参数说明：
 *   argc: 命令行参数的数量（包括程序名本身）
 *   argv: 命令行参数的数组（argv[0]是程序名，argv[1]是第一个参数，以此类推）
 * 
 * 这个函数负责：
 *   1. 判断shell是交互模式还是批处理模式
 *   2. 设置信号处理（忽略Ctrl+C）
 *   3. 进入主循环，不断读取用户输入并执行命令
 */
int main(int argc, char **argv)
{
    /*
     * 第一步：判断shell的运行模式
     * 
     * interactive: 是否为交互模式
     *   - true: 用户在终端直接输入命令（交互模式）
     *   - false: 从脚本文件读取命令（批处理模式）
     * 
     * isatty()函数检查标准输入是否是一个终端设备
     * 如果用户直接在终端运行shell，返回true；如果是从文件输入，返回false
     */
    bool interactive = isatty(STDIN_FILENO);
    
    /*
     * fp: 文件指针，指向我们要读取命令的来源
     * 默认是stdin（标准输入，通常是键盘），如果是批处理模式，会改为指向文件
     */
    FILE *fp = stdin;
    
    /* 
     * 步骤1：处理信号 - 在交互模式下忽略SIGINT信号
     * 
     * SIGINT信号是当用户按下Ctrl+C时系统发送的信号
     * 在交互模式下，我们希望忽略这个信号，这样用户按Ctrl+C时：
     *   - shell本身不会退出（SIG_IGN表示忽略这个信号）
     *   - 但正在执行的命令仍然会被中断（这是正常行为）
     */
    if (interactive) {
        // SIG_IGN表示忽略SIGINT信号（即Ctrl+C）
        signal(SIGINT, SIG_IGN);
    }

    /*
     * 第二步：处理命令行参数
     * 
     * 如果用户提供了文件名作为参数（argc == 2），说明要执行脚本文件
     * 例如：./shell56 script.txt
     */
    if (argc == 2) {
        // 设置为批处理模式
        interactive = false;
        // 打开用户指定的文件作为输入源
        fp = fopen(argv[1], "r");
        // 如果文件打开失败（文件不存在、权限不足等），打印错误并退出
        if (fp == NULL) {
            fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    
    /*
     * 如果参数太多（超过1个），报错并退出
     * shell只接受0个参数（交互模式）或1个参数（脚本文件）
     */
    if (argc > 2) {
        fprintf(stderr, "%s: too many arguments\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    /*
     * 第三步：准备存储用户输入和解析结果的变量
     * 
     * line: 存储用户输入的整行命令（最多1024个字符）
     * linebuf: 解析器使用的缓冲区（用于存储token的字符串）
     * tokens: 存储解析后的token指针数组（每个token是指向linebuf中某个位置的指针）
     */
    char line[1024], linebuf[1024];
    char *tokens[MAX_TOKENS];
    
    /*
     * 第四步：主循环 - 不断读取和执行命令
     * 
     * 这个循环会一直运行，直到：
     *   - 用户输入EOF（在终端按Ctrl+D，或文件读到末尾）
     *   - 用户执行exit命令退出shell
     */
    while (true) {
        /*
         * 如果处于交互模式，显示提示符 "$ "
         * 
         * 提示符就是shell等待用户输入时显示的符号
         * 类似于你在终端看到 "$ ls" 中的 "$"
         * 
         * fflush(stdout) 的作用是立即将输出刷新到屏幕
         * 因为通常系统会等到遇到换行符才显示，但提示符后面没有换行，
         * 所以需要手动刷新，让用户立即看到提示符
         */
        if (interactive) {
            printf("$ ");
            fflush(stdout);
        }

        /*
         * 读取用户输入的一行命令
         * 
         * fgets()函数从文件指针fp读取一行（最多1023个字符，留一个给字符串结束符'\0'）
         * 如果读到文件末尾（EOF），返回NULL，这时我们break退出循环
         */
        if (!fgets(line, sizeof(line), fp))
            break;

        /*
         * 解析用户输入的命令
         * 
         * parse()函数将用户输入的字符串（如 "ls -l /home"）分割成多个token
         * 例如："ls -l /home" 会被分割成：
         *   tokens[0] = "ls"
         *   tokens[1] = "-l"
         *   tokens[2] = "/home"
         *   n_tokens = 3
         * 
         * 这个函数会处理引号、空格等特殊情况
         */
        int n_tokens = parse(line, MAX_TOKENS, tokens, linebuf, sizeof(linebuf));

        /*
         * 步骤4：展开 $? 变量
         * 
         * 在执行命令之前，我们需要先将命令中所有的 $? 替换为实际的上一个命令的退出状态码
         * 例如：用户输入 "echo $?"，如果上一个命令成功（退出码0），
         * 那么这个函数会将 $? 替换为 "0"，变成 "echo 0"
         */
        expand_dollar_question(tokens, n_tokens);

        /*
         * 如果解析后有token（即用户确实输入了命令，而不是空行），执行命令
         */
        if (n_tokens > 0) {
            execute_command(tokens, n_tokens);
        }
    }

    /*
     * 退出前的美化处理
     * 如果是交互模式，在退出前打印一个换行符，让输出更美观
     * （试试删除这行，然后用Ctrl+D退出，看看有什么区别）
     */
    if (interactive)
        printf("\n");
}

/*
 * execute_command: 命令执行调度器
 * 
 * 这个函数是整个shell的核心，它决定如何执行用户输入的命令
 * 
 * 参数说明：
 *   tokens: 解析后的命令token数组（例如：["ls", "-l", "/home"]）
 *   n_tokens: token的数量（例如：3）
 * 
 * 执行流程：
 *   1. 首先检查是否是内置命令（shell自己实现的命令）
 *   2. 如果不是内置命令，检查是否有管道（|）
 *   3. 如果没有管道，检查是否有重定向（< 或 >）
 *   4. 如果都没有，执行普通的外部命令
 */
void execute_command(char **tokens, int n_tokens) {
    /*
     * 如果没有token（空命令），直接返回，什么都不做
     */
    if (n_tokens == 0) return;
    
    /*
     * 检查第一个token（即命令名）是否是内置命令
     * 
     * 内置命令是shell自己实现的命令，不需要启动新进程
     * 例如：cd（改变目录）、pwd（显示当前目录）、exit（退出shell）
     */
    if (is_builtin_command(tokens[0])) {
        /*
         * 步骤2：执行内置命令
         * 内置命令在shell进程内直接执行，不需要fork新进程
         */
        last_exit_status = execute_builtin(tokens, n_tokens);
    } else {
        /*
         * 如果不是内置命令，就是外部命令（如ls, cat等系统命令）
         * 外部命令需要检查是否有特殊的操作符
         */
        
        /*
         * 第一步：检查是否有管道操作符（|）
         * 
         * 管道允许将一个命令的输出作为另一个命令的输入
         * 例如："ls | grep test" 表示将ls的输出传给grep命令
         */
        bool has_pipes = false;
        // 遍历所有token，查找是否有 "|" 符号
        for (int i = 0; i < n_tokens; i++) {
            if (strcmp(tokens[i], "|") == 0) {
                has_pipes = true;
                break; // 找到一个就够了，可以退出循环
            }
        }
        
        if (has_pipes) {
            /*
             * 步骤6：执行管道命令
             * 管道是最复杂的，因为需要创建多个进程并通过管道连接它们
             */
            execute_pipeline(tokens, n_tokens);
        } else {
            /*
             * 第二步：检查是否有重定向操作符（< 或 >）
             * 
             * < 表示输入重定向：将文件内容作为命令的输入
             *   例如："cat < file.txt" 表示从file.txt读取内容
             * > 表示输出重定向：将命令的输出写入文件
             *   例如："ls > output.txt" 表示将ls的输出写入output.txt
             */
            bool has_redirection = false;
            // 遍历所有token，查找是否有 "<" 或 ">" 符号
            for (int i = 0; i < n_tokens; i++) {
                if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
                    has_redirection = true;
                    break;
                }
            }
            
            if (has_redirection) {
                /*
                 * 步骤5：执行带重定向的命令
                 * 需要打开文件，并将标准输入或标准输出重定向到文件
                 */
                execute_with_redirection(tokens, n_tokens);
            } else {
                /*
                 * 步骤3：执行普通的外部命令
                 * 这是最简单的情况：直接fork一个新进程，执行命令
                 */
                execute_external(tokens, n_tokens);
            }
        }
    }
}

/*
 * is_builtin_command: 检查命令是否是内置命令
 * 
 * 参数说明：
 *   command: 命令名字符串（例如："cd", "pwd", "exit"）
 * 
 * 返回值：
 *   1: 是内置命令
 *   0: 不是内置命令（是外部命令）
 * 
 * 内置命令是shell自己实现的命令，不需要启动外部程序
 * 我们的shell实现了三个内置命令：
 *   - cd: 改变当前工作目录
 *   - pwd: 打印当前工作目录
 *   - exit: 退出shell程序
 */
int is_builtin_command(char *command) {
    // strcmp函数比较两个字符串，如果相等返回0
    if (strcmp(command, "cd") == 0) return 1;    // cd命令：改变目录
    if (strcmp(command, "pwd") == 0) return 1;   // pwd命令：显示当前目录
    if (strcmp(command, "exit") == 0) return 1;  // exit命令：退出shell
    return 0; // 不是内置命令，返回0表示这是外部命令
}

/*
 * execute_builtin: 执行内置命令
 * 
 * 参数说明：
 *   tokens: 解析后的命令token数组
 *   n_tokens: token的数量
 * 
 * 返回值：
 *   0: 命令执行成功
 *   1: 命令执行失败（或非0的退出状态码）
 * 
 * 这个函数直接在当前shell进程中执行内置命令，不需要fork新进程
 */
int execute_builtin(char **tokens, int n_tokens) {
    /*
     * 处理 cd 命令：改变当前工作目录
     * 
     * cd命令可以：
     *   - cd         : 无参数，切换到HOME目录
     *   - cd /path   : 一个参数，切换到指定目录
     *   - cd a b     : 多个参数，这是错误的，应该报错
     */
    if (strcmp(tokens[0], "cd") == 0) {
        char *target_dir; // 目标目录路径
        
        if (n_tokens == 1) {
            /*
             * 情况1：没有参数，切换到HOME目录
             * getenv("HOME")获取环境变量HOME的值（通常是用户的主目录，如/home/username）
             */
            target_dir = getenv("HOME");
            // 如果HOME环境变量未设置，报错
            if (target_dir == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        } else if (n_tokens == 2) {
            /*
             * 情况2：有一个参数，切换到用户指定的目录
             * 例如：cd /tmp，tokens[1]就是 "/tmp"
             */
            target_dir = tokens[1];
        } else {
            /*
             * 情况3：多个参数，这是错误的
             * 例如：cd /tmp /home，这是不允许的
             * 
             * 注意：根据测试要求，错误消息应该是 "wrong number of arguments"
             */
            fprintf(stderr, "cd: wrong number of arguments\n");
            return 1;
        }
        
        /*
         * chdir()是系统调用，用于改变当前工作目录
         * 如果成功，返回0；如果失败（目录不存在、权限不足等），返回-1并设置errno
         */
        if (chdir(target_dir) != 0) {
            // 打印错误信息，strerror(errno)将错误码转换为可读的错误描述
            fprintf(stderr, "cd: %s\n", strerror(errno));
            return 1;
        }
        return 0; // 成功，返回0
        
    } else if (strcmp(tokens[0], "pwd") == 0) {
        /*
         * 处理 pwd 命令：打印当前工作目录
         * 
         * pwd命令应该没有参数，如果有参数就报错
         * 例如：
         *   pwd        : 正确，显示当前目录
         *   pwd /tmp   : 错误，pwd不接受参数
         */
        if (n_tokens > 1) {
            fprintf(stderr, "pwd: too many arguments\n");
            return 1;
        }
        
        /*
         * 准备一个缓冲区来存储当前目录路径
         * PATH_MAX是系统定义的常量，表示路径的最大长度（通常是4096）
         */
        char cwd[PATH_MAX];
        
        /*
         * getcwd()函数获取当前工作目录的完整路径
         * 参数说明：
         *   cwd: 存储路径的缓冲区
         *   sizeof(cwd): 缓冲区的大小
         * 返回值：成功返回cwd的指针，失败返回NULL
         */
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            // 成功获取路径，打印出来并换行
            printf("%s\n", cwd);
            return 0;
        } else {
            // 获取失败（理论上不应该发生，但以防万一）
            perror("pwd"); // perror会自动打印错误信息
            return 1;
        }
        
    } else if (strcmp(tokens[0], "exit") == 0) {
        /*
         * 处理 exit 命令：退出shell程序
         * 
         * exit命令可以：
         *   - exit      : 无参数，以状态码0退出（表示成功）
         *   - exit 5    : 一个参数，以指定的状态码退出
         *   - exit 5 6  : 多个参数，这是错误的
         */
        if (n_tokens > 2) {
            fprintf(stderr, "exit: too many arguments\n");
            return 1;
        }
        
        if (n_tokens == 1) {
            /*
             * 无参数：以状态码0退出（表示成功退出）
             */
            exit(0);
        } else {
            /*
             * 有一个参数：将参数转换为整数，作为退出状态码
             * atoi()函数将字符串转换为整数
             * 例如：exit 5 -> 以状态码5退出
             */
            exit(atoi(tokens[1]));
        }
    }
    
    return 0; // 理论上不应该到达这里，但为了代码完整性
}


/*
 * execute_external: 执行外部命令（如ls, cat等系统命令）
 * 
 * 参数说明：
 *   tokens: 解析后的命令token数组（例如：["ls", "-l", "/home"]）
 *   n_tokens: token的数量
 * 
 * 这个函数使用经典的fork/exec/wait模式：
 *   1. fork: 创建一个子进程（复制当前进程）
 *   2. exec: 在子进程中替换为要执行的命令程序
 *   3. wait: 父进程等待子进程执行完成
 * 
 * 为什么要fork新进程？
 *   因为exec会替换整个程序，如果我们不fork就直接exec，shell程序本身会被替换，
 *   执行完命令后shell就消失了。通过fork，我们可以保留shell进程，只替换子进程。
 */
void execute_external(char **tokens, int n_tokens) {
    /*
     * fork()函数创建一个新的子进程
     * 
     * fork()的返回值有三种情况：
     *   - 在子进程中：返回0
     *   - 在父进程中：返回子进程的PID（进程ID，一个正整数）
     *   - 出错：返回-1
     * 
     * fork之后，有两个几乎完全相同的进程在运行：
     *   - 父进程：继续执行shell的逻辑，等待子进程完成
     *   - 子进程：即将被替换为要执行的命令程序
     */
    pid_t pid = fork();
    
    if (pid == 0) {
        /*
         * 子进程代码块
         * 这个代码块只在子进程中执行
         */
        
        /*
         * 恢复SIGINT信号为默认行为
         * 
         * 在main函数中，我们设置了忽略SIGINT（Ctrl+C），这样shell不会被中断。
         * 但在子进程中，我们希望命令能够被Ctrl+C中断（这是正常行为）。
         * 所以我们将SIGINT恢复为默认行为（SIG_DFL）。
         */
        signal(SIGINT, SIG_DFL);
        
        /*
         * execvp()函数用新的程序替换当前进程
         * 
         * 参数说明：
         *   tokens[0]: 要执行的程序名（例如："ls"）
         *   tokens: 传递给程序的参数数组（例如：["ls", "-l", "/home", NULL]）
         * 
         * execvp会在系统的PATH环境变量指定的目录中查找可执行文件
         * 例如：execvp("ls", ...) 会在 /bin/ls、/usr/bin/ls 等目录中查找ls程序
         * 
         * 重要：如果execvp成功，它不会返回（因为整个进程都被替换了）
         *       只有当出错时才会返回（例如程序不存在）
         */
        execvp(tokens[0], tokens);
        
        /*
         * 如果代码执行到这里，说明execvp失败了
         * （通常是因为命令不存在、没有执行权限等原因）
         */
        fprintf(stderr, "%s: %s\n", tokens[0], strerror(errno));
        exit(EXIT_FAILURE); // 以失败状态码退出子进程
        
    } else if (pid > 0) {
        /*
         * 父进程代码块
         * 这个代码块只在父进程中执行（pid是子进程的ID，是一个正数）
         */
        
        /*
         * 父进程需要等待子进程执行完成
         * 
         * waitpid()函数等待指定的子进程结束
         * 
         * 参数说明：
         *   pid: 要等待的子进程ID
         *   &status: 用于存储子进程的退出状态（通过这个变量返回）
         *   WUNTRACED: 选项标志，表示也要等待被暂停的进程
         * 
         * 返回值：成功返回子进程ID，失败返回-1
         */
        int status;
        
        /*
         * 使用循环等待，直到子进程真正退出或被信号终止
         * 
         * WIFEXITED(status): 检查子进程是否正常退出（调用exit）
         * WIFSIGNALED(status): 检查子进程是否被信号终止（如Ctrl+C）
         * 
         * 这个循环是为了处理一些特殊情况，比如子进程可能被暂停和恢复
         */
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        
        /*
         * 子进程已经结束，提取退出状态码
         * 
         * WEXITSTATUS(status)从status中提取退出码（0表示成功，非0表示失败）
         * 我们将这个值保存到全局变量中，这样用户可以在下一个命令中使用 $? 来获取它
         */
        last_exit_status = WEXITSTATUS(status);
        
    } else {
        /*
         * fork失败的情况（pid < 0）
         * 这通常发生在系统资源不足时（进程数达到上限等）
         */
        perror("fork"); // 打印错误信息
        last_exit_status = 1; // 设置退出状态为1（表示失败）
    }
}

/*
 * expand_dollar_question: 展开 $? 特殊变量
 * 
 * 参数说明：
 *   tokens: 解析后的命令token数组（这个函数会修改这个数组）
 *   n_tokens: token的数量
 * 
 * 功能说明：
 *   这个函数实现了shell的 $? 特殊变量功能。
 *   $? 代表上一个命令的退出状态码（0表示成功，非0表示失败）。
 * 
 *   例如：
 *     $ ls /nonexistent      # 执行失败
 *     $ echo $?              # 输出上一个命令的退出码（比如2）
 *     
 *   这个函数会将命令中的 $? 替换为实际的数字字符串（如 "0", "1", "2"等）
 * 
 * 关键特性：
 *   - 使用静态缓冲区，避免内存管理问题
 *   - 可以处理同一个命令中出现多个 $? 的情况
 *   - 适用于内置命令和外部命令
 *   - 安全的空指针检查
 */
void expand_dollar_question(char **tokens, int n_tokens) {
    /*
     * 使用静态缓冲区存储退出状态码的字符串形式
     * 
     * static关键字表示这个变量在函数调用之间保持存在
     * （普通局部变量在函数返回后会被销毁）
     * 这样我们可以在多次调用之间复用这个缓冲区，避免内存泄漏
     * 
     * 16个字符足够存储任何退出状态码（最大9999，加上字符串结束符'\0'）
     */
    static char qbuf[16];
    
    /*
     * 将整数形式的退出状态码转换为字符串
     * 
     * snprintf()函数类似于printf，但它是将格式化的字符串写入缓冲区
     * 参数说明：
     *   qbuf: 目标缓冲区
     *   sizeof(qbuf): 缓冲区大小（防止溢出）
     *   "%d": 格式字符串，表示将整数转换为十进制字符串
     *   last_exit_status: 要转换的值（上一个命令的退出状态码）
     * 
     * 例如：如果last_exit_status是0，qbuf变成"0"
     *      如果last_exit_status是1，qbuf变成"1"
     */
    snprintf(qbuf, sizeof(qbuf), "%d", last_exit_status);
    
    /*
     * 遍历所有token，查找并替换 $? 符号
     * 
     * 例如：如果用户输入 "echo $?"，tokens可能是 ["echo", "$?"]
     *       这个循环会将 tokens[1] 从 "$?" 改为指向 "0"（或实际的退出码）
     */
    for (int i = 0; i < n_tokens; i++) {
        /*
         * 检查当前token是否为 "$?"
         * 
         * tokens[i] != NULL: 确保token指针有效（防止空指针）
         * strcmp(tokens[i], "$?") == 0: 检查token的内容是否为 "$?"
         */
        if (tokens[i] != NULL && strcmp(tokens[i], "$?") == 0) {
            /*
             * 找到 $?，将其替换为实际的退出状态码字符串
             * 
             * 注意：我们不是复制字符串，而是直接将tokens[i]指向qbuf
             * 这可以工作是因为qbuf是静态变量，在程序运行期间一直存在
             */
            tokens[i] = qbuf;
        }
    }
}

/*
 * execute_with_redirection: 执行带输入/输出重定向的命令
 * 
 * 参数说明：
 *   tokens: 解析后的命令token数组（可能包含 < 和 > 操作符）
 *   n_tokens: token的数量
 * 
 * 功能说明：
 *   这个函数处理shell的重定向功能：
 *   - 输入重定向 <：将文件内容作为命令的输入
 *     例如："cat < file.txt" 等价于先打开file.txt，然后将其内容传给cat命令
 *   - 输出重定向 >：将命令的输出写入文件
 *     例如："ls > output.txt" 将ls的输出写入output.txt文件（而不是显示在屏幕上）
 * 
 * 实现原理：
 *   使用dup2()系统调用重定向标准输入（文件描述符0）和标准输出（文件描述符1）
 *   每个进程都有三个默认的文件描述符：
 *     - 0: 标准输入（stdin）- 通常是键盘
 *     - 1: 标准输出（stdout）- 通常是屏幕
 *     - 2: 标准错误（stderr）- 通常是屏幕
 */
void execute_with_redirection(char **tokens, int n_tokens) {
    /*
     * 准备变量来存储重定向的文件名
     */
    char *input_file = NULL;   // 输入重定向的文件名（< 后面的文件）
    char *output_file = NULL;  // 输出重定向的文件名（> 后面的文件）
    
    /*
     * clean_tokens用于存储去除重定向操作符后的"干净"命令
     * 例如："cat < input.txt" 解析后变成 clean_tokens = ["cat", NULL]
     *       这样execvp才能正确执行命令
     */
    char *clean_tokens[MAX_TOKENS];
    int clean_count = 0; // clean_tokens中实际有多少个token
    
    /*
     * 第一步：解析tokens，识别重定向操作符并提取文件名
     * 
     * 遍历所有token，查找 "<" 和 ">" 符号：
     *   - 找到 "<"：下一个token是输入文件名
     *   - 找到 ">"：下一个token是输出文件名
     *   - 其他token：是命令的参数，添加到clean_tokens中
     */
    for (int i = 0; i < n_tokens; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            /*
             * 找到输入重定向操作符
             */
            // 检查是否有下一个token（即文件名）
            if (i + 1 < n_tokens) {
                input_file = tokens[i + 1]; // 保存输入文件名
                i++; // 跳过文件名（下次循环直接处理再下一个token）
            }
        } else if (strcmp(tokens[i], ">") == 0) {
            /*
             * 找到输出重定向操作符
             */
            // 检查是否有下一个token（即文件名）
            if (i + 1 < n_tokens) {
                output_file = tokens[i + 1]; // 保存输出文件名
                i++; // 跳过文件名
            }
        } else {
            /*
             * 这是一个普通的命令参数，不是重定向操作符
             * 将其添加到clean_tokens中
             */
            clean_tokens[clean_count++] = tokens[i];
        }
    }
    
    /*
     * execvp要求参数数组以NULL结尾，表示数组结束
     * 例如：["cat", "-n", NULL]
     */
    clean_tokens[clean_count] = NULL;
    
    /*
     * 第二步：fork子进程来执行命令
     * 
     * 为什么要fork？
     *   因为重定向只应该影响要执行的命令，不应该影响shell本身。
     *   如果我们不fork就直接重定向，shell的输入输出也会被重定向，这是不对的。
     */
    pid_t pid = fork();
    
    if (pid == 0) {
        /*
         * 子进程代码块
         */
        
        // 恢复SIGINT信号为默认行为（让命令可以被Ctrl+C中断）
        signal(SIGINT, SIG_DFL);
        
        /*
         * 处理输入重定向
         * 
         * 如果用户使用了 < filename，我们需要：
         *   1. 打开文件
         *   2. 将标准输入（文件描述符0）重定向到这个文件
         *   3. 这样当命令从标准输入读取时，实际是从文件读取
         */
        if (input_file) {
            /*
             * open()函数打开文件
             * 
             * 参数说明：
             *   input_file: 要打开的文件路径
             *   O_RDONLY: 只读模式打开
             * 
             * 返回值：成功返回文件描述符（正整数），失败返回-1
             */
            int fd = open(input_file, O_RDONLY);
            if (fd == -1) {
                // 文件打开失败（文件不存在、权限不足等）
                fprintf(stderr, "%s: %s\n", input_file, strerror(errno));
                exit(1);
            }
            
            /*
             * dup2()函数复制文件描述符
             * 
             * dup2(fd, 0) 的作用：
             *   - 将文件描述符fd复制到文件描述符0（标准输入）
             *   - 现在标准输入（0）和文件fd指向同一个文件
             *   - 当程序从标准输入读取时，实际是从文件读取
             * 
             * 举个例子：
             *   原来：0 -> 键盘输入
             *   执行dup2(fd, 0)后：0 -> 文件内容，fd -> 文件内容
             */
            dup2(fd, 0);
            
            /*
             * 关闭原来的文件描述符fd
             * 
             * 为什么？
             *   因为dup2后，fd和0都指向同一个文件。
             *   我们已经用0来读取文件了，不需要fd了。
             *   关闭fd可以避免文件描述符泄漏（系统对每个进程能打开的文件数有限制）
             */
            close(fd);
        }
        
        /*
         * 处理输出重定向
         * 
         * 如果用户使用了 > filename，我们需要：
         *   1. 打开文件（如果不存在则创建，如果存在则清空）
         *   2. 将标准输出（文件描述符1）重定向到这个文件
         *   3. 这样当命令向标准输出写入时，实际是写入文件
         */
        if (output_file) {
            /*
             * open()函数打开文件（用于写入）
             * 
             * 参数说明：
             *   output_file: 要打开的文件路径
             *   O_WRONLY: 只写模式
             *   O_CREAT: 如果文件不存在则创建
             *   O_TRUNC: 如果文件已存在，清空其内容（截断为0）
             *   0666: 文件权限（rw-rw-rw-，所有人可读写）
             * 
             * 这相当于创建一个新文件，或者清空已存在的文件
             */
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                // 文件打开/创建失败
                fprintf(stderr, "%s: %s\n", output_file, strerror(errno));
                exit(1);
            }
            
            /*
             * dup2(fd, 1) 将文件描述符fd复制到文件描述符1（标准输出）
             * 现在标准输出（1）指向文件，命令的输出会写入文件而不是屏幕
             */
            dup2(fd, 1);
            
            // 关闭原来的文件描述符
            close(fd);
        }
        
        /*
         * 重定向设置完成，现在执行命令
         * 
         * 执行时，命令会：
         *   - 如果有输入重定向：从文件读取输入（而不是键盘）
         *   - 如果有输出重定向：将输出写入文件（而不是屏幕）
         */
        execvp(clean_tokens[0], clean_tokens);
        
        /*
         * 如果execvp返回，说明执行失败（通常是命令不存在）
         */
        fprintf(stderr, "%s: %s\n", clean_tokens[0], strerror(errno));
        exit(EXIT_FAILURE);
        
    } else if (pid > 0) {
        /*
         * 父进程代码块：等待子进程完成
         * 
         * 这部分逻辑与execute_external中的完全相同：
         *   1. 等待子进程执行完成
         *   2. 获取子进程的退出状态码
         *   3. 保存到全局变量中（供 $? 使用）
         */
        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        
        last_exit_status = WEXITSTATUS(status);
        
    } else {
        /*
         * fork失败
         */
        perror("fork");
        last_exit_status = 1;
    }
}

/*
 * execute_pipeline: 执行管道命令（处理 | 操作符）
 * 
 * 参数说明：
 *   tokens: 解析后的命令token数组（可能包含多个 | 分隔的命令）
 *   n_tokens: token的数量
 * 
 * 功能说明：
 *   这个函数实现了shell的管道功能。管道允许将一个命令的输出作为另一个命令的输入。
 * 
 *   例如："ls | grep test" 
 *   - ls命令列出文件
 *   - 它的输出通过管道传递给grep命令
 *   - grep命令过滤出包含"test"的行
 * 
 *   更复杂的例子："ls | grep txt | wc -l"
 *   - ls列出文件 -> grep过滤包含txt的 -> wc统计行数
 *   - 数据从左到右流动，每个命令的输出是下一个命令的输入
 * 
 * 实现原理：
 *   1. 将命令按 | 分割成多个独立的命令
 *   2. 创建管道（pipe）连接相邻的命令
 *   3. 为每个命令fork一个子进程
 *   4. 每个子进程：
 *      - 从上一个管道读取输入（除了第一个命令）
 *      - 向下一个管道写入输出（除了最后一个命令）
 *      - 执行命令
 *   5. 父进程等待所有子进程完成
 * 
 * 限制：
 *   最多支持4个管道阶段（即最多5个命令，用4个|连接）
 */
void execute_pipeline(char **tokens, int n_tokens) {
    /*
     * TEST 12: 检测悬空管道语法错误
     * 
     * 需要检查以下错误情况：
     *   - | 在开头：第一个token就是 |
     *   - | 在结尾：最后一个token是 |
     *   - 连续两个 |：相邻的token都是 |
     */
    if (n_tokens > 0 && strcmp(tokens[0], "|") == 0) {
        // | 在开头，语法错误
        last_exit_status = 1;
        return;
    }
    if (n_tokens > 0 && strcmp(tokens[n_tokens - 1], "|") == 0) {
        // | 在结尾，语法错误
        last_exit_status = 1;
        return;
    }
    // 检查连续两个 |
    for (int i = 0; i < n_tokens - 1; i++) {
        if (strcmp(tokens[i], "|") == 0 && strcmp(tokens[i + 1], "|") == 0) {
            // 连续两个 |，语法错误
            last_exit_status = 1;
            return;
        }
    }
    
    /*
     * 第一步：将tokens分割成多个独立的命令，同时处理每个命令的重定向
     * 
     * commands数组存储分割后的命令（去除重定向操作符）
     * 例如："cat < file1 | grep test > file2" 会被分割成：
     *   commands[0] = ["cat", NULL], input_files[0] = "file1", output_files[0] = NULL
     *   commands[1] = ["grep", "test", NULL], input_files[1] = NULL, output_files[1] = "file2"
     * 
     * 最多支持5个命令（4个管道），所以第一维是5
     */
    char *commands[5][MAX_TOKENS];
    int cmd_count = 0;           // 实际有多少个命令
    int cmd_tokens[5] = {0};     // 每个命令有多少个token（不包括重定向操作符）
    char *input_files[5] = {NULL};   // 每个命令的输入重定向文件
    char *output_files[5] = {NULL};  // 每个命令的输出重定向文件
    
    /*
     * 遍历所有token，按 | 符号分割命令，同时提取重定向操作符
     * 
     * 例如："cat < file1 | grep test > file2" 的tokens是 
     * ["cat", "<", "file1", "|", "grep", "test", ">", "file2"]
     * 我们需要将其分割成：
     *   命令0: ["cat"], 输入文件: "file1", 输出文件: NULL
     *   命令1: ["grep", "test"], 输入文件: NULL, 输出文件: "file2"
     */
    int cmd_idx = 0; // 当前正在构建的命令索引（0, 1, 2...）
    for (int i = 0; i < n_tokens; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            /*
             * 遇到管道符号 |，表示当前命令结束
             * 
             * 在命令数组末尾添加NULL（execvp要求参数数组以NULL结尾）
             * 然后移动到下一个命令（cmd_idx++）
             */
            commands[cmd_idx][cmd_tokens[cmd_idx]] = NULL;
            cmd_idx++;
            cmd_count++; // 已完成的命令数+1
        } else if (strcmp(tokens[i], "<") == 0) {
            /*
             * 输入重定向操作符 <
             * 下一个token是输入文件名
             */
            if (i + 1 < n_tokens && strcmp(tokens[i + 1], "|") != 0) {
                input_files[cmd_idx] = tokens[i + 1];
                i++; // 跳过文件名
            }
        } else if (strcmp(tokens[i], ">") == 0) {
            /*
             * 输出重定向操作符 >
             * 下一个token是输出文件名
             */
            if (i + 1 < n_tokens && strcmp(tokens[i + 1], "|") != 0) {
                output_files[cmd_idx] = tokens[i + 1];
                i++; // 跳过文件名
            }
        } else {
            /*
             * 这是命令的一个token（命令名或参数），添加到当前命令中
             * （不包括重定向操作符和文件名，因为它们已经单独处理了）
             */
            commands[cmd_idx][cmd_tokens[cmd_idx]] = tokens[i];
            cmd_tokens[cmd_idx]++;
        }
    }
    
    /*
     * 最后一个命令也需要以NULL结尾
     * 并且命令总数+1（因为最后一个命令之前没有|）
     */
    commands[cmd_idx][cmd_tokens[cmd_idx]] = NULL;
    cmd_count++;
    
    /*
     * 检查是否有空命令（TEST 12要求）
     * 
     * 如果某个命令分割后没有任何token（除了NULL），这是语法错误
     */
    for (int i = 0; i < cmd_count; i++) {
        if (cmd_tokens[i] == 0) {
            // 空命令，语法错误
            last_exit_status = 1;
            return;
        }
    }
    
    /*
     * 检查管道阶段数是否超过限制
     * 
     * 如果cmd_count > 4，说明有超过4个管道阶段（即超过5个命令）
     */
    if (cmd_count > 4) {
        fprintf(stderr, "Too many pipeline stages (max 4)\n");
        last_exit_status = 1;
        return;
    }
    
    /*
     * 第二步：创建管道
     * 
     * 管道是一个通信通道，连接两个进程：
     *   - 一个进程向管道写入数据
     *   - 另一个进程从管道读取数据
     * 
     * pipe()函数创建一个管道，返回两个文件描述符：
     *   - pipes[i][0]: 读端（从管道读取数据）
     *   - pipes[i][1]: 写端（向管道写入数据）
     * 
     * 如果有N个命令，需要N-1个管道（每个管道连接两个相邻的命令）
     * 例如：3个命令需要2个管道
     */
    int pipes[3][2];  // 最多3个管道（支持4个命令）
    pid_t pids[4];    // 存储每个子进程的进程ID
    
    /*
     * 创建所有需要的管道
     * 
     * cmd_count - 1 是因为N个命令需要N-1个管道
     */
    for (int i = 0; i < cmd_count - 1; i++) {
        /*
         * pipe(pipes[i]) 创建一个新的管道
         * 
         * 如果成功，pipes[i][0]是读端，pipes[i][1]是写端
         * 如果失败（系统资源不足等），返回-1
         */
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            last_exit_status = 1;
            return;
        }
    }
    
    /*
     * 第三步：为每个命令fork一个子进程
     * 
     * 每个命令都需要在独立的进程中执行，这样它们才能通过管道通信
     */
    for (int i = 0; i < cmd_count; i++) {
        /*
         * fork一个子进程来执行第i个命令
         */
        pids[i] = fork();
        
        if (pids[i] == 0) {
            /*
             * 子进程代码块：设置输入输出重定向并执行命令
             */
            
            // 恢复SIGINT信号为默认行为
            signal(SIGINT, SIG_DFL);
            
            /*
             * 第一步：先设置管道重定向
             * 
             * 设置输入重定向：从上一个管道读取
             * 第一个命令（i=0）如果没有输入文件重定向，从标准输入读取
             * 其他命令（i>0）需要从前一个管道（pipes[i-1]）读取输入
             */
            if (i > 0) {
                /*
                 * dup2(pipes[i-1][0], 0) 将管道i-1的读端复制到标准输入
                 * 这样命令从标准输入读取时，实际是从上一个命令的输出读取
                 */
                dup2(pipes[i-1][0], 0);
            }
            
            /*
             * 设置输出重定向：写入下一个管道
             * 
             * 最后一个命令如果没有输出文件重定向，向标准输出写入
             * 其他命令需要向管道i的写端写入输出
             */
            if (i < cmd_count - 1) {
                /*
                 * dup2(pipes[i][1], 1) 将管道i的写端复制到标准输出
                 * 这样命令向标准输出写入时，实际是写入管道，被下一个命令读取
                 */
                dup2(pipes[i][1], 1);
            }
            
            /*
             * 第二步：处理文件重定向（会覆盖管道重定向）
             * 
             * TEST 6要求支持：cmd1 < file1 | cmd2 > file2
             * 文件重定向的优先级高于管道重定向
             */
            
            // 处理输入文件重定向（< filename）
            if (input_files[i] != NULL) {
                int fd = open(input_files[i], O_RDONLY);
                if (fd == -1) {
                    fprintf(stderr, "%s: %s\n", input_files[i], strerror(errno));
                    exit(1);
                }
                dup2(fd, 0);  // 覆盖标准输入（可能已经设置为管道）
                close(fd);
            }
            
            // 处理输出文件重定向（> filename）
            if (output_files[i] != NULL) {
                int fd = open(output_files[i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    fprintf(stderr, "%s: %s\n", output_files[i], strerror(errno));
                    exit(1);
                }
                dup2(fd, 1);  // 覆盖标准输出（可能已经设置为管道）
                close(fd);
            }
            
            /*
             * 关闭所有管道文件描述符
             * 
             * 为什么要关闭？
             *   1. 我们已经用dup2将需要的管道端复制到了0和1（或者被文件重定向覆盖了）
             *   2. 原始的管道文件描述符不再需要
             *   3. 关闭它们可以避免文件描述符泄漏
             *   4. 更重要的是：管道只有在所有写端都关闭后，读端才会收到EOF
             *      如果不关闭，最后一个命令可能永远等不到输入结束
             */
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]); // 关闭读端
                close(pipes[j][1]); // 关闭写端
            }
            
            /*
             * 重定向设置完成，执行命令
             * 
             * 执行时，命令会：
             *   - 从标准输入读取（可能是文件或管道）
             *   - 向标准输出写入（可能是文件或管道或标准输出）
             */
            execvp(commands[i][0], commands[i]);
            
            /*
             * 如果execvp返回，说明执行失败
             */
            fprintf(stderr, "%s: %s\n", commands[i][0], strerror(errno));
            exit(EXIT_FAILURE);
            
        } else if (pids[i] < 0) {
            /*
             * fork失败
             */
            perror("fork");
            last_exit_status = 1;
            return;
        }
    }
    
    /*
     * 第四步：父进程关闭所有管道文件描述符
     * 
     * 为什么要关闭？
     *   1. 父进程不需要使用管道（只有子进程之间需要通信）
     *   2. 关闭所有写端是必要的：管道只有在所有写端都关闭后，读端才会收到EOF
     *      如果不关闭父进程持有的写端，最后一个命令可能永远等不到输入结束
     *   3. 关闭读端虽然不是必须的，但可以避免资源浪费
     */
    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]); // 关闭读端
        close(pipes[i][1]); // 关闭写端
    }
    
    /*
     * 第五步：等待所有子进程完成
     * 
     * 父进程需要等待所有命令执行完成
     * 然后提取最后一个命令的退出状态码（作为整个管道的退出状态）
     */
    for (int i = 0; i < cmd_count; i++) {
        /*
         * 等待第i个子进程完成
         */
        int status;
        do {
            waitpid(pids[i], &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        
        /*
         * 如果是最后一个命令，保存其退出状态码
         * 
         * 根据shell的惯例，管道的退出状态码是最后一个命令的退出状态码
         * 例如："false | true"，虽然第一个命令失败，但整个管道返回0（因为true成功）
         */
        if (i == cmd_count - 1) {
            last_exit_status = WEXITSTATUS(status);
        }
    }
}