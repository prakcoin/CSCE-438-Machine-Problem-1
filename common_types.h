#ifndef COMMON_TYPES_H_INCLUDED
#define COMMON_TYPES_H_INCLUDED

#include <vector>

using namespace std;

typedef struct {
    char room_name[256];
    int m_sock;
    int port_num;
    int num_members;
    vector<int> slave_socket;
} room;

struct threadargs{
    int socket;
    int port;
    struct sockaddr_in address;
};

#endif // COMMON_TYPES_H_INCLUDED
