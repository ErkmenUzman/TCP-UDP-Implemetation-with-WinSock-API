#ifndef WINSOCKETCLIENT_H
#define WINSOCKETCLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT "9000"
#define IP "127.0.0.1"
#define DEFAULT_BUFLEN 512

typedef char Int8;
typedef short int Int16;
typedef int Int32;

typedef unsigned char UInt8;
typedef unsigned short int UInt16;
typedef unsigned int UInt32;

Int32 _iResult;

// Define a structure to hold our connection data
typedef struct {
    SOCKET Connectsocket;
    struct sockaddr_in serverAddr;
    Int8 IsRunning;
} WinClient;

// Function Prototypes
int InitAndConnectSocket(WinClient* client);
void ConnectServer(WinClient* client, struct addrinfo* result);
int SendPacket(WinClient* client, const char* data);
unsigned __stdcall ReceiveThread(void* data);
int ReceivePacket(WinClient* client, char* outBuffer, int maxLen);
void CleanupSocket(WinClient* client);

#endif
