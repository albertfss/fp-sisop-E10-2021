#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <wait.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TRUE   1 
#define FALSE  0 
#define PORT 8080 
#define DATA_BUFFER 1024

int curr_fd = -1;
char auth_user[2][DATA_BUFFER]; // [0] => id, [1] => pass
const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;

int create_tcp_server_socket();

void *menu(void *argv);
void login(int fd, char *message, int *logon);
void regist(int fd, char *cmd);
void _log(char *cmd, char *filepath);
void createTable(int fd, char *cmd);
void createDB(int fd, char *cmd);
void grantPermission(int, char *);
void printLog(char*);

int getInput(int fd, char *prompt, char *storage);
char *getFileName(char *filePath);
bool validLogin(FILE *fp, char *msg);
bool isRegistered(FILE *fp, char *id);
void parseFilePath(char *filepath, char *raw_filename, char *ext);
void getUserPass(char *id, char *pass, char *message, int *symbol);
void getUserDb(char*,char*,char*);
bool isSymbol(char);
void getTime(char * dest);

int main() {
    socklen_t addrlen;
    struct sockaddr_in new_addr;
    pthread_t tid;
    char buff[DATA_BUFFER];
    int server_fd = create_tcp_server_socket();
    int new_fd;
    char *dirName = "DbAkun";
    mkdir(dirName, 0777);

    if(server_fd) {
        while (TRUE) {
            new_fd = accept(server_fd, (struct sockaddr *)&new_addr, &addrlen);
            if (new_fd >= 0) {
                printf("Accepted a new connection with fd: %d\n", new_fd);
                pthread_create(&tid, NULL, &menu, (void *) &new_fd);
            } else {
                fprintf(stderr, "Accept failed [%s]\n", strerror(errno));
            }
        } 
    }
}


int create_tcp_server_socket()
{
    struct sockaddr_in address;
    int opt = TRUE, fd;
    int addrlen = sizeof(address);

    char buffer[1028];

    fd_set readfds;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
      
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        printf("%d\n",fd);
        perror("tolol");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
      
    if (bind(fd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return fd;
}

void *menu(void *argv)
{
    int fd = *(int *) argv;
    char cmd[DATA_BUFFER];

    char message[DATA_BUFFER] = {};
    char prompt[DATA_BUFFER];
    int logon = 0;

    recv(fd, message, DATA_BUFFER, 0);
    if(strcmp(message, "root") != 0) {
        login(fd, message, &logon);
        int i;
        for(i=0; i<strlen(message); i++) {
            if(message[i] == ':') break;
            prompt[i] = message[i];
        }
        prompt[i] = '#';
        prompt[++i] = ' ';
        printf("%sx\n",prompt);
        printf("msg : %s\n",message);
    }
    else {
        strcpy(prompt, "root# ");
        logon = 1;
    }

    while (recv(fd, cmd, DATA_BUFFER, MSG_PEEK | MSG_DONTWAIT) != 0 && logon == 1) {
        if (getInput(fd, prompt, cmd) == 0) break;
        if (strstr(cmd, ":CREATE USER ") != NULL && strstr(cmd, " IDENTIFIED BY ") != NULL) {
            regist(fd, cmd);
            printLog(cmd);
        } 
        else if(strstr(cmd, ":CREATE DATABASE ") != NULL) {
            createDB(fd,cmd);
            printLog(cmd);
        }
        else if(strstr(cmd, ":GRANT PERMISSION ") != NULL && strstr(cmd, " INTO ") != NULL) {
            grantPermission(fd,cmd);
            printLog(cmd);
        }
        else {
            send(fd, "Error: Invalid command\n", SIZE_BUFFER, 0);
        }
    }
    close(fd);
}

void login(int fd, char *message, int *logon)
{
    char id[DATA_BUFFER], password[DATA_BUFFER];
    FILE *fp = fopen("DbAkun/tableAkun", "r+");
    if(fp == NULL) {
        send(fd, "Database for user not found!\n", SIZE_BUFFER, 0);
        close(fd);
        return;
    }

    if (validLogin(fp, message)) {
        curr_fd = fd;
        strcpy(auth_user[0], id);
        strcpy(auth_user[1], password);
        *logon = 1;
    } else {
        send(fd, "Wrong Username or Password\n", SIZE_BUFFER, 0);
        close(fd);
    }
    fclose(fp);
}

void regist(int fd, char *cmd)
{
    char id[DATA_BUFFER], password[DATA_BUFFER];
    FILE *fp = fopen("DbAkun/tableAkun", "a+");
    if(fp == NULL) {
        send(fd, "Cannot create table\n", SIZE_BUFFER, 0);
        return;
    }
    int symbol = 0;
    getUserPass(id, password, cmd, &symbol);

    if (isRegistered(fp, id)) {
        send(fd, "Username is already registered\n", SIZE_BUFFER, 0);
    } 
    else if(symbol == 1) {
        send(fd, "Username and Password only contain alphanumeric\n", SIZE_BUFFER, 0);
    }
    else if(symbol == 2) {
        send(fd, "Invalid argument\n", SIZE_BUFFER, 0);
    }
    else {
        fprintf(fp, "%s:%s\n", id, password);
        send(fd, "User created\n", SIZE_BUFFER, 0);
    }
    fclose(fp);
}

void createDB(int fd, char *message) {
    char *ptrUsr = strstr(message, "CREATE DATABASE");
    char tmpdb[1000] = {};
    strcpy(tmpdb, ptrUsr);
    char dbName[1000] = {};
    int start=16; int i=0;
    while(i < strlen(tmpdb)-16) {
        dbName[i] = tmpdb[start];
        if(isSymbol(dbName[i])) {
            send(fd, "Database name only contain alphanumeric\n", SIZE_BUFFER, 0);
            return;
        }
        i++; start++;
    }
    FILE *fp = fopen("DbAkun/listDB", "a+");
    if(fp == NULL) {
        send(fd, "unknown error, cannot create database\n", DATA_BUFFER, 0);
    } else {
        fprintf(fp, "%s\n", dbName);
        if(mkdir(dbName, 0777) == 0) {
            send(fd, "Database Created\n", SIZE_BUFFER, 0);
        }
        else {
            send(fd, "Database already exists\n", SIZE_BUFFER, 0);
        }
    }
    fclose(fp);
}

char *getFileName(char *filePath)
{
    char *ret = strrchr(filePath, '/');
    if (ret) return ret + 1;
    else return filePath;
}

bool validLogin(FILE *fp, char *message)
{
    char db[DATA_BUFFER], input[DATA_BUFFER];
    while (fscanf(fp, "%s", db) != EOF) {
        if (strcmp(db, message) == 0) return true;
    }
    return false;
}

bool isRegistered(FILE *fp, char *id)
{
    char db[DATA_BUFFER], *tmp;
    while (fscanf(fp, "%s", db) != EOF) {
        tmp = strtok(db, ":");
        if (strcmp(tmp, id) == 0) return true;
    }
    return false;
}

void grantPermission(int fd, char *message) {
    char user[DATA_BUFFER], db[DATA_BUFFER];
    FILE *fp = fopen("DbAkun/permissionList", "a+");
    if(fp == NULL) {
        send(fd, "Cannot create table\n", SIZE_BUFFER, 0);
        return;
    }
    getUserDb(user, db, message);
    int cmpUser = strcmp(user, "NO ACCOUNT FOUND");
    int cmpDB = strcmp(db, "NO DATABASE FOUND");

    if(cmpUser == 0 && cmpDB == 0) {
        send(fd, "Database and Account not found\n", SIZE_BUFFER, 0);
    }
    else if(cmpUser == 0) {
        send(fd, "Account not found\n", SIZE_BUFFER, 0);
    }
    else if(cmpDB == 0) {
        send(fd, "Database not found\n", SIZE_BUFFER, 0);
    }
    else {
        char comb[2050] = {0};
        sprintf(comb, "%s:%s", user, db);
        char plist[DATA_BUFFER] = {0};
        char *token;
        bool isInside = false;
        char newLine[DATA_BUFFER];
        while(fscanf(fp, "%s", plist) != EOF) {
            if(strcmp(comb, plist) == 0) {
                send(fd, "Permission already Granted\n", SIZE_BUFFER, 0);
                fclose(fp);
                return;
            }
        }
        
        fprintf(fp, "%s:%s\n", user, db);
        send(fd, "Permission Granted\n", SIZE_BUFFER, 0);
    }
    fclose(fp);
}

int getInput(int fd, char *prompt, char *storage)
{
    send(fd, prompt, SIZE_BUFFER, 0);

    int valread = read(fd, storage, DATA_BUFFER);
    if (valread != 0) printf("Input: [%s]\n", storage);
    return valread;
}

void getUserPass(char *id, char *pass, char *message, int *symbol) {
    char *ptrUsr = strstr(message, "CREATE USER");
    memset(pass, 0, SIZE_BUFFER);
    memset(id, 0, SIZE_BUFFER);
    int space = 0;
    char tmpUsr[1000] = {};
    strcpy(tmpUsr, ptrUsr);
    for(int i=0; i< strlen(tmpUsr); i++) {
        if(tmpUsr[i] == ' ') space++;
        if(space > 5) {
            *symbol = 1;
            return;
        }
    }
    if(space < 5) {
        *symbol = 2;
        return;
    }
    int start=12; int i=0;
    while(tmpUsr[start] != ' ') {
        id[i] = tmpUsr[start];
        if(isSymbol(id[i])) {
            *symbol = 1;
            return;
        }
        i++; start++;
    }

    char *ptrPass = strstr(message, "IDENTIFIED BY");
    char tmpPass[1000] = {};
    strcpy(tmpPass, ptrPass);
    start=14; i=0;
    while(i < strlen(tmpPass)-14) {
        pass[i] = tmpPass[start];
        if(isSymbol(pass[i])) {
            *symbol = 1;
            return;
        }
        i++; start++;
    }
}

void getUserDb(char *user, char *db, char *message) {
    memset(user, 0, SIZE_BUFFER);
    memset(db, 0, SIZE_BUFFER);
    char *ptrDb = strstr(message, "GRANT PERMISSION");
    char tmpDb[1000] = {};
    strcpy(tmpDb, ptrDb);
    int start=strlen("GRANT PERMISSION "); int i=0;
    while(tmpDb[start] != ' ') {
        db[i] = tmpDb[start];
        i++; start++;
    }
    FILE *fdb = fopen("DbAkun/listDB", "r+");
    char tmpfdb[1000];
    bool matchDb = false;
    int idb = 0;
    while(fscanf(fdb, "%s", tmpfdb) != EOF) {
        if(strcmp(tmpfdb, db) == 0) {
            matchDb = true;
            break;
        }
        idb++;
    }
    if(!matchDb) strcpy(db, "NO DATABASE FOUND");

    char *ptrUser = strstr(message, "INTO");
    char tmpUser[1000] = {};
    strcpy(tmpUser, ptrUser);
    start=strlen("INTO "); i=0;
    int tempStart = start;
    while(i < strlen(tmpUser)-tempStart) {
        user[i] = tmpUser[start];
        i++; start++;
    }
    FILE *fda = fopen("DbAkun/tableAkun", "r+");
    char tmpfda[1000];
    bool matchAccount = false;
    char *tmp;
    while(fscanf(fda, "%s", tmpfda) != EOF) {
        tmp = strtok(tmpfda, ":");
        if(strcmp(tmp, user) == 0) {
            matchAccount = true;
            break;
        }
    }
    if(!matchAccount) strcpy(user, "NO ACCOUNT FOUND");
    fclose(fda);
    fclose(fdb);
}


bool isSymbol(char c) {
    if( !((c >= 'a') && (c <= 'z')) && !((c >= 'A') && (c <= 'Z')) 
            && !((c >= '0') && (c <= '9')) ) {
        return true;
    }
    return false;
}

void getTime(char * dest){
    char buffer[10000];
    memset(buffer,0,sizeof(buffer));
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(buffer,sizeof(buffer),"%y-%m-%d %X",&tm);
    strcpy(dest,buffer);
}

void printLog(char *str) {
    FILE *fp = fopen("logging.log", "a+");
    char timestamp[100];
    getTime(timestamp);
    fprintf(fp, "%s:%s\n", timestamp, str);
    fclose(fp);
}