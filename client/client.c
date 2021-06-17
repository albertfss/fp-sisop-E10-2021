#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

#define PORT 8080
#define DATA_BUFFER 1024

const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;
char inputPath[DATA_BUFFER];
bool _inputPath = false;
bool root = false;
char user[DATA_BUFFER] = {};

int create_tcp_client_socket();
void *handleInput(void *client_fd);
void *handleOutput(void *client_fd);
void getServerInput(int fd, char *input);
void sendFile(int fd);
void writeFile(int fd);
void getStr(char *message);

int main(int argc, char** argv)
{
    pthread_t tid[2];
    int client_fd = create_tcp_client_socket();
    if(client_fd == -1) return 0;

    char message[DATA_BUFFER] = {};
    if(!geteuid()) {
        root = true;
        strcpy(user, "root");
        sprintf(message, "root");
    }
    else {
        if(argc != 5 || strcmp(argv[1], "-u") != 0 || strcmp(argv[3], "-p") != 0) {
            printf("Invalid argument!\n");
            return 0;
        }
        strcpy(user, argv[2]);
        sprintf(message, "%s:%s",argv[2], argv[4]);
    }
    send(client_fd, message, DATA_BUFFER, 0);

    pthread_create(&(tid[0]), NULL, &handleOutput, (void *) &client_fd);
    pthread_create(&(tid[1]), NULL, &handleInput, (void *) &client_fd);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    close(client_fd);
    return 0;
}

int create_tcp_client_socket()
{
    struct sockaddr_in serv_addr;
    int sock;
    int opt = 1;
    struct hostent *local_host; 

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
  
    memset(&serv_addr, '0', sizeof(serv_addr));
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
      
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
  
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    return sock;
}

void *handleInput(void *client_fd)
{
    int fd = *(int *) client_fd;

    while (1) {
        char message[DATA_BUFFER] = {0};
        getStr(message);
        // printf("%s", message);
        if (!root) {
            if(strstr(message, " CREATE USER ") != NULL) {
                printf("Access denied!\n%s# ", user);
                continue;
            }
            else if(strstr(message, " GRANT PERMISSION ") != NULL) {
                printf("Access denied!\n%s# ", user);
                continue;
            } else {
                send(fd, message, SIZE_BUFFER, 0);    
            }
        }
        else 
            send(fd, message, SIZE_BUFFER, 0);
    }
}

void *handleOutput(void *client_fd) 
{
    int fd = *(int *) client_fd;
    char message[DATA_BUFFER] = {0};

    while (1) {
        memset(message, 0, SIZE_BUFFER);
        if (read(fd, message, DATA_BUFFER) == 0) {
            printf("Server shutdown\n");
            exit(EXIT_SUCCESS);
        }
        printf("%s", message);
        if(strcmp("Database for user not found!\n", message) == 0) {
            // close(fd);
        }
        if(strcmp("Wrong Username or Password\n", message) == 0) {
            // close(fd);
        }
        fflush(stdout);
    }
}

void getServerInput(int fd, char *input)
{
    if (read(fd, input, DATA_BUFFER) == 0) {
        printf("Server shutdown\n");
        exit(EXIT_SUCCESS);
    }
}

void getStr(char *message) {
    char bufferInput = '0';
    int i=0;
    for(i; i<strlen(user); i++) {
        message[i] = user[i];
    }
    message[i] = ':';
    i++;

    while(bufferInput != ';') {
        bufferInput = getchar();
        if(bufferInput == '\n') {
            bufferInput = ' ';
        }
        message[i++] = bufferInput;
        int temp = i-1;
        if(message[temp] == ' ' && message[temp-1] == ' ') {
            i--;
        }
        if(message[temp] == ';' && message[temp-1] == ' ') {
            i--;
        }
        if(message[temp] == ' ' && message[temp-1] == ':') {
            i--;
        }
    }
    message[i-1] = '\0';
}