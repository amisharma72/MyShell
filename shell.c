#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>
#include <ctype.h>

#define MAX_BUF 100
#define MAX_TOKS 100
char history[50][50];
int count = 0;
extern char **environ;
char *args[MAX_TOKS];

void sigint_handler(int signum)   /* gives warning that the signal has been disabled */
{
    printf("\nCTRL+C Interrupt Signal has been disabled. To exit use the SIGUSR1 signal or enter any command to continue...\n");
}

void displayHistory()        /* prints the history */
{
    for ( int i =0 ; i <=count-1; i++)
        printf("%d %s",i+1, history[i]);
}

void my_handler(int signum)
{
    if (signum == SIGUSR1)      /* display history commands and exit on using kill -SIGUSR1 */
    {
        kill(getpid(), SIGUSR1);
        displayHistory();
        exit(0);
    }
}

void add_hist(const char *cmd)       /* adds the history list */
{
    strcpy(history[count], cmd );
    count++;
}

int main(int argc, char **argv)
{
    char *pch;
    char *path;
    size_t size=50;
    char * s =(char*) malloc(sizeof(char)* size);
    static const char prompt[] = "%% ";
    FILE *fp;
    int in_red;
    int out_red;
    int out1_red;
    
    signal(SIGUSR1, my_handler);
    signal(SIGINT, sigint_handler);   /* catches the CTRL C signal and calls an interrupt handler */
    
    if (argc > 2) {
        perror("error");
        exit(EXIT_FAILURE);
    }
    if (argc == 2) {
        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            perror("Cannot open");
            exit(EXIT_FAILURE);
        }
    } else fp = stdin;
    
    while (1)
    {
        in_red = out_red = out1_red= 0;
        for (int i = 0; i < MAX_TOKS; i++)
            args[i] = NULL;
        
        if (fp == stdin) printf(prompt);
        char *a = fgets(s, MAX_BUF-1, fp);      /* read input */
        
        if (a == NULL || strcmp(s, "exit\n") == 0 || strcmp(s, "quit\n") == 0)      /* exit or quit */
        {
            if (a == NULL && fp == stdin) printf("\n");
            exit(EXIT_SUCCESS);
        }
        if (strcmp(s, "history\n") == 0)     /* history command */
        {
            add_hist(s);
            displayHistory();
            continue;
        }
        if (strcmp(s,"\n") != 0 )
            add_hist(s);

        char *string = s;
        int i = 0;
        
        pch= strtok_r(string, " \n", &string);      /* break input into tokens */
        while(pch!= NULL && i < MAX_TOKS)
        {
            args[i] = pch;
            if(strcmp(pch, "<") == 0)
            {
                in_red = i;
                i--;
            }
            if(strcmp(pch, ">")==0)
            {
                out_red = i;
                i--;
            }
            if(strcmp(pch, ">>")==0)
            {
                out1_red = i;
                i--;
            }
            if(strcmp(pch, "|")==0)
            {
                int pipefds[2];
                pid_t id;
                if(pipe(pipefds) != 0)            /* pipe() system call and PIPE Support */
                perror("failed to create pipe");
                
                id = fork();
                if(id == -1){
                    perror("fork failed");
                    exit(-1);
                }
                if(id == 0){
                    dup2(pipefds[1],STDOUT_FILENO);    /* dup2 system call */
                    close(pipefds[0]);
                    execlp("ls","ls","-l",NULL);
                    exit(1);
                }else{
                    dup2(pipefds[0],STDIN_FILENO);
                    close(pipefds[1]);
                    execlp("wc","wc","-l",NULL);
                    exit(1);
                }
            }
          
            i++;
            pch= strtok_r(NULL, " \n", &string);
        }
        args[i] = NULL;
        
        if (strcmp(args[0], "help") == 0)    /* help command */
        {
            printf("enter command or exit\n");
            continue;
        }
        
        if (!strcmp(args[0], "env"))        /* environment command */
        {
            char **envir = environ;
            while (*envir) {
                printf("%s\n", *envir);
                envir++;
        }
            continue;
        }
        
        if (strcmp(args[0], "cd") == 0)      /* cd command */
        {
            if (i == 1) {
                path = getenv("HOME");     /* searches environment list to find the environment variable */
            } else {
                path = args[1];
            }
            int cd_prompt = chdir(path);
            if (cd_prompt != 0)
            {
                switch(cd_prompt)
                {
                    case ENOENT:
                        printf("Doesnot exist\n");
                        break;
                    case ENOTDIR:
                        printf("No directory\n");
                        break;
                    default:
                        printf("Bad address\n");
                }
            }
            continue;
        }
        
        int fd[2];              /* file descriptor */
        pid_t pid = fork(), stat;       /* fork process */
        if (pid< 0)
        {
            perror("fork failed");
            exit(1);
        }
        if (pid == 0)        /* child process and usage of execvp */
        {
            if(in_red)
            {
                if((fd[0] = open(args[in_red], O_RDONLY, 0)) == -1)   /* open function with flag arguments */
                {
                    perror(args[in_red]);
                    exit(EXIT_FAILURE);
                }
                dup2(fd[0], 0);       /* dup2 system call */
                close(fd[0]);
                args[in_red] = NULL;
            }
            if(out_red)
            {
                if((fd[1] = open(args[out_red], O_WRONLY | O_CREAT | O_TRUNC | O_CREAT,
                               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
                {
                    perror (args[out_red]);
                    exit( EXIT_FAILURE);
                }
                dup2(fd[1], 1);
                close(fd[1]);
                args[out_red] = NULL;
            }
            if(out1_red)
            {
             
                if((fd[1] = open(args[out1_red], O_RDWR | O_APPEND)) == -1)
                {
                    perror (args[out1_red]);
                    exit( EXIT_FAILURE);
                }
                dup2(fd[1], 1);
                close(fd[1]);
                args[out1_red] = NULL;
            }
            execvp(args[0], args);
            printf("error\n");
            exit(1);
        }
        else            /* parent process */
        {
            while (wait (&stat) != pid)
                continue;
        }
    }
}
