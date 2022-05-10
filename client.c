#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 5000
#define MAXBUF 4096

void
strip_newline(char* s)
{
    while (*s != '\0') {
        if (*s == '\r' || *s == '\n') {
            *s = '\0';
        }
        s++;
    }
}

void*
receiver(void* sockID)
{
    int cli_sock = *((int*)sockID);
    char bufin[MAXBUF];
    int rlen;
    while (1) {
        char data[MAXBUF];
        int r = recv(cli_sock, data, MAXBUF, 0);
        if (r <= 0) {
            break;
        }
        strip_newline(data);
        printf("\n(recv): %s\n", data);
    }
    pthread_detach(pthread_self());
}

int
main()
{
    int client_socket, ret;
    struct sockaddr_in server;
    char buffer[MAXBUF];

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        printf("error in connection\n");
        exit(EXIT_FAILURE);
    }
    printf("client socket created\n");

    memset(&server, '\0', sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    ret = connect(client_socket, (struct sockaddr*)&server, sizeof(server));
    if (ret < 0) {
        perror("connect");
        exit(1);
    }
    printf("Connected to server\n");

    pthread_t thread;
    pthread_create(&thread, NULL, &receiver, (void*)&client_socket);

    while (1) {
        // scanf("%s", &buffer);
        fgets(buffer, MAXBUF, stdin);
        // buffer[strcspn(buffer, "\n")] = '\0';
        // printf("sending: %s\n", buffer);
        send(client_socket, buffer, strlen(buffer), 0);

        if (strcmp(buffer, "/quit\n") == 0) {
            close(client_socket);
            printf("Disconnected from server\n");
            exit(1);
        }
    }

    return 0;
}
