#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <pthread.h>
#include "interface.h"
#include "common_types.h"

#define PORT_NUMBER 1022

using namespace std;

/*
 * TODO: IMPLEMENT BELOW THREE FUNCTIONS
 */
extern vector<room> room_db;

int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
//void process_chatmode(const char* host, const int port);
void process_chatmode(int sockfd);

int main(int argc, char** argv)
{
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}

    display_title();

	while (1) {

		int sockfd = connect_to(argv[1], atoi(argv[2]));

		char command[MAX_DATA];
        get_command(command, MAX_DATA);


		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);

		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0 && reply.status == SUCCESS) {
			printf("Now you are in the chatmode\n");
			process_chatmode(sockfd);
		}

		close(sockfd);
    }

    return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 *
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	//
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------

    // below is just dummy code for compilation, remove it.
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket failed.");
        exit(-1);
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (inet_pton(AF_INET, host, &addr.sin_addr) < 0){
        perror("Hostname error: ");
        exit(-1);
	}

	int c = connect(sockfd, (struct sockaddr*) &addr, sizeof(addr));
	if (c < 0){
        perror("Connection error 1: ");
	}
	return sockfd;
}

/*
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply
 */
struct Reply process_command(const int sockfd, char* command)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
    // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
    // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	//
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------
	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------
    char buffer[256];
    char recbuf[256];

    strcpy(buffer, command);
    int rc = send(sockfd, buffer, sizeof(buffer), 0);
    if (rc < 0){
        perror("Process command: send error.");
    }
    struct Reply reply;
    memset(recbuf, 0, 256);
    if (strncmp(buffer, "CREATE", 6) == 0){
        int recmessage = recv(sockfd, recbuf, sizeof(recbuf), 0);
        if (recmessage < 0){
            perror("Error with receiving message: ");
        }
        if (strncmp(recbuf, "FAILURE_ALREADY_EXISTS", 22) == 0){
            reply.status = FAILURE_ALREADY_EXISTS;
        } else if (strncmp(recbuf, "SUCCESS", 7) == 0){
            reply.status = SUCCESS;
        } else if (strncmp(recbuf, "FAILURE_UNKNOWN", 15) == 0){
            reply.status = FAILURE_UNKNOWN;
        }
    } else if (strncmp(buffer, "DELETE", 6) == 0){
        int recmessage = recv(sockfd, recbuf, sizeof(recbuf), 0);
        if (strncmp(recbuf, "FAILURE_NOT_EXISTS", 18) == 0){
            reply.status = FAILURE_NOT_EXISTS;
        } else if (strncmp(recbuf, "SUCCESS", 7) == 0){
            reply.status = SUCCESS;
        } else if (strncmp(recbuf, "FAILURE_INVALID", 15) == 0){
            reply.status = FAILURE_INVALID;
        }
    } else if (strncmp(buffer, "JOIN", 4) == 0){
        int recmessage = recv(sockfd, recbuf, sizeof(recbuf), 0);
        vector<char*> splitcommand;
        char* token = strtok(recbuf, " ");
        while (token != NULL){
            splitcommand.push_back(token);
            token = strtok(NULL, " ");
        }
        char* statusstring = splitcommand[0];
        char* nummemberstring = splitcommand[1];
        char* port = splitcommand[2];
        if (strncmp(statusstring, "FAILURE_NOT_EXISTS", 18) == 0){
            reply.status = FAILURE_NOT_EXISTS;
            reply.port = -1;
            reply.num_member = 0;
        } else if (strncmp(statusstring, "SUCCESS", 7) == 0){
            reply.status = SUCCESS;
            reply.port = atoi(port);
            reply.num_member = atoi(nummemberstring);
        }
    } else if (strncmp(buffer, "LIST", 4) == 0){
        int recmessage = recv(sockfd, recbuf, sizeof(recbuf), 0);
        if (strcmp(recbuf, "No list") == 0){
            reply.status = FAILURE_INVALID;
            char* listnotfound = "List not found.";
            strcpy(reply.list_room, listnotfound);
        } else {
            reply.status = SUCCESS;
            strcpy(reply.list_room, recbuf);
        }
    }
	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	//
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
    //
    // For the "LIST" command,
    // You are suppose to copy the list of chatroom to the list_room
    // variable. Each room name should be seperated by comma ','.
    // For example, if given command is "LIST", the Reply variable
    // will be set as following.
    //
    // Reply reply;
    // reply.status = SUCCESS;
    // strcpy(reply.list_room, list);
    //
    // "list" is a string that contains a list of chat rooms such
    // as "r1,r2,r3,"
	// ------------------------------------------------------------

	// REMOVE below code and write your own Reply.
	return reply;
}

/*
 * Get into the chat mode
 *
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(int sockfd)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------
    //int sockfd = connect_to(host, port);
    // ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------

    char sendbuffer[256];
    char recbuffer[256];

    for (;;){
        memset(sendbuffer, 0, 256);
        memset(recbuffer, 0, 256);
        printf("Enter a message:\n");
        get_message(sendbuffer, 256);
        int sendmsg = send(sockfd, sendbuffer, sizeof(sendbuffer), 0);
        int recmsg = recv(sockfd, recbuffer, sizeof(recbuffer), 0);
        display_message(recbuffer);
        printf("\n");
    }

    close(sockfd);
    printf("Exiting chat...\n");
    return;
    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    // 1. To get a message from a user, you should use a function
    // "void get_message(char*, int);" in the interface.h file
    //
    // 2. To print the messages from other members, you should use
    // the function "void display_message(char*)" in the interface.h
    //
    // 3. Once a user entered to one of chatrooms, there is no way
    //    to command mode where the user  enter other commands
    //    such as CREATE,DELETE,LIST.
    //    Don't have to worry about this situation, and you can
    //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------
}

