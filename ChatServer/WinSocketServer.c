#include "WinSocketServer.h"

/* Send all bytes reliably */
static int SendAll(SOCKET s, const char* data, int len) {
    int total = 0;
    while (total < len) {
        int sent = send(s, data + total, len - total, 0);
        if (sent <= 0) return -1;
        total += sent;
    }
    return total;
}

/* Route private messages */
static void RoutePrivateMessage(WinServer* server, SOCKET sender, const char* payload) {
    char local[DEFAULT_BUFLEN + 1];
    strcpy_s(local, sizeof(local), payload);

    char* target = local + 4;              // after "MSG|"
    char* content = strchr(target, '|');
    if (!content) return;

    *content++ = '\0';

    SOCKET targetSocket = INVALID_SOCKET;
    char senderName[50] = "Unknown";

    EnterCriticalSection(&server->Lock);

    for (int i = 0; i < server->ClientCount; i++) {
        if (strcmp(server->Clients[i].username, target) == 0)
            targetSocket = server->Clients[i].socket;

        if (server->Clients[i].socket == sender)
            strcpy_s(senderName, sizeof(senderName), server->Clients[i].username);
    }

    LeaveCriticalSection(&server->Lock);

    char msg[DEFAULT_BUFLEN];
    if (targetSocket != INVALID_SOCKET) {
        sprintf_s(msg, sizeof(msg), "[PM from %s]: %s", senderName, content);
    }
    else {
        strcpy_s(msg, sizeof(msg), "[System]: User not found.");
        targetSocket = sender;
    }

    Int32 len = (Int32)strlen(msg);
    Int32 netLen = htonl(len);

    SendAll(targetSocket, (char*)&netLen, 4);
    SendAll(targetSocket, msg, len);
}

/* Init server socket */
int InitAndBindServer(WinServer* server) {
    struct addrinfo hints = { 0 }, * result = NULL;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(IP, PORT, &hints, &result) != 0)
        return 1;

    server->ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (server->ListenSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        return 1;
    }

    if (bind(server->ListenSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(server->ListenSocket);
        return 1;
    }

    freeaddrinfo(result);
    return 0;
}

/* Accept loop */
int ListenAndAcceptServer(WinServer* server) {
    if (listen(server->ListenSocket, SOMAXCONN) == SOCKET_ERROR)
        return 1;

    server->IsRunning = 1;
    printf("[Server] Listening...\n");

    while (server->IsRunning) {
        SOCKET client = accept(server->ListenSocket, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        EnterCriticalSection(&server->Lock);

        if (server->ClientCount >= MAX_CLIENTS) {
            LeaveCriticalSection(&server->Lock);
            closesocket(client);
            continue;
        }

        server->Clients[server->ClientCount].socket = client;
        server->Clients[server->ClientCount].username[0] = '\0';
        server->ClientCount++;

        LeaveCriticalSection(&server->Lock);

        SessionData* session = malloc(sizeof(SessionData));
        session->ClientSocket = client;
        session->ServerRef = server;

        if (!_beginthreadex(NULL, 0, ClientSessionThread, session, 0, NULL)) {
            closesocket(client);
            free(session);
        }
    }
    return 0;
}

/* Client thread */
unsigned __stdcall ClientSessionThread(void* data) {
    SessionData* session = (SessionData*)data;
    SOCKET s = session->ClientSocket;
    char buffer[DEFAULT_BUFLEN + 1];

    while (session->ServerRef->IsRunning) {
        Int32 netSize = 0, recvd = 0;

        while (recvd < 4) {
            int r = recv(s, ((char*)&netSize) + recvd, 4 - recvd, 0);
            if (r <= 0) goto cleanup;
            recvd += r;
        }

        Int32 size = ntohl(netSize);
        if (size <= 0 || size > DEFAULT_BUFLEN) goto cleanup;

        recvd = 0;
        while (recvd < size) {
            int r = recv(s, buffer + recvd, size - recvd, 0);
            if (r <= 0) goto cleanup;
            recvd += r;
        }

        buffer[size] = '\0';

        if (strncmp(buffer, "JOIN|", 5) == 0) {
            EnterCriticalSection(&session->ServerRef->Lock);
            for (int i = 0; i < session->ServerRef->ClientCount; i++) {
                if (session->ServerRef->Clients[i].socket == s) {
                    strcpy_s(session->ServerRef->Clients[i].username,
                        sizeof(session->ServerRef->Clients[i].username),
                        buffer + 5);
                    break;
                }
            }
            LeaveCriticalSection(&session->ServerRef->Lock);
        }
        else if (strncmp(buffer, "MSG|", 4) == 0) {
            RoutePrivateMessage(session->ServerRef, s, buffer);
        }
    }

cleanup:
    EnterCriticalSection(&session->ServerRef->Lock);

    for (int i = 0; i < session->ServerRef->ClientCount; i++) {
        if (session->ServerRef->Clients[i].socket == s) {
            session->ServerRef->Clients[i] =
                session->ServerRef->Clients[session->ServerRef->ClientCount - 1];
            session->ServerRef->ClientCount--;
            break;
        }
    }

    LeaveCriticalSection(&session->ServerRef->Lock);

    closesocket(s);
    free(session);
    return 0;
}
