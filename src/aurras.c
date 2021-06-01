#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>




int main(int argc, char **argv) {
    int toServidor = open   ("/tmp/clienteToServidor",O_WRONLY,0644);
    int fromServidor = open ("/tmp/servidorToCliente",O_RDONLY,0644);
    int bytes;
    char buffer[1024];
    buffer[0] = 0;
    if (argc > 4 || argc == 2){
        //if (strcmp (argv[1],"transform")) return 0;
        for (int i = 1; i < argc; i++){
            strcat (buffer,argv[i]);
            if (i < argc -1)strcat (buffer," ");
        }
        //strcat (buffer,"\n");
        write (toServidor,buffer,strlen(buffer) + 1);
        close (toServidor);
    }
    while (1)
    {
        if ((bytes = read (fromServidor,buffer,1024)) == 0)break;
        write (STDOUT_FILENO,buffer,bytes);
    }
    close (fromServidor);
    
}

/*

int main() {
    int fd = open ("samples/sample-1-so.m4a",O_RDONLY,0644);
    int fd2 = open ("output.mp3", O_CREAT | O_WRONLY | O_TRUNC,0644);
    if (fd2 == -1)printf ("Erro ao ler file\n");
    if (!fork ()){
        dup2 (fd,STDIN_FILENO);
        dup2 (fd2,STDOUT_FILENO);
        close (fd2);
        close (fd);
        execlp ("bin/aurrasd-filters/aurrasd-tempo-double","bin/aurrasd-filters/aurrasd-tempo-double",NULL);
    }
    else
        wait (NULL);
}   

*/
