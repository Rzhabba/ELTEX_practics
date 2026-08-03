#include <sys/stat.h> 
#include <stdlib.h> 
#include <fcntl.h> 
#include <stdio.h> 
#include <unistd.h>

#define BUF_SIZE 1024 

int main (int argc, char * argv [ ]) 
    { 
    int inputFd, outputFd, openFlags; 
    mode_t filePerms ; 
    ssize_t numRead; 
    char buf[BUF_SIZE]; 
    if (argc != 3) { 
        printf("Usage: %s old-file new-file \n", argv[0]); 
        exit(-1); 
        } 
    /* Открытие файлов ввода и вывода */ 
    inputFd = open (argv[1], O_RDONLY); 
    if (inputFd == -1) { 
        printf ("Error opening file %s\n", argv[1]); exit(-2); 
        } 
    openFlags = O_CREAT | O_WRONLY | O_TRUNC; 
    filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | 
    S_IROTH | S_IWOTH; /* rw - rw - rw - */ 
    outputFd = open (argv [2], openFlags, filePerms); 
    if (outputFd == -1) { 
        printf ("Error opening file %s\n ", argv[2]);  
        exit(-3); 
        } 
    /* Перемещение данных до достижения конца файла 
    ввода или возникновения ошибки */ 
    while ((numRead = read (inputFd, buf, BUF_SIZE)) > 0) { 
    if (write (outputFd, buf, numRead) != numRead) { 
        printf ("couldn't write whole buffer\n "); exit(-4); 
    } 
    if (numRead == -1) { 
        printf ("read error\n "); exit(-5); 
    } 
    if (close (inputFd ) == -1 ) { 
        printf ("close input error\n"); exit(-6); 
    } 
    if (close (outputFd ) == -1 ) { 
        printf ("close output error\n"); exit(-7); 
    } 
    } 
    exit(EXIT_SUCCESS); 
    }

    