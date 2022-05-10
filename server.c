#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 4096
#define PORT 5000
#define MAX_CLIENTS 10

static unsigned int client_count;
static unsigned int uid = 1;

typedef struct {
    struct sockaddr_in addr; /* remote address */
    int connfd;              /* conn file descriptor */
    int uid;                 /* unique identifier */
} client_t;

client_t* clients[MAX_CLIENTS];

pthread_mutex_t client_mut = PTHREAD_MUTEX_INITIALIZER;

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

/* Add client to list */
void
add_client(client_t* cl)
{
    pthread_mutex_lock(&client_mut);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&client_mut);
}

/* Delete client from list */
void
del_client(int uid)
{
    pthread_mutex_lock(&client_mut);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->uid == uid) {
                clients[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&client_mut);
}

/* Send message to all clients but the sender */
void
send_all_but_self(char* s, int uid)
{
    pthread_mutex_lock(&client_mut);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->uid != uid) {
                if (write(clients[i]->connfd, s, strlen(s)) < 0) {
                    perror("send_all_but_self(write)");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&client_mut);
}

/* Send message to single client */
void
send_to(char* s, int dest_uid)
{
    pthread_mutex_lock(&client_mut);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]->uid == dest_uid) {
            if (write(clients[i]->connfd, s, strlen(s)) < 0) {
                perror("send_to(write)");
                break;
            }
            break;
        }
    }
    pthread_mutex_unlock(&client_mut);
}

/* Send message to all clients */
void
send_all(char* s)
{
    pthread_mutex_lock(&client_mut);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (write(clients[i]->connfd, s, strlen(s)) < 0) {
                perror("send_all(write)");
                break;
            }
        }
    }
    pthread_mutex_unlock(&client_mut);
}

void*
client_handler(void* arg)
{
    char bufout[MAXLINE], bufin[MAXLINE];
    int rlen;

    client_count++;
    // converting arg
    client_t* cli = (client_t*)arg;
    printf("Connected to participant %d \n", cli->uid);
    while (1) {
        rlen = read(cli->connfd, bufin, sizeof(bufin) - 1);
        if (rlen <= 0) {
            break;
        }
        bufin[rlen] = '\0';
        bufout[0] = '\0';
        strip_newline(bufin);

        /* Ignore empty buffer */
        if (!strlen(bufin)) {
            printf("recv: %s\n", bufin);
            continue;
        }
        /* quit option */
        if (bufin[0] == '/') {
            char *command, *param;
            command = strtok(bufin, " ");
            if (!strcmp(command, "/quit")) {
                break;
            }
            else if (!strcmp(command, "/msg")) {
                param = strtok(NULL, " ");
                printf("Message sent to participant %s\n", param);
                if (param) {
                    int uid = atoi(param);
                    param = strtok(NULL, " ");
                    if (param) {
                        sprintf(bufout, "[%d]", cli->uid);
                        while (param != NULL) {
                            strcat(bufout, " ");
                            strcat(bufout, param);
                            param = strtok(NULL, " ");
                        }
                        strcat(bufout, "\r\n");
                        // printf("%d %s\n", cli->uid, bufout);
                        send_to(bufout, uid);
                    }
                    else {
                        // send_message_self("<< message cannot be null\r\n", cli->connfd);
                    }
                }
            }
        }
        else {
            sprintf(bufout, "[%d]: %s\r\n", cli->uid, bufin);
            printf("Message brodcasted by participant %s",bufout);
            send_all_but_self(bufout, cli->uid);
        }
    }
    /* Close connection */
    sprintf(bufout, "Participant %d has left\r\n", cli->uid);
    send_all(bufout);
    close(cli->connfd);

    /* Delete client from queue and join thread when leaves */
    del_client(cli->uid);
    printf("Participant %d quit \n", cli->uid);
    free(cli);
    client_count--;
    pthread_detach(pthread_self());
}

int
main(int argc, char** argv)
{
int sockfd, ret,n;
	 struct sockaddr_in serverAddr;

	int newSocket;
	struct sockaddr_in clientAddr;

	socklen_t addr_size;
	    pthread_t tid;
	char buffer[MAXLINE];
	pid_t childpid;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		printf("Error in connection.\n");
		exit(1);
	}

	memset(&serverAddr, '\0', sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	

	if(bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))<0)
	{
		printf("Error in binding.\n");
		exit(1);
	}

	listen(sockfd, MAX_CLIENTS) ;
	 printf("%s\n","Server running...waiting for connections.");
	 
	while(1){
		newSocket = accept(sockfd, (struct sockaddr*)&clientAddr, &addr_size);
		if(newSocket < 0)
		{
			exit(1);
		}

			  if ((client_count + 1) == MAX_CLIENTS) {
            printf("[-] max clients reached\n");
            printf("[-] reject ");
            printf("\n");
            close(newSocket);
            continue;
        }

        client_t* cli = (client_t*)malloc(sizeof(client_t));
        cli->addr = clientAddr;
        cli->connfd = newSocket;
        cli->uid = uid++;
        add_client(cli);
        pthread_create(&tid, NULL, &client_handler, (void*)cli);
        memset(&clientAddr, '\0', sizeof(clientAddr));
        

		}
	close(newSocket);
	 close (sockfd);

	return 0;

}










