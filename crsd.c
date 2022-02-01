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
#include <iostream>
#include <pthread.h>
#include "interface.h"
#include "common_types.h"

#define BUFFER_LENGTH 256
#define PORT_NUMBER 1025
#define FALSE 0
#define MAX_ROOM 20
#define MAX_MEMBER 5

using namespace std;

vector<room> room_db;
vector<int> ports; //port numbers for each chatroom

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void send_to_all(char* s, char* room_name, int socket){
    pthread_mutex_lock(&mutex);
    char messagebuf[256];
    memset(messagebuf, 0, 256);
    strcpy(messagebuf, s);
    for (int i = 0; i < room_db.size(); i++){
        if (strcmp(room_db[i].room_name, room_name) == 0){
            for (int j = 0; j < room_db[i].slave_socket.size(); j++){
                if (room_db[i].slave_socket[j] != socket){
                    //send to every slave socket in the room
                    send(room_db[i].slave_socket[j], messagebuf, strlen(messagebuf), 0);
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

int handle_create(char* buffer, int s_sock){
    string msg = "";
    pthread_mutex_lock(&mutex);
    if (room_db.size() >= MAX_ROOM){ //room exceed capacity
        char* sentunknown = "FAILURE_UNKNOWN";
        int sentmsg = send(s_sock, sentunknown, strlen(sentunknown), 0);
        return 0;
    }

    vector<char*> splitcommand;
    char* token = strtok(buffer, " ");
    while (token != NULL){
        splitcommand.push_back(token);
        token = strtok(NULL, " ");
    }
    char* name = splitcommand[1];
    for (int i = 0; i < room_db.size(); i++){
        if (strcmp(name, room_db[i].room_name) == 0){ //if the room already exists
            char* sentalreadyexists = "FAILURE_ALREADY_EXISTS";
            int sentmsg = send(s_sock, sentalreadyexists, strlen(sentalreadyexists), 0);
            if (sentmsg < 0){
                perror("Send failed");
            }
            pthread_mutex_unlock(&mutex);
            return 0;
        }
    }
    int port = ports[0]; //able to make a new room
    ports.erase(ports.begin());

    int mastersock = socket(AF_INET, SOCK_STREAM, 0);
    room newRoom;
    newRoom.num_members = 0;
    newRoom.port_num = port;
    strcpy(newRoom.room_name, name);
    newRoom.m_sock = mastersock;
    room_db.push_back(newRoom);

    char* sentsuccess = "SUCCESS";
    int sentmsg = send(s_sock, sentsuccess, strlen(sentsuccess), 0);
    printf("Chatroom created\n");

    pthread_mutex_unlock(&mutex);
    return 1;
}


int handle_delete(char* buffer, int s_sock){
    string msg = "";
    pthread_mutex_lock(&mutex);
    if (room_db.size() == 0){ //nothing to delete
        char* sentinvalid = "FAILURE_INVALID";
        printf("Failed to delete chatroom.\n");
        int sentmsg = send(s_sock, sentinvalid, strlen(sentinvalid), 0);
        return 0;
    }

    vector<char*> splitcommand;
    char* token = strtok(buffer, " ");
    while (token != NULL){
        splitcommand.push_back(token);
        token = strtok(NULL, " ");
    }
    char* name = splitcommand[1];
    for (int i = 0; i < room_db.size(); i++){
        if (strcmp(name, room_db[i].room_name) == 0){
            for (int j = 0; j < room_db[i].slave_socket.size(); j++){ //found the room, now closing every slave socket
                char* chatroomdeletemsg = "Chatroom being closed. Shutting down connection...";
                send(room_db[i].slave_socket[j], chatroomdeletemsg, strlen(chatroomdeletemsg), 0);
                close(room_db[i].slave_socket[j]);
            }
            int port = room_db[i].port_num;
            ports.push_back(port);
            room_db.erase(room_db.begin() + i); //deleting element from vector
            char* sentsuccess = "SUCCESS";
            int sentmsg = send(s_sock, sentsuccess, strlen(sentsuccess), 0);
            printf("Chatroom deleted successfully\n");
            pthread_mutex_unlock(&mutex);
            return 1;
        }
    }
    char* sentnotexists = "FAILURE_NOT_EXISTS"; //couldnt find the chatroom
    int sentmsg = send(s_sock, sentnotexists, strlen(sentnotexists), 0);
    printf("Failed to delete chatroom\n");
    pthread_mutex_unlock(&mutex);
    return 0;
}

int handle_join(char* buffer, int s_sock){
    vector<char*> splitcommand;
    char* token = strtok(buffer, " ");
    while (token != NULL){
        splitcommand.push_back(token);
        token = strtok(NULL, " ");
    }
    char* name = splitcommand[1];
    string msg = "";
    for (int i = 0; i < room_db.size(); i++){
        if (strcmp(name, room_db[i].room_name) == 0){
            msg = "SUCCESS";
            msg += " ";
            room_db[i].num_members++;
            msg += to_string(room_db[i].num_members);
            msg += " ";
            msg += to_string(room_db[i].port_num);

            room_db[i].slave_socket.push_back(s_sock);

            int sentmsg = send(s_sock, msg.c_str(), strlen(msg.c_str()), 0);
            return 0;
        }
    }
    char* failuremsg = "FAILURE_NOT_EXISTS";
    int sentmsg = send(s_sock, failuremsg, strlen(failuremsg), 0);
    return 1;
}

int handle_list(char* buffer, int s_sock){
    string list = "";
    if (room_db.size() > 0){
        for (int i = 0; i < room_db.size(); i++){
            list += room_db[i].room_name;
            if (i != room_db.size() - 1){
                list += ",";
            }
        }
        int sentmsg = send(s_sock, list.c_str(), strlen(list.c_str()), 0);
        return 1;
    }
    char* nolist = "No list";
    int sentmsg = send(s_sock, nolist, strlen(nolist), 0);
    return 0;
}

bool parse_command(char* buffer, int s_sock){
    if (strncmp(buffer, "CREATE", 5) == 0){
        handle_create(buffer, s_sock);
        return true;
    } else if (strncmp(buffer, "DELETE", 6) == 0){
        handle_delete(buffer, s_sock);
        return true;
    } else if (strncmp(buffer, "JOIN", 4) == 0){
        return handle_join(buffer, s_sock);
    } else if (strncmp(buffer, "LIST", 4) == 0){
        handle_list(buffer, s_sock);
        return true;
    }
    return true;
}

int passiveTCPsock(int port, int backlog) {
    struct sockaddr_in sin;          /* Internet endpoint address */
    memset(&sin, 0, sizeof(sin));    /* Zero out address */
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0){
        perror("Failed to create socket");
    }
    /* Bind the socket */
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
       perror("Failed to bind socket");

    /* Listen on socket */
    if (listen(s, backlog) < 0)
       perror("Can't listen");

    return s;
}

void *connection_handler(void *threadinfo){
    //Get the socket descriptor
    threadargs* t = (threadargs*) threadinfo;
    int sock = t->socket;
    int port = t->port;
    int read_size = 0;
    char buffer[BUFFER_LENGTH];
    char command_buffer[BUFFER_LENGTH];
    int rc = 0;

    while(1){
        rc = recv(sock, buffer, sizeof(buffer), 0);
        if (rc < 0){
            perror("Receive failed ");
        }
        if (!rc){
            continue;
        }
        memcpy(command_buffer, buffer, sizeof(buffer));
        if (!parse_command(buffer, sock)){
            break;
        }
    }

    vector<char*> splitcommand;
    char* token = strtok(command_buffer, " ");
    while (token != NULL){
        splitcommand.push_back(token);
        token = strtok(NULL, " ");
    }
    char* roomname = splitcommand[1];

    memset(buffer, 0, BUFFER_LENGTH);
    while(1){
        read_size = recv(sock, buffer, BUFFER_LENGTH, 0);
        if (read_size < 0){
            perror("Receive failed.");
        }
        if (!read_size){
            continue;
        }
        send_to_all(buffer, roomname, sock);
		memset(buffer, 0, BUFFER_LENGTH);
    }
    return 0;
}


int main(int argc, char** argv){
    int m_sock, s_sock;      /* master and slave socket     */
    struct sockaddr_in peer_addr;
    int peeraddrsize = sizeof(struct sockaddr_in);
    int rc = 1;
    int port = atoi(argv[1]);

    m_sock = passiveTCPsock(port, 32);
    char buffer[BUFFER_LENGTH];
    for (int i = port; i <= port + MAX_ROOM; i++){
        ports.push_back(i);
    }
    printf("Ready for connections!\n");

    while (true) {

        s_sock = accept(m_sock, (struct sockaddr*)&peer_addr, (socklen_t*) &peeraddrsize);
        if (s_sock < 0){
            perror("Socket not accepted");
        }

        pthread_t handler_thread;

        threadargs *tinfo = (threadargs*)malloc(sizeof(threadargs));
        memcpy(&tinfo->address, &peer_addr, sizeof(struct sockaddr_in));
        tinfo->socket = s_sock;
        tinfo->port = port;
        pthread_create(&handler_thread, NULL, connection_handler, (void*) tinfo);
    }
}
