#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include "../libraries/logger.c"

pid_t spawn(const char* program, char** arg_list);
void free_resources();
void on_child_terminated(int signo);

char *fl_log, *fl_cx, *fl_cz, *fl_ix, *fl_iz, *fl_mw;
pid_t pid_mx, pid_mz, pid_wd, pid_cc, pid_ic;
int fd_log;

int main(int argc, char *argv[])
{
    printf("################ MASTER PROCESS ################\n");
    fflush(stdout);
    
    fl_log = "./sources/log/log_file.txt";
    fl_cx = "/tmp/cmd_motor_x";
    fl_cz = "/tmp/cmd_motor_z";
    fl_ix = "/tmp/isp_motor_x";
    fl_iz = "/tmp/isp_motor_z";
    fl_mw = "/tmp/master_watchdog";
    
    //###################### CREATING FILES FOR COMUNICATION ###########################
    
    // Creates named pipe for comunication between command_console and motor_x
    if(mkfifo(fl_cx, 0666) == -1 && errno != EEXIST)
        perror("Creating pipe for command_console and motor_x");
        
    // Creates named pipe for comunication between command_console and motor_z
    if(mkfifo(fl_cz, 0666) == -1 && errno != EEXIST)
        perror("Creating pipe for command_console and motor_z");
       
    // Creates named pipe for comunication between inspection_console and motor_x
    if(mkfifo(fl_ix, 0666) == -1 && errno != EEXIST)
        perror("Creating pipe for inspection_console and motor_x");
        
    // Creates named pipe for comunication between inspection_console and motor_z
    if(mkfifo(fl_iz, 0666) == -1 && errno != EEXIST)
        perror("Creating pipe for inspection_console and motor_z");
        
    // Creates named pipe for comunication between master and watchdog    
    if(mkfifo(fl_mw, 0666) == -1 && errno != EEXIST)
        perror("Creating pipe for master and watchdog");
        
    // Creates and opens log file
    fd_log = creat(fl_log, 0666);
    if(fd_log == -1)
        perror("Creating log file");
        
    char sfd_log[10];
    sprintf(sfd_log, "%i", fd_log);
    
    //########################### CREATING PROCESSES #################################
    
    signal(SIGCHLD, on_child_terminated);
    
    // child (watchdog)
    char * args_wd[] = { "./exes/watchdog", sfd_log, fl_mw, (char*)NULL };
    pid_wd = spawn("./exes/watchdog", args_wd);
    
    char spid_wd[10];
    sprintf(spid_wd, "%i", pid_wd);
    
    // child (motor_x)
    char * args_x[] = { "./exes/motor", sfd_log, spid_wd, fl_cx, fl_ix, (char*)NULL };
    pid_mx = spawn("./exes/motor", args_x);
    
    // child (motor_z)
    char * args_z[] = { "./exes/motor", sfd_log, spid_wd, fl_cz, fl_iz, (char*)NULL };
    pid_mz = spawn("./exes/motor", args_z);
    
    // Comunicates to watchdog the pid of the two motors
    // And closes and frees pipe immediately
    int fd_wd = open(fl_mw, O_WRONLY);
    if(fd_wd == -1)
        perror("Opening pipe with watchdog"); 
    if(write(fd_wd, &pid_mx, sizeof(int)) == -1)
        perror("Writing motor_x pid on pipe");
    if(write(fd_wd, &pid_mz, sizeof(int)) == -1)
        perror("Writing motor_z pid on pipe");
    if(close(fd_wd) == -1)
        perror("Closing pipe with watchdog");
    if(unlink(fl_mw) == -1)
        perror("Unlinking pipe with watchdog");
    
    char spid_mx[10];
    char spid_mz[10];
    sprintf(spid_mx, "%i", pid_mx);
    sprintf(spid_mz, "%i", pid_mz);
    
    // child (command_console)
    char * args_cc[] = { "/usr/bin/konsole",  "-e", "./exes/command_console", 
                         sfd_log, spid_wd, fl_cx, fl_cz,(char*)NULL };
    pid_cc = spawn("/usr/bin/konsole", args_cc);
    
    // child (inspection_console)    
    char * args_ic[] = { "/usr/bin/konsole",  "-e", "./exes/inspection_console", 
                         sfd_log, spid_wd, fl_ix, fl_iz, spid_mx, spid_mz, (char*)NULL };
    pid_ic = spawn("/usr/bin/konsole", args_ic);
    
    //############################# LOGGIN MESSAGES #################################
    

    printf("Process [watchdog] has id %i.\n", pid_wd);
    printf("Process [motor_x] has id %i.\n", pid_mx);
    printf("Process [motor_z] has id is %i.\n", pid_mz);
    printf("Process [command_console] has id %i.\n", pid_cc);
    printf("Process [inspection_cosole] has id %i.\n", pid_ic);
    printf("\n");
    fflush(stdout);
    
    // Check if all the processes have been created correctly
    // Then, the master process waits for the end of one of its child.
    if(pid_wd == -1 || pid_mx == -1 || pid_mz == -1 || pid_cc == -1 || pid_ic == -1)
    {
        printf("Could not create all child processes.\n");
    }
    else
    {
        printf("All child processes created correctly.\n");
        
        char command[10];
        do
        {
            printf("Insert 'q' to quit program: ");
            scanf("%s", command);
        }
        while(strcmp(command, "q") != 0);
    }
    
    //############################## TERMINATING ###################################
    
    // Stop handling signal for child termination,
    // we are going to terminate them all
    signal(SIGCHLD, SIG_DFL);
    
    free_resources();
    
    return 0;
}

void on_child_terminated(int signo)
{
    if(signo == SIGCHLD)
    {
        int status;
        int pid = wait(&status);
        
        printf("\n\n");
        printf("Child with pid %i has been terminated with status %i.\n", pid, status);
        printf("Check log file for details of eventual errors.\n");
        free_resources();
        exit(0);
    }
}
    
void free_resources()
{       
    //########################## CLEANING UP RESOURCES #############################
    printf("\n");
    
    //Terminating all child processes
    if(kill(pid_mx, SIGTERM) == -1 && errno != ESRCH)
        perror("Killing motor_x process");
    if(kill(pid_mz, SIGTERM) == -1 && errno != ESRCH)
        perror("Killing motor_z process");
    if(kill(pid_wd, SIGTERM) == -1 && errno != ESRCH)
        perror("Killing watchdog process");
    if(kill(pid_cc, SIGTERM) == -1 && errno != ESRCH)
        perror("Killing command_console process");
    if(kill(pid_ic, SIGTERM) == -1 && errno != ESRCH)
        perror("Killing inspection_console process");
        
    // Closes log file
    if(close(fd_log) == -1)
        perror("Closing log file");
    
    // Freeing all the allocated files
    if(unlink(fl_cx) == -1)
        perror("Unlinking pipe for command_console and motor_x");
    if(unlink(fl_cz) == -1)
        perror("Unlinking pipe for command_console and motor_z");
    if(unlink(fl_ix) == -1)
        perror("Unlinking pipe for inspection_console and motor_x");
    if(unlink(fl_iz) == -1)
        perror("Unlinking pipe for inspection_console and motor_z");
    
    printf("All the resources have been cleaned up.\n");
    fflush(stdout);
}

pid_t spawn(const char* program, char** arg_list) 
{
    pid_t child_pid = fork();
    
    if(child_pid != 0)
        return child_pid;
     
    execvp(program, arg_list);
    
    // System call 'execvp' only returns if an error has occurred
    perror("Could not create child process");
    return -1;
}
