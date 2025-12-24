#include "WinSocketServer.h"

int main() {
    WSADATA wsaData;
    WinServer server = { 0 };

    printf("--- WinSock TCP Server Starting ---\n");

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    InitializeCriticalSection(&server.Lock);

    if (InitAndBindServer(&server) != 0) {
        printf("Server init failed\n");
        DeleteCriticalSection(&server.Lock);
        WSACleanup();
        return 1;
    }

    ListenAndAcceptServer(&server);

    server.IsRunning = 0;
    closesocket(server.ListenSocket);
    DeleteCriticalSection(&server.Lock);
    WSACleanup();

    return 0;
}
