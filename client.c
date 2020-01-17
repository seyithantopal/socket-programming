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

void str_trim_lf(char *arr, int length) {
	int i;
	for(i = 0; i < length; i++) {
		if(arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

void str_overwrite_stdout() {
	printf("\r%s", "> ");
	fflush(stdout);
}

//global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char nickname[LENGTH_NAME] = {};

void catch_ctrl_c_and_exit(int sig) {
	flag = 1;
}

int isFile = 0;
void recv_msg_handler() {
	char receiveMessage[LENGTH_SEND] = {};

	char fileName[512] = {};
	char whoChar[20] = {};
	int who = 0;
	int receive = 0;

	FILE *fptr;

	memset(receiveMessage, 0, LENGTH_SEND);
	int numWrite = 0, write_sz, bytesReceived = 0;
	while(1) {
		while((receive = recv(sockfd, receiveMessage, LENGTH_SEND, 0)) > 0) {
			char *tmp = receiveMessage;

			int size = strlen(receiveMessage);
			

			char **result = NULL;
			char *tokenPtr = strtok(receiveMessage, "-");

			int spaces = 0, i = 0;

			while(tokenPtr != NULL) {
				spaces++;
				result = realloc(result, spaces * sizeof(char *));
				result[spaces - 1] = tokenPtr;
				tokenPtr = strtok(NULL, "-");
			}
			if(spaces == 2) {

				if((fptr = fopen("hello.txt", "ab")) == NULL) printf("File could not be opened");
				write_sz = fwrite(receiveMessage, sizeof(char), size, fptr);
				memset(receiveMessage, 0, LENGTH_SEND);
				fclose(fptr);
			}
			else {
				printf("\r%s\n", tmp);
				str_overwrite_stdout();

			}
		}
	}
}

void send_msg_handler() {
	char message[LENGTH_MSG] = {};
	char buffer[LENGTH_MSG] = {};
	char buffer2[LENGTH_MSG] = {};

	char fileName[512] = {};
	char whoChar[20] = {};
	int who = 0;

	FILE *fptr;
	int numRead = 0;
	int numWrite = 0, write_sz, bytesReceived = 0;

	while(1) {
		str_overwrite_stdout();
		while(fgets(message, LENGTH_MSG, stdin) != NULL) {
			str_trim_lf(message, LENGTH_MSG);
			if(strlen(message) == 0) str_overwrite_stdout();
			else break;
		}

		// control
		char **result = NULL;
		char *tokenPtr = strtok(message, "-");

		int spaces = 0, i = 0;

		while(tokenPtr != NULL) {
			spaces++;
			result = realloc(result, spaces * sizeof(char *));
			result[spaces - 1] = tokenPtr;
			tokenPtr = strtok(NULL, "-");
		}
		if(strcmp(result[0], "file") == 0) {
			isFile = 1;
			strcpy(whoChar, result[1]);
			strcpy(fileName, result[2]);
			who = atoi(whoChar);


			// send file
			
			if((fptr = fopen(fileName, "rb")) == NULL) {
				printf("File could not be opened or be written\n");
				continue;
			}
			else {
				memset(buffer, 0, LENGTH_MSG);
				memset(message, 0, LENGTH_MSG);

				while((numRead = fread(buffer, sizeof(char), LENGTH_MSG, fptr))>0) {
					sprintf(buffer2, "%d-%s", who, buffer);
					send(sockfd, buffer2, numRead, 0);
					memset(buffer, 0, LENGTH_MSG);
				}
			}
			fclose(fptr);

		} else {
			send(sockfd, message, LENGTH_MSG, 0);
		}

		// control end
		
		if(strcmp(message, "exit") == 0) break;
	}
	catch_ctrl_c_and_exit(2);
}

int main() {

	signal(SIGINT, catch_ctrl_c_and_exit);

	// naming
	printf("Please enter your name: ");
	if(fgets(nickname, LENGTH_NAME, stdin) != NULL) str_trim_lf(nickname, LENGTH_NAME);

	// create a socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		printf("Fail to create a socket\n");
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

	// connect to server
	int err = connect(sockfd, (struct sockaddr *)&serverAddr, s_addrlen);
	if(err < 0) {
		printf("Failed to connect to the server\n");
		exit(EXIT_FAILURE);
	}

	// names
	printf("Connect to server: %s:%d\n", inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));
	printf("You are: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
	
	send(sockfd, nickname, LENGTH_NAME, 0);

	pthread_t send_msg_thread;	
	if(pthread_create(&send_msg_thread, NULL, (void *)send_msg_handler, NULL) != 0) {
		printf("Failed to create pthread\n");
		exit(EXIT_FAILURE);
	}

	pthread_t recv_msg_thread;	
	if(pthread_create(&recv_msg_thread, NULL, (void *)recv_msg_handler, NULL) != 0) {
		printf("Failed to create pthread\n");
		exit(EXIT_FAILURE);
	}

	while(1) {
		if(flag) {
			printf("\nBye\n");
			break;
		}
	}
	close(sockfd);
	return 0;
}