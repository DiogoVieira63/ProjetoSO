#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>

#include "aurras.h"

//servidor

typedef struct filtros
{
    char *name;
    char *path;
    int running;
    int max;
}*Filtro;


int out = 0;
int taskNumber = 0;
int task_pid[1024];
int taskStatus[1024];
char* taskCommand[1024];
Filtro* filtrosArray;
int numberFiltros;
int fifoNumber;



int howMany (){
    int pipefd[2];
    if (pipe (pipefd)== -1){
        fprintf (stderr,"erro pipe\n");
    }
    int fd = open ("etc/aurrasd.conf",O_RDONLY,0644);
    char result[10];
    if (!fork ()){
        close (pipefd[0]);
        dup2 (pipefd[1],STDOUT_FILENO);
        close (pipefd[1]);
        dup2 (fd,STDIN_FILENO);
        close (fd);
        execlp ("wc","wc","-l",NULL);
    }
    else {
        close(pipefd[1]);
        read (pipefd[0],result,10);
        wait (NULL);
    }
    return atoi (result);
}

Filtro doFiltro (char *linha, char *pathFiltros){
    Filtro filtro = malloc (sizeof (struct filtros));
    filtro->name = strdup (strtok (linha," "));
    filtro->path = strdup(pathFiltros); 
    strcat (filtro->path,strdup (strtok (NULL," ")));
    filtro->running = 0;
    filtro->max = atoi (strtok (NULL,"\n"));
    return filtro;
}


void showStatus (Filtro* filtros, int number, char *buffer){
    for (int i = 0; i < number;i++){
        Filtro f = filtros[i];
        char str[50];
        sprintf (str,"filter %s: %d/%d (running/max)\n",f->name,f->running,f->max);
        strcat (buffer,strdup (str));
    }
}


ssize_t readln(int fd, char* line, size_t size) {
    ssize_t bytes_read = read(fd, line, size);
    if(!bytes_read) return 0;

    size_t line_length = strcspn(line, "\n") + 1;
    if(bytes_read < line_length) line_length = bytes_read;
    line[line_length] = 0;
    
    lseek(fd, line_length - bytes_read, SEEK_CUR);
    return line_length;
}

Filtro* setupFiltros (int *number, char *pathFiltros, char *pathConfig){
    *number = howMany();
    Filtro *array = malloc (sizeof (Filtro*) * (*number));
    int fd = open (pathConfig,O_RDONLY,0644);
    int bytes;
    char buffer[1024];
    for (int i = 0; i < *number;i++){
        bytes = readln (fd,buffer,50);
        if (bytes > 1) {
            array[i] = doFiltro (buffer, pathFiltros);        }
    }
    return array;
}


void updateFiltros (char * str, int number){
    char *token;
    token = strtok(str, " ");
    while( token != NULL ) {
         for (int i = 0; i < numberFiltros;i++){
             Filtro filtro = filtrosArray[i];
             if (!strcmp(filtro->name,token)){
                 filtro->running += number;
             }
         }
       token = strtok(NULL," ");
    }
}


int canProcess (char *str){
    char *rest = strdup (str);
    char *token;
    token = strtok(rest, " ");
    int array[numberFiltros];
    for (int i = 0;i < numberFiltros;i++) array[i] = 0;
    while( token != NULL ) {
         for (int i = 0; i < numberFiltros;i++){
             Filtro filtro = filtrosArray[i];
             if (!strcmp(filtro->name,token)){
                 array[i] += 1;
             }
         }
       token = strtok(NULL," ");
    }
    int result = 1;
    for (int i = 0; i < numberFiltros;i++){
        Filtro filtro = filtrosArray[i];
        if (filtro->max < array[i]) {
            result = -1;
            break;
        }
        if (filtro->max < filtro->running + array[i]) {
            result = 0;
            break;
        }
    }
    free (rest);
    return result;
}

void sigTermHandler (int signum){
    for (int i = 0; i < taskNumber;i++){
        if (taskStatus[i] == A_EXECUTAR){
            waitpid (task_pid[i],NULL,0);
        }
    out = 1;
    }
}

void sigChld_handler (int signum){
    int status;
    int pid;
    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0;i < taskNumber;i++){
            if (task_pid[i] == pid){
                taskStatus[i] = TERMINADO;
                updateFiltros(taskCommand[i],-1);
                break;
            }
        }
    }
    
}



int check_ffmpeg (){
    if (!(fork ())){
        int fd = open("/dev/null", O_WRONLY);
        dup2 (fd,STDOUT_FILENO);
        close (fd);
        execlp ("which","which","ffmpeg",NULL);
    }
    else
    {
        int status;
        wait (&status);
        if (WEXITSTATUS (status) == 0) return 1;
        else if (WEXITSTATUS (status) == 1) printf ("need ffmpeg installed\n");
    }
    return 0;
    
}

char* getFiltro (Filtro* array, int numberFiltros,char *name){
    for (int i = 0; i < numberFiltros;i++){
        Filtro f = array[i];
        if (!(strcmp (name,f->name))) return f->path;
    }
    return NULL;
}


void loop_pipe(char **cmd,char *output,char *input){
    int p[2];
    pid_t pid;
    int fd_in = open (input,O_RDONLY,0644);
    int fdout = open (output,O_CREAT |O_TRUNC |O_WRONLY,0644);
    if (fd_in == -1 || fdout == -1) return; 
    while (*cmd != NULL) {
        if (pipe(p)) {
            fprintf(stderr, "Pipe failed.\n");
            return ;
        }
        if (!(pid = fork ())) {
            dup2(fd_in, STDIN_FILENO); //set input from previous commands
            if (cmd[1] != NULL) //if there's a command next change the output to write in pipe
                dup2(p[1],STDOUT_FILENO); 
            else {
                dup2(fdout,STDOUT_FILENO);
                close (fdout);
            }
            close(p[0]);
            execlp(cmd[0],cmd[0],NULL);
            exit(EXIT_FAILURE);
        } else {
            wait(NULL);
            close(p[1]);    
            fd_in = p[0];  //save the input for the next command
            cmd++;
        }
    }
}


int main(int argc, char** argv){
    if (argc != 3 ) {
        fprintf (stderr,"Invalid number of arguments");
        return 0;
    }
    if (!check_ffmpeg()) return 0;
    filtrosArray = setupFiltros (&numberFiltros,argv[2],argv[1]);
    mkfifo (CLIENTETOSERVER,0644); //comunicação do cliente para o servidor
    mkfifo (SERVERTOCLIENTE,0644); //comunicação do servidor para o cliente
    signal (SIGTERM,sigTermHandler);
    signal(SIGCHLD, sigChld_handler);
    char buffer[1024];
    while (1){
        int fromCliente = open (CLIENTETOSERVER,O_RDONLY,0644);
        int toCliente = open (SERVERTOCLIENTE,O_WRONLY,0644);
        if (out) break;
        while (read (fromCliente,buffer,1024)> 0){
            close (fromCliente);
            char fifo[200];
            sprintf (fifo,"tmp/fifo%d",fifoNumber++);
            mkfifo (fifo,0644);
            write(toCliente,fifo,strlen (fifo)+1);
            close(toCliente);
            int toPersonalCliente = open (fifo,O_WRONLY,0644);
            char outputBuffer[1024];
            outputBuffer[0] = 0;
            char *command = strdup (buffer);
            if (!strcmp (command,"status")){
                for (int i = 0;i < taskNumber;i++){
                    if (taskStatus[i] == 0){
                        char str[200];
                        sprintf (str,"task #%d:%s\n",i,taskCommand[i]);
                        strcat(outputBuffer,strdup(str));
                    }
                }
                showStatus (filtrosArray,numberFiltros,outputBuffer);
                char strPid[20];
                sprintf (strPid,"pid: %d\n",getpid());
                strcat(outputBuffer,strPid);
                write (toPersonalCliente,outputBuffer,strlen(outputBuffer) +1);
                close (toPersonalCliente);
                unlink (fifo);
            }
            if (!strcmp (strtok (command," "),"transform")){
                char message[] = "Pending\n";
                taskStatus[taskNumber] = PENDENTE;
                write (toPersonalCliente,message,strlen(message) +1);
                taskCommand[taskNumber] = strdup (buffer); 
                char *linha = strtok (NULL,"");
                char *input = strtok (linha," ");
                char *output = strtok (NULL," "); 
                char*cmd[20]; 
                int number = 0;  
                char *token;
                while ((token = strtok(NULL, " "))){
                    cmd[number] = getFiltro(filtrosArray,numberFiltros,token);
                    number++;
                }
                cmd[number] = NULL;                
                pid_t pid;
                int result;
                while ((result =canProcess(buffer)) == 0){
                    sleep (0.5);
                }
                if (result == -1){
                    char message2[] = "O seu pedido ultrapassa o limite máximo para um filtro\nstatus para ver a disponibilidade\n";
                    write (toPersonalCliente,message2,strlen(message2) +1);
                }
                else{
                    updateFiltros(buffer,1);
                    char message1[] = "Processing\n";
                    write (toPersonalCliente,message1,strlen(message1) +1);
                    taskStatus[taskNumber] = A_EXECUTAR;
                    if (!(pid =fork ())){
                        loop_pipe(cmd,output,input);
                        close (toPersonalCliente);
                        unlink (fifo);
                        exit(0);
                    }
                    task_pid[taskNumber] = pid;
                    taskNumber++;
                }
            }
        close (toPersonalCliente);
        }
    }
    unlink(CLIENTETOSERVER);
    unlink(SERVERTOCLIENTE);
}
