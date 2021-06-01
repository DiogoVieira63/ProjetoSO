#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>


//servidor

#define MAX_PROCESSES 5



int static out = 0;


typedef struct filtros
{
    char *name;
    char *path;
    int running;
    int max;
}*Filtro;

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
        int bytes;
        bytes = read (pipefd[0],result,10);
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

void printFiltro (Filtro f){
    printf ("name -> %s\n",f->name);
    printf ("path -> %s\n",f->path);
    printf ("max -> %d\n",f->max);
    printf ("running -> %d\n",f->running);

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
        array[i] = doFiltro (buffer, pathFiltros);
    }
    return array;
}




void ctrlChandler (int signum){
    out = 1;
    printf ("Goodbye\n");
}

int check_ffmpeg (){
    if (!(fork ())){
        execlp ("which","which","ffmpeg",NULL);
    }
    else
    {
        int status;
        wait (&status);
        printf ("exit status %d\n",WEXITSTATUS (status));
        if (WEXITSTATUS (status) == 0) return 1;
        else if (WEXITSTATUS (status) == 1) printf ("need ffmpeg installed\n");
    }
    return 0;
    
}



int main(int argc, char** argv){
    if (argc != 3 ) {
        fprintf (stderr,"Invalid number of arguments");
        return 0;
    }
    int numberFiltros;
    Filtro* filtrosArray = setupFiltros (&numberFiltros,argv[2],argv[1]);
    if (!check_ffmpeg()) return 0;
    int processesStatus[5];
    mkfifo ("/tmp/servidorToCliente",0644); //comunicação do servidor para o cliente
    mkfifo ("/tmp/clienteToServidor",0644); //comunicação do cliente para o servidor
    char message[30] = "Information Received\n";
    int number = 0;
    //signal (SIGINT,ctrlChandler);
    char buffer[1024];
    int bytes;
    int fromCliente = open ("/tmp/clienteToServidor",O_RDONLY,0644);
    int toCliente = open ("/tmp/servidorToCliente",O_WRONLY,0644);
    printf ("Read from cliente\n");
    bytes = read (fromCliente,buffer,1024);
    char outputBuffer[1024];
    outputBuffer[0] = 0;
    if (!strcmp (buffer,"status")){
        showStatus (filtrosArray,numberFiltros,outputBuffer);
    }
    write (toCliente,outputBuffer,strlen (outputBuffer) + 1);
    close (fromCliente);
    /*
    printf ("Write to cliente\n");
    while (1){
        if ((bytes = read (STDIN_FILENO,buffer,1024)) == 0)break;
        write (toCliente,buffer,bytes);
    }
    */
    close (toCliente);
}


/*
int main() {
    int fd = open ("clienteToServidor",O_RDONLY,0644);
    printf ("read from servidor\n");
    char buffer[100];
    while (1)
    {
        int bytes;
        if (( bytes = read (fd,buffer,100)) == 0) break;
        write (STDOUT_FILENO,buffer,bytes);
    }
    close (fd);
    
}

*/