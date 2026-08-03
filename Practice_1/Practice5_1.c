#include <sys/stat.h> 
#include <sys/wait.h>
#include <stdlib.h> 
#include <fcntl.h> 
#include <stdio.h> 
#include <unistd.h>
#include <string.h>

#define BUF_SIZE 1024 
#define NAME_SIZE 256

int main(void){
    int inputFd, outputFd, openFlags; 
    mode_t filePerms ; 
    ssize_t numRead; 
    char buf[BUF_SIZE]; 
    char nameOriginal[NAME_SIZE];
    char nameCopy[NAME_SIZE+8];
    int dataTransfer[2], readyOrNot[2];
//через dataTransfer отсылаем в качестве родителя
//и читаем в качестве ребенка
//данные исходного файла,
//readyOrNot- для потомка сигнал о том, что тот готов
//к приему данных, для родителя- о том, что готов к отправке
    int rd, wr;


    printf("Введите имя копируемого файла: \n");
    fgets(nameOriginal, NAME_SIZE, stdin);
    nameOriginal[strcspn(nameOriginal, "\n")]= 0;
    if (nameOriginal[0]==0){
        fprintf(stderr, "Ошибка. Имя не может быть пустым\n");
        exit(-1);
    }

    inputFd=open(nameOriginal, O_RDONLY);
    if (inputFd==-1){
        fprintf(stderr, "Ошибка. Файл не существует. Коипровать нечего\n");
        exit(-2);
    }

    snprintf(nameCopy,sizeof nameCopy, "%s.copy", nameOriginal);


    if(pipe(dataTransfer)<0){
        perror("Parrent Pipe Error");
        exit(-1);
    }
    if (pipe(readyOrNot)<0){
        perror("Child Pipe error");
        exit(-1);
    }

    pid_t pid =fork();
    if(pid<0){
        perror("Fork error");
        exit(-1);
    }
//ДОЧЕРНИЙ ПРОЦЕСС

    if(pid==0){
        rd=dataTransfer[0]; //читаем данные
        wr= readyOrNot[1]; //сообщаем о готовности дочернего
        close(dataTransfer[1]); //закрываем неиспользуемые концы
        close(readyOrNot[0]);

        char ready= 'R';
        //полнимаем "флажок" о готовности к приему
        if(write(wr, &ready, 1)!=1){
            exit(-3);
        }
//Открытие файла вывода
    openFlags = O_CREAT | O_WRONLY | O_TRUNC; 
    filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | 
    S_IROTH | S_IWOTH; /* rw - rw - rw - */ 
        outputFd=open(nameCopy, openFlags, filePerms);
        if (outputFd==-1){
            fprintf(stderr, "Error while opening file\n");
            exit(-3);
        }
//перемещаем данные блоком buf размера BUF_SIZE,
//пока не вернет 0
    while((numRead=read(rd, buf, BUF_SIZE))>0){
        if(write(outputFd, buf, numRead)!=numRead){
            fprintf (stderr, "couldn't write whole buffer\n "); exit(-4); 
        }
    }
    if (numRead == -1) { 
        fprintf (stderr, "read error\n "); exit(-5); 
    } 
    if (close (outputFd ) == -1 ) { 
        fprintf (stderr, "close output error\n"); exit(-7); 
    } 
    exit(EXIT_SUCCESS); 
    }

//РОДИТЕЛЬСКИЙ ПРОЦЕСС
    wr=dataTransfer[1]; //пишем данные
    rd=readyOrNot[0]; //читаем готовность о готовности
    close(dataTransfer[0]); //закрываем концы
    close(readyOrNot[1]);

    char ready= 0;
//Ждем, пока дочерний сообщит о готовности
    if(read(rd, &ready, 1)!=1 || ready!= 'R'){
        exit(-5);
    }

//перемещаем данные, пока файл не закончится (либо вылезет ошибка)
    while((numRead=read(inputFd, buf, BUF_SIZE))>0){
        if (write(wr, buf, numRead) != numRead) { 
        fprintf (stderr, "couldn't write whole buffer\n "); exit(-4); 
    } 
}
    if (numRead == -1) { 
        fprintf (stderr, "read error\n "); exit(-5); 
    } 
    if (close (inputFd ) == -1 ) { 
        fprintf (stderr, "close input error\n"); exit(-6); 
    } 
//закрываем пишущий конец. Дочерний в случае закрытия
//решит, что все передано
    close(wr);
    close(rd);


//просто ждем, пока завершится дочерний процесс
    int status;
    waitpid(pid, &status, 0);
    exit (EXIT_SUCCESS);
}