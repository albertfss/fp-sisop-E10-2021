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
void grantPermissionCust(int, char *);
void printLog(char*);
void use(int, char*);
void dropTable(int, char*);
void dropDB(int, char*);
void insertInto(int, char*);

int getInput(int fd, char *prompt, char *storage);
char *getFileName(char *filePath);
bool validLogin(FILE *fp, char *msg);
bool isRegistered(FILE *fp, char *id);
void parseFilePath(char *filepath, char *raw_filename, char *ext);
void getUserPass(char *id, char *pass, char *message, int *symbol);
void getUserDb(char*,char*,char*);
bool isSymbol(char);
void getTime(char * dest);
void parseDB(char*,char*,char*);
void parseTable(char*,char*,char*);
void getUser(char*,char*);

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
    }
    else {
        strcpy(prompt, "root# ");
        logon = 1;
    }

    while (recv(fd, cmd, DATA_BUFFER, MSG_PEEK | MSG_DONTWAIT) != 0 && logon == 1) {
        if (getInput(fd, prompt, cmd) == 0) break;
        printLog(cmd);

        if (strstr(cmd, ":CREATE USER ") != NULL && strstr(cmd, " IDENTIFIED BY ") != NULL) {
            regist(fd, cmd);
            
        } 
        else if(strstr(cmd, ":CREATE DATABASE ") != NULL) {
            createDB(fd,cmd);
            
        }
        else if(strstr(cmd, ":GRANT PERMISSION ") != NULL && strstr(cmd, " INTO ") != NULL) {
            grantPermission(fd,cmd);
            
        }
        else if(strstr(cmd, ":USE ") != NULL) {
            use(fd, cmd);
            
        }
        else if(strstr(cmd, ":CREATE TABLE ") != NULL) {
            createTable(fd, cmd);
        }
        else if(strstr(cmd, ":DROP TABLE ") != NULL) {
            dropTable(fd, cmd);
        }
        else if(strstr(cmd, ":INSERT INTO ") != NULL) {
            insertInto(fd, cmd);
        }
        else if(strstr(cmd, ":\exit") != NULL|| strstr(cmd, ":-q") != NULL) {
            close(fd);
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
    char *ptrDb = strstr(message, "CREATE DATABASE");
    char tmpdb[1000] = {};
    strcpy(tmpdb, ptrDb);
    int space = 0;
    for(int i=0; i< strlen(tmpdb); i++) {
        if(tmpdb[i] == ' ') space++;
        if(space > 2) {
            send(fd, "Invalid Command or Argument\n", SIZE_BUFFER, 0);
            return;
        }
    }
    if(space < 2) {
        send(fd, "Invalid Command or Argument\n", SIZE_BUFFER, 0);
        return;
    }

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
            
            char tmp[DATA_BUFFER] = {0};
            strcpy(tmp, message);
            char *token = strtok(tmp, ":");
            char user[DATA_BUFFER] = {0};
            if(token != NULL){
                char *ptrTkn = strstr(token, "/-");
                if(ptrTkn != NULL) {
                    char tmpTkn[DATA_BUFFER];
                    strcpy(tmpTkn, ptrTkn);
                    for(int j=0; j<strlen(tmpTkn)-2; j++) {
                        user[j] = tmpTkn[j+2];
                    }
                } else
                    strcpy(user, token);
                char perm[DATA_BUFFER] = {0};
                strcpy(perm, user);
                strcat(perm, ":");
                strcat(perm, dbName);
                grantPermissionCust(fd, perm);
            }
        }
        else {
            send(fd, "Database already exists\n", SIZE_BUFFER, 0);
        }
    }
    fclose(fp);
}

void createTable(int fd, char *message) {
    char cmd[DATA_BUFFER] = {0};
    char db[DATA_BUFFER] = {0};
    if(strstr(message, "/-") == NULL) {
        send(fd, "Use Database first\n", SIZE_BUFFER, 0);
        return;
    }
    parseDB(message,cmd,db);
    char tName[DATA_BUFFER] = {0};
    char tVal[DATA_BUFFER] = {0};
    parseTable(message, tName, tVal);
    char dest1[DATA_BUFFER];
    strcpy(dest1, db);
    strcat(dest1, tName);
    FILE *fp1 = fopen(dest1, "r");
    if(fp1 == NULL) {
        char query[DATA_BUFFER][DATA_BUFFER] = {0};
        int i = 0,j = 0,k = 0;
        // if(tVal[strlen(tVal-1)] != ')' || strstr(tVal, "(") == NULL) {
        //     send(fd, "Invalid Argument\n", SIZE_BUFFER, 0);
        //     return;
        // }
        
        while(k < strlen(tVal)) {
            if(k >0 && tVal[k-1] == ',' && tVal[k] == ')') {
                send(fd, "Invalid Argument\n", SIZE_BUFFER, 0);
                return;
            }
            if(!isSymbol(tVal[k])) {
                query[i][j] = tVal[k];
                j++;
            } 
            else if(tVal[k] == '(' || tVal[k] == ')') {
                k++;
                continue;
            }
            else if(tVal[k] == ',') {
                j=0;
                i++;
            }
            else if(tVal[k] == ' ') {
                if(k+1 < strlen(tVal)) {
                    if(!isSymbol(tVal[k-1]) && !isSymbol(tVal[k+1])) {
                        query[i][j] = ' ';
                        j++;
                    }
                }
            }
            k++;
        }
        for(int a=0; a<=i; a++) {
            int space = 0;
            for(int b=0; b<strlen(query[a]); b++) {
                if(query[a][b] == ' ') space++;
            }
            if(space > 1) {
                send(fd, "Invalid Query\n", SIZE_BUFFER, 0);
                return;
            }
        }
        char dataType[DATA_BUFFER] = {0};
        char var[DATA_BUFFER] = {0};
        for(int line=0; line <= i; line++) {
            char *token = strtok(query[line], " ");
            if(token != NULL) {
                if(var[0] == '\0') {
                    strcpy(var, token);
                } else {
                    strcat(var, "\t");
                    strcat(var, token);
                }
            }
            token = strtok(NULL, " ");
            if(token != NULL) {
                if(strcmp(token, "int") == 0 || strcmp(token, "string") == 0) {
                    if(dataType[0] == '\0') {
                        strcpy(dataType, token);
                    } else {
                        strcat(dataType, "\t");
                        strcat(dataType, token);
                    }
                }
                else {
                    send(fd, "Invalid Data Type!\n", SIZE_BUFFER, 0);
                    return;
                }
            } else {
                send(fd, "Invalid Query\n", SIZE_BUFFER, 0);
                return;
            }
        }
        FILE *fp = fopen(dest1, "w+");
        if(fp == NULL) {
            send(fd, "Unknown Error Occurred\n", SIZE_BUFFER, 0);
            return;
        }
        fprintf(fp, "%s\n", dataType);
        fprintf(fp, "%s\n", var);
        send(fd, "Table Created\n", SIZE_BUFFER, 0);
        fclose(fp);
        return;
    }
    send(fd, "Table already exist\n", SIZE_BUFFER, 0);
    fclose(fp1);
}

void use(int fd, char *cmd) {
    char db[DATA_BUFFER] = {0};
    int i=0, start=4;
    char *ptrCmd = strstr(cmd, "USE ");
    char tmpCmd[1000] = {0};
    strcpy(tmpCmd, ptrCmd);
    while(i < strlen(tmpCmd)-4) {
        db[i] = tmpCmd[start];
        i++; start++;
    }
    FILE *fdb = fopen("DbAkun/listDB", "r+");
    if(fdb == NULL) {
        send(fd, "Unknown error\n", SIZE_BUFFER, 0);
    }
    char tmpfdb[1000];
    bool matchDb = false;
    while(fscanf(fdb, "%s", tmpfdb) != EOF) {
        if(strcmp(tmpfdb, db) == 0) {
            matchDb = true;
            break;
        }
    }
    if(!matchDb) send(fd, "Cannot find such Database\n", SIZE_BUFFER, 0);
    else {
        char temp[DATA_BUFFER] = {0};
        char user[DATA_BUFFER] = {0};
        getUser(cmd, user);
        if(strcmp("root", user) != 0) {
            FILE *fp = fopen("DbAkun/permissionList", "r+");
            char comb[DATA_BUFFER];
            strcpy(comb, user);
            strcat(comb, ":");
            strcat(comb, db);
            bool match = false;
            char file[DATA_BUFFER] = {0};
            while (fscanf(fp, "%s", file) != EOF) {
                if (strcmp(file, comb) == 0) match = true;
            }
            if(!match) {
                send(fd, "Forbidden!\n", SIZE_BUFFER, 0);
                return;
            }
        }
        strcpy(temp, "USING:");
        strcat(temp, db);
        strcat(temp, "\n");
        
        send(fd, temp, SIZE_BUFFER, 0);
    }
    fclose(fdb);
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

    if(strcmp(message, "INVALID ARG") == 0) {
        send(fd, "Invalid Command or Argument\n", SIZE_BUFFER, 0);
    }
    else if(cmpUser == 0 && cmpDB == 0) {
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

void grantPermissionCust(int fd, char *message) {
    FILE *fp = fopen("DbAkun/permissionList", "a+");
    if(fp == NULL) {
        printf("Error opening permissionList\n");
        return;
    }
    fprintf(fp, "%s\n", message);

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
    int space = 0;
    for(int i=0; i< strlen(tmpDb); i++) {
        if(tmpDb[i] == ' ') space++;
        if(space > 4) {
            strcpy(message, "INVALID ARG");
            return;
        }
    }
    if(space < 4) {
        strcpy(message, "INVALID ARG");
        return;
    }
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
    strftime(buffer,sizeof(buffer),"%Y-%m-%d %X",&tm);
    strcpy(dest,buffer);
}

void printLog(char *str) {
    FILE *fp = fopen("logging.log", "a+");
    char timestamp[100];
    getTime(timestamp);
    char *token;

    if(strstr(str, "-") != NULL) {
        char temp[DATA_BUFFER] = {0};
        strcpy(temp, str);
        token = strtok(temp, "-");
        token = strtok(NULL, "-");
        if(token != NULL) {
            fprintf(fp, "%s:%s\n", timestamp, token);
        }
    } else {
        fprintf(fp, "%s:%s\n", timestamp, str);
    }

    fclose(fp);
}

void parseDB(char *message, char *cmd, char *db) {
    char tmp[DATA_BUFFER];
    strcpy(tmp, message);
    char *token = strtok(tmp, "-");
    if(token != NULL) {
        printf("Database : [%s]\n", token);
        strcpy(db, token);
        token = strtok(NULL, "-");
        if(token != NULL) strcpy(cmd, token);
    }
}

void parseTable(char *cmd, char *tName, char *tVal) {
    memset(tName, 0, DATA_BUFFER);
    memset(tVal, 0, DATA_BUFFER);
    char *ptrN = strstr(cmd, "CREATE TABLE");
    char tmpN[DATA_BUFFER] = {0};
    strcpy(tmpN, ptrN+strlen("CREATE TABLE "));

    char *token = strtok(tmpN, " ");
    if(token != NULL) {
        strcpy(tName, token);
    }
    char *ptrQ = strstr(cmd, tName);
    strcpy(tVal, ptrQ + strlen(tName));
}

void dropTable(int fd, char* cmd) {
    if(strstr(cmd, "/-") == NULL) {
        send(fd, "Use Database first\n", SIZE_BUFFER, 0);
        return;
    }
    char tb[DATA_BUFFER] = {0};
    char *ptr = strstr(cmd, "DROP TABLE ");
    strcpy(tb, ptr+strlen("DROP TABLE "));
    char *token = strtok(cmd, "-");
    char db[DATA_BUFFER] = {0};
    if(token != NULL) {
        strcpy(db, token);
    }
    char dest[DATA_BUFFER];
    strcpy(dest, db);
    strcat(dest, "/");
    strcat(dest, tb);
    FILE *fp = fopen(dest, "r");
    if(fp == NULL) {
        send(fd, "Table not found\n", SIZE_BUFFER, 0);
        return;
    }
    fclose(fp);
    remove(dest);
    send(fd, "Table Dropped\n", SIZE_BUFFER, 0);
}

void dropDB(int fd, char*cmd) {
    char db[DATA_BUFFER] = {0};
    char *ptr = strstr(cmd, "DROP DATABASE ");
    strcpy(db, ptr+strlen("DROP DATABASE "));
    FILE *fp = fopen("DbAkun/listDB", "a+");

    
}

void getUser(char* cmd, char *user) {
    memset(user, 0, DATA_BUFFER);
    if(strstr(cmd, "/-") != NULL) {
        char tmp[DATA_BUFFER];
        strcpy(tmp, cmd);
        char *token = strtok(tmp, "-");
        if(token != NULL) {
            token = strtok(NULL, ":");
            if(token != NULL)
                strcpy(user,token);
        }
    } else {
        char tmp[DATA_BUFFER];
        strcpy(tmp, cmd);
        char *token = strtok(tmp, ":");
        if(token != NULL)
            strcpy(user,token);
    }
}

void insertInto(int fd, char *cmd) {
    char msg[DATA_BUFFER] = {0};
    char db[DATA_BUFFER] = {0};
    if(strstr(cmd, "/-") == NULL) {
        send(fd, "Use Database first\n", SIZE_BUFFER, 0);
        return;
    }
    parseDB(cmd,msg,db);
    char tName[DATA_BUFFER];
    char tVal[DATA_BUFFER];
    memset(tName, 0, DATA_BUFFER);
    memset(tVal, 0, DATA_BUFFER);
    char *ptrN = strstr(cmd, "INSERT INTO");
    char tmpN[DATA_BUFFER] = {0};
    strcpy(tmpN, ptrN+strlen("INSERT INTO "));

    if(strstr(tmpN, " (") != NULL) {
        char *token = strtok(tmpN, " ");
        if(token != NULL) {
            strcpy(tName, token);
        }
        char *ptrQ = strstr(cmd, tName);
        strcpy(tVal, ptrQ + strlen(tName));
    } else {
        char *ptr = strstr(tmpN, "(");
        strcpy(tVal, ptr);
    }

    char dest1[DATA_BUFFER];
    strcpy(dest1, db);
    strcat(dest1, tName);
    FILE *fp1 = fopen(dest1, "a+");
    if(fp1 == NULL) {
        send(fd, "Table not found\n", SIZE_BUFFER, 0);
        return;
    }
    char query[DATA_BUFFER][DATA_BUFFER] = {0};
    int i = 0,j = 0,k = 0;
    while(k < strlen(tVal)) {
        if(k >0 && tVal[k-1] == ',' && tVal[k] == ')') {
            send(fd, "Invalid Argument\n", SIZE_BUFFER, 0);
            return;
        }
        if(!isSymbol(tVal[k])) {
            query[i][j] = tVal[k];
            j++;
        } 
        else if(tVal[k] == '(' || tVal[k] == ')') {
            k++;
            continue;
        }
        else if(tVal[k] == ',') {
            j=0;
            i++;
        }
        else if(tVal[k] == ' ') {
            if(k+1 < strlen(tVal)) {
                if(!isSymbol(tVal[k-1]) && !isSymbol(tVal[k+1])) {
                    query[i][j] = ' ';
                    j++;
                }
            }
        }
        else if(tVal[k] == '\'') {
            query[i][j] = '\'';
            j++;
        }
        k++;
    }
    int col=0;
    char tempF[DATA_BUFFER] = {0};
    if (fgets(tempF, SIZE_BUFFER, fp1) != NULL) {
        int tab = 0;
        for(int a = 0; a<strlen(tempF); a++) {
            if(tempF[a] == '\t')
                tab++;
        }
        col = tab+1;
        if(col != i+1) {
            send(fd, "Invalid Argument", SIZE_BUFFER, 0);
            fclose(fp1);
            return;
        }
    }
    rewind(fp1);
    char tempFile[DATA_BUFFER] = {0};
    char dataType[DATA_BUFFER][DATA_BUFFER];
    int index = 0;
    while(fscanf(fp1, "%s", tempFile)!= EOF) {
        if(strcmp(tempFile, "int") == 0) {
            strcpy(dataType[index], "int");
            for(int i=0; i<strlen(query[index]); i++) {
                if((query[index][i] <= '0' && query[index][i] >= '9')) {
                    send(fd, "Value is not int\n", SIZE_BUFFER, 0);
                    fclose(fp1);
                    return;
                }
            }
            index++;
        } else if(strcmp(tempFile, "string") == 0) {
            strcpy(dataType[index], "string");
            if(query[index][0] != '\'' || query[index][strlen(query[index])-1] != '\'') {
                send(fd, "Invalid Query\n", SIZE_BUFFER, 0);
                fclose(fp1);
                return;
            }
            index++;
        } else {
            break;
        }
    }
    char push[DATA_BUFFER] = {0};
    for(int i=0; i<col; i++) {
        if(i == 0) {
            strcpy(push, query[i]);
        }
        else {
            strcat(push, "\t");
            strcat(push, query[i]);
        }
    }
    fprintf(fp1, "%s\n", push);
    send(fd, "Data Inserted\n", SIZE_BUFFER, 0);
    fclose(fp1);
}
