#ifndef WINSOCKETSERVER_H
#define WINSOCKETSERVER_H

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdint.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT "9000"
#define IP "127.0.0.1"
#define DEFAULT_BUFLEN 512
#define MAX_CLIENTS 100

typedef int8_t   Int8;
typedef int16_t  Int16;
typedef int32_t  Int32;
typedef uint8_t  UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;

/* Client info */
typedef struct {
    SOCKET socket;
    char username[50];
} ClientContext;

/* Server info */
typedef struct {
    SOCKET ListenSocket;
    Int8 IsRunning;
    ClientContext Clients[MAX_CLIENTS];
    Int32 ClientCount;
    CRITICAL_SECTION Lock;
} WinServer;

/* Per-session thread data */
typedef struct {
    SOCKET ClientSocket;
    WinServer* ServerRef;
} SessionData;

/* API */
int InitAndBindServer(WinServer* server);
int ListenAndAcceptServer(WinServer* server);
unsigned __stdcall ClientSessionThread(void* data);

#endif
