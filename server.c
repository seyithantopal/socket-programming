#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#define LENGTH_NAME 31
#define LENGTH_MSG 512
#define LENGTH_SEND 512
#define PORT 8080
#define BACKLOG 5


int counter = -1;

typedef struct ClientNode {
	int sockfd;
	struct ClientNode *prev;
	struct ClientNode *next;
	char ip[16];
	char name[LENGTH_NAME];
} ClientList;

ClientList *addNode(int sockfd, char *ip) {
	ClientList *temp = (ClientList *)malloc(sizeof(ClientList));
	temp->sockfd = sockfd;
	temp->prev = NULL;
	temp->next = NULL;
	strncpy(temp->ip, ip, 16);
	strncpy(temp->name, "NULL", 5);
	counter++;
	return temp;
}



// global variables
ClientList *head, *current;
int serverSockfd, clientSockfd;
char buff[128];

char *countPeople() {
	if(counter == 1 || counter == 0) {
		sprintf(buff, "There is %d client on the server\n", counter);
	} else {
		sprintf(buff, "There are %d clients on the server\n", counter);
	}
	return buff;
}

void str_overwrite_stdout() {
	printf("\r%s", "> ");
	fflush(stdout);
}

void printList(ClientList *node) {
	ClientList *temp = node;
	while(temp != NULL) {
		printf("Data: %d\n", temp->sockfd);
		temp = temp->next;
	}
}


void sendToAllClients(ClientList *node, char buffer[]) {
	ClientList *clients = head->next;
	while(clients != NULL) {
		printf("Send to sockfd %d: \"%s\" \n", clients->sockfd, buffer);
		send(clients->sockfd, buffer, LENGTH_SEND, 0);
		clients = clients->next;
	}
}


void exitClients(ClientList *node) {
	ClientList *temp;
	if(node == current) {
		node->prev->next = NULL;
		current = node->prev;
	} else {
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}
	counter--;
	printf("%s\n", countPeople());
	free(node);
}

void clientHandler(void *p_client) {
	int flag = 0;
	char nickname[LENGTH_NAME] = {};
	char recvBuffer[LENGTH_MSG] = {};
	char sendBuffer[LENGTH_SEND] = {};
	char buffer[LENGTH_SEND] = {};

	int receive = -1;
	char whoChar[20] = {};
	int who = 0;

	ClientList *temp = (ClientList *)p_client;

	// naming
	if(recv(temp->sockfd, nickname, LENGTH_NAME, 0) <= 0) {
		printf("%s did not input the name\n", temp->ip);
		flag = 1;
	} else {
		strncpy(temp->name, nickname, LENGTH_NAME);
		printf("%s\n", countPeople());
		printf("%s(%s)(%d) joined the communication\n", temp->name, temp->ip, temp->sockfd);
		sprintf(sendBuffer, "%s(%d) joined the communication", temp->name, temp->sockfd);
		sendToAllClients(temp, sendBuffer);
	}

	// conversation
	while(1) {
		if(flag == 1) break;
		receive = recv(temp->sockfd, recvBuffer, LENGTH_MSG, 0);
		if(receive > 0) {
			// split process
			if(strlen(recvBuffer) == 0) continue;
			char *tmp = recvBuffer;
			char **result = NULL;
			char *tokenPtr = strtok(recvBuffer, "-");

			int spaces = 0, i = 0;

			while(tokenPtr != NULL) {
				spaces++;
				result = realloc(result, spaces * sizeof(char *));
				result[spaces - 1] = tokenPtr;
				tokenPtr = strtok(NULL, "-");
			}
			if(spaces == 2) {
				strcpy(whoChar, result[0]);
				who = atoi(whoChar);
				sprintf(buffer, "%d-%s", 1, result[1]);
				send(who, buffer, LENGTH_SEND, 0);
				
			} 
			// split process end
			else { // send messages to all clients
				if(strcmp(recvBuffer, "server") == 0) {
					char message[LENGTH_SEND] = {};
					char serverBuffer[LENGTH_SEND] = {};
					str_overwrite_stdout();
					fgets(message, LENGTH_MSG, stdin);
					sprintf(serverBuffer, "Server says that %s", message);
					sendToAllClients(temp, serverBuffer);
				} else {
					if(strcmp(recvBuffer, "exit") == 0) {
						printf("%s(%s)(%d) left the communication\n", temp->name, temp->ip, temp->sockfd);
						sprintf(sendBuffer, "%s(%s) left the communication", temp->name, temp->ip);
						flag = 1;
					}	
					else {
						sprintf(sendBuffer, "<%s>: %s", temp->name, recvBuffer);
					}
					sendToAllClients(temp, sendBuffer);
				}
			}
		}
	}

	// remove node when one of them wants to exit
	close(temp->sockfd);	
	exitClients(temp);
}


int main() {
	// create a socket
	serverSockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSockfd < 0) {
		printf("ERROR: Failed to create a socket\n");
		exit(EXIT_FAILURE);
	}

	// socket information
	struct sockaddr_in serverAddr, clientAddr;
	int s_addrlen = sizeof(serverAddr);
	int c_addrlen = sizeof(clientAddr);

	memset(&serverAddr, 0, s_addrlen);
	memset(&clientAddr, 0, c_addrlen);

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(PORT);

	// bind and listen
	int bn = bind(serverSockfd, (struct sockaddr *)&serverAddr, s_addrlen);
	if(bn <0) {
		printf("ERROR: Failed to bind\n");
	}
	int ls = listen(serverSockfd, BACKLOG);
	if(ls <0) {
		printf("ERROR: Failed to listen\n");
	}

	printf("Server has been started on %s:%d\n", inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));

	head = addNode(serverSockfd, inet_ntoa(serverAddr.sin_addr));
	current = head;

	while(1) {
		clientSockfd = accept(serverSockfd, (struct sockaddr *)&clientAddr, &c_addrlen);
		printf("Client %s:%d has come in\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

		ClientList *c = NULL;
		c = addNode(clientSockfd, inet_ntoa(clientAddr.sin_addr));
		c->prev = current;
		current->next = c;
		current = c;

		pthread_t id;
		if(pthread_create(&id, NULL, (void *)clientHandler, (void *)c) != 0) {
			perror("ERROR: Failed to create a pthread\n");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}