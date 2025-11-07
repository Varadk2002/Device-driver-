#include<stdio.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>
#include<signal.h>

void sigchld_handler(int sig)
{
    printf("SIGCHLD received : %d\n",sig);
    int s;
    waitpid(-1,&s,0);//-1 means for any child
    printf("Exit status of child : %d\n", WEXITSTATUS(s));
}
// Implement small Shell program
//  1. Take command from user
//  2. Interpret the command
//      2.1 seperate command and options from user enetered command
//      2.2 differntiate internal / external command
//      3.3 execute the external command / write logic for internal commands

int main(void)
{
    char cmd[264], *args[32], *ptr;
    signal(SIGCHLD,sigchld_handler);

    while(1){
        //  1. Take command from user
        printf("cmd> ");
        gets(cmd);

        //2 seperate command and options from user enetered command
        int i = 0;
        ptr = strtok(cmd, " ");             // ls -l -a
        args[i++] = ptr;                        // args[0] = "ls"
        do{                                     // args[1] = "-l"
            ptr = strtok(NULL, " ");            // args[2] = "-a"
            args[i++] = ptr;
            // i=i-2;
            // if(args[i]=='&')
            // {
            //     flag=1;
            // }                    // args[3] = NULL
        }while(ptr != NULL);

        //3. differntiate internal / external command
        if(strcmp(args[0], "cd") == 0){             //cd path
            // internal command                         // args[0] = "cd"
            chdir(args[1]);                            // args[1] = "path"            
        }
        else if(strcmp(args[0], "exit") == 0){
            // internal command
            _exit(0);
        }
        else{
            // external command
            int pid = fork();
            if(pid == 0){
                // child process
                int err = execvp(args[0], args);
                if(err < 0){
                    printf("%s command is failed\n", args[0]);
                    _exit(-1);
                }
            }
            // else if(args[i-2]=='&')
            else if(strcmp(args[i-2],"&")==0)
            {
                // _exit(0);
                // signal(SIGCHLD,sigchld_handler);
                int s;
                waitpid(pid,&s,WNOHANG);
            }
            else{
                // parent process (shell)
                int s;
                // if (flag==0)
                // {
                waitpid(pid, &s, 0);
                // }
            }
        }
    }

    return 0;
}








