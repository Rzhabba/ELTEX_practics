#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define BUF_SIZE  1024 
#define NAME_SIZE 256 
#define MAX_FILES 64

//типы служебных сообщений
#define M_READY 1 //флажок готовности 
#define M_FILE  2 //флажок наличия файла
#define M_SKIP  3 //флажок "битого"
#define M_DONE  4 //флажок конца

//заголовок сообщения
typedef struct {
    int       type;
    long long size;
    char      name[NAME_SIZE];
} hdr_t;

//запись/чтение ровно n байт
static int xwrite(int fd, const void *p, size_t n)
{
    const char *q = p; size_t done = 0;
//тут и ниже q- указатель на текущую позицию в буфере
//без этих функций часто возникают ошибки копирования
//done - сколько байт уже записано
    while (done < n) {
        ssize_t k = write(fd, q + done, n - done);
//q+done - сколько уже записано, 
//n-done - сколько осталось записать
        if (k < 0) continue;
//если к <0 -повторяем запись
        done += (size_t)k;
    }
    return done == n ? 0 : -1; //если все записано - вернуть 0, если нет-то -1
}

static int xread(int fd, void *p, size_t n)    // 0-ок, 1-EOF, -1-ошибка
{
    char *q = p; size_t done = 0;
    while (done < n) {
        ssize_t k = read(fd, q + done, n - done);
        if (k < 0) continue;
        if (k == 0) return 1;
        done += (size_t)k;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int inputFd, outputFd, openFlags;
    mode_t filePerms;
    ssize_t numRead;
    char buf[BUF_SIZE];
    char nameCopy[NAME_SIZE + 16];
    char ackName[NAME_SIZE + 16];
    int dataTransfer[2], readyOrNot[2];
    int rd, wr;
    const char *pipename = NULL;

    /* массив копируемых файлов */
    const char *files[MAX_FILES];
    int nfiles = 0;
    hdr_t msg;

    // разбор аргументов: -p <имя канала>, все остальные — имена файлов
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Ошибка: после -p нет имени канала\n");
                exit(-1);
            }
            pipename = argv[++i];
        } else {
            if (nfiles >= MAX_FILES) {
                fprintf(stderr, "Слишком много файлов (максимум %d)\n", MAX_FILES);
                exit(-1);
            }
            files[nfiles++] = argv[i];   // каждый аргумент, который не -р будет файлом
        }
    }
    if (nfiles == 0) {
        fprintf(stderr, "Формат: %s [-p pipename] file1 [file2 ...]\n", argv[0]);
        exit(-1);
    }

    //создание каналов — именованные или неименованные
    if (pipename != NULL) {
        snprintf(ackName, sizeof ackName, "%s.ack", pipename);
        unlink(pipename); unlink(ackName);
        if (mkfifo(pipename, 0666) < 0) { perror(pipename); exit(-1); }
        if (mkfifo(ackName, 0666) < 0)  { perror(ackName);  exit(-1); }
    } else {
        if (pipe(dataTransfer) < 0) { perror("pipe"); exit(-1); }
        if (pipe(readyOrNot) < 0)   { perror("pipe"); exit(-1); }
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(-1); }

    //ДОЧЕРНИЙ ПРОЦЕСС
    if (pid == 0) {
        if (pipename != NULL) {
            rd = open(pipename, O_RDONLY);
            if (rd < 0) { perror(pipename); exit(-3); }
            wr = open(ackName, O_WRONLY);
            if (wr < 0) { perror(ackName); exit(-3); }
        } else {
            rd = dataTransfer[0];
            wr = readyOrNot[1];
            close(dataTransfer[1]);
            close(readyOrNot[0]);
        }

        //бесконечный цикл приёма файлов, выход по M_DONE 
        for (;;) {
            //сообщаем родителю, что готовы принять следующий файл
            memset(&msg, 0, sizeof msg);
            msg.type = M_READY;
            if (xwrite(wr, &msg, sizeof msg) < 0) exit(-3);

            //читаем служебные сообщения от родителя
            if (xread(rd, &msg, sizeof msg) != 0) exit(-3);

            if (msg.type == M_DONE) break;        // все файлы переданы
            if (msg.type == M_SKIP) continue;     // файла нет, идём к следующему
            if (msg.type != M_FILE) exit(-3);

            snprintf(nameCopy, sizeof nameCopy, "%s.copy", msg.name);

            openFlags = O_CREAT | O_WRONLY | O_TRUNC;
            filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
                        S_IROTH | S_IWOTH; /* rw - rw - rw - */
            outputFd = open(nameCopy, openFlags, filePerms);
            if (outputFd == -1) {
                fprintf(stderr, "Error while opening %s\n", nameCopy);
                exit(-3);
            }

            //принимаем ровно msg.size байт содержимого 
            long long left = msg.size;
            while (left > 0) {
                size_t chunk = (left < BUF_SIZE) ? (size_t)left : BUF_SIZE;
                numRead = read(rd, buf, chunk);
                if (numRead <= 0) {
                    fprintf(stderr, "read error\n"); exit(-5);
                }
                if (write(outputFd, buf, numRead) != numRead) {
                    fprintf(stderr, "couldn't write whole buffer\n"); exit(-4);
                }
                left -= numRead;
            }
            if (close(outputFd) == -1) {
                fprintf(stderr, "close output error\n"); exit(-7);
            }
        }
        exit(EXIT_SUCCESS);
    }

    //РОДИТЕЛЬСКИЙ ПРОЦЕСС 
    if (pipename != NULL) {
        wr = open(pipename, O_WRONLY);
        if (wr < 0) { perror(pipename); exit(-5); }
        rd = open(ackName, O_RDONLY);
        if (rd < 0) { perror(ackName); exit(-5); }
    } else {
        wr = dataTransfer[1];
        rd = readyOrNot[0];
        close(dataTransfer[0]);
        close(readyOrNot[1]);
    }

    //цикл по всем файлам
    for (int i = 0; i < nfiles; i++) {
        // ждём готовность от дочернего сообщение о готовности
        if (xread(rd, &msg, sizeof msg) != 0 || msg.type != M_READY) exit(-5);

        // открываем исходный файл; если искомого нет- идем к следующему
        inputFd = open(files[i], O_RDONLY);
        if (inputFd == -1) {
            fprintf(stderr, "Ошибка. Файл '%s' не существует, пропускаем\n", files[i]);
            memset(&msg, 0, sizeof msg);
            msg.type = M_SKIP;
            xwrite(wr, &msg, sizeof msg);
            continue;
        }

        struct stat st;
        fstat(inputFd, &st);

        // первое сообщение: имя и размер файла
        memset(&msg, 0, sizeof msg);
        msg.type = M_FILE;
        msg.size = (long long)st.st_size;
        snprintf(msg.name, sizeof msg.name, "%s", files[i]);
        if (xwrite(wr, &msg, sizeof msg) < 0) exit(-5);

        // пересылка содержимого блоками
        while ((numRead = read(inputFd, buf, BUF_SIZE)) > 0) {
            if (write(wr, buf, numRead) != numRead) {
                fprintf(stderr, "couldn't write whole buffer\n"); exit(-4);
            }
        }
        if (numRead == -1) { fprintf(stderr, "read error\n"); exit(-5); }
        if (close(inputFd) == -1) { fprintf(stderr, "close input error\n"); exit(-6); }
    }

    //пересылаем ребенку сообщение о конце
    memset(&msg, 0, sizeof msg);
    msg.type = M_DONE;
    xwrite(wr, &msg, sizeof msg);

    close(wr);
    close(rd);

    int status;
    waitpid(pid, &status, 0);

    if (pipename != NULL) { //отвязываемся
        unlink(pipename);
        unlink(ackName);
    }
    exit(EXIT_SUCCESS);
}