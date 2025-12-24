#include "WinSocketClient.h"
#include <process.h>

void ConnectServer(WinClient* client, struct addrinfo* result) {
	struct addrinfo* ptr = NULL;

	// Loop through all addresses returned by getaddrinfo until one works
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create the socket inside the loop using current ptr info
		client->Connectsocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (client->Connectsocket == INVALID_SOCKET) {
			continue;
		}

		// Try to connect
		_iResult = connect(client->Connectsocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (_iResult == SOCKET_ERROR) {
			closesocket(client->Connectsocket);
			client->Connectsocket = INVALID_SOCKET;
			continue;
		}
		break; // If we reach here, we are connected!
	}

	freeaddrinfo(result);
}

int InitAndConnectSocket(WinClient* client) {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

	struct addrinfo* result = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(IP, PORT, &hints, &result) != 0) {
		WSACleanup();
		return 1;
	}

	ConnectServer(client, result);

	if (client->Connectsocket == INVALID_SOCKET) {
		WSACleanup();
		return 1;
	}

	return 0;
}

int SendPacket(WinClient* client, const char* data) {
	if (!client || client->Connectsocket == INVALID_SOCKET) return -1;

	int dataLen = (int)strlen(data);

	// This ensures C# and C agree on the number regardless of CPU type
	int networkLen = htonl(dataLen);

	// Send the Header (4 bytes)
	if (send(client->Connectsocket, (char*)&networkLen, sizeof(int), 0) == SOCKET_ERROR) {
		return -1;
	}

	// Send the Payload (The actual message)
	int totalSent = 0;
	while (totalSent < dataLen) {
		int result = send(client->Connectsocket, data + totalSent, dataLen - totalSent, 0);

		if (result > 0) {
			totalSent += result;
		}
		else if (result == SOCKET_ERROR) {
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				Sleep(1);
				continue;
			}
			return -1; // Fatal error should be checked in higher level code
		}
	}

	return totalSent;
}

unsigned __stdcall ReceiveThread(void* data) {
	WinClient* client = (WinClient*)data;
	char recvBuffer[DEFAULT_BUFLEN];

	while (client->IsRunning) {
		int bytesReceived = ReceivePacket(client, recvBuffer, sizeof(recvBuffer));

		if (bytesReceived > 0) {
			// 1. Ensure null termination
			recvBuffer[bytesReceived] = '\0';

			// 2. PROTOCOL PARSING
			if (strncmp(recvBuffer, "USERLIST|", 9) == 0) {
				// Handle User List update
				printf("\r[System] Online Users: %s\nUser: ", recvBuffer + 9);
			}
			else if (strstr(recvBuffer, "|") != NULL) {
				// If it contains a pipe, it's likely a formatted message from the server
				// Example: "Admin: Hello World" (sent by C# server)
				printf("\r%s\nUser: ", recvBuffer);
			}
			else {
				// Fallback for plain text
				printf("\r[Server]: %s\nUser: ", recvBuffer);
			}

			fflush(stdout);
		}
		else if (bytesReceived == 0) {
			printf("\r[System]: Server closed connection.\n");
			client->IsRunning = 0;
			break;
		}
		else {
			printf("\r[System]: Receive error. Disconnecting...\n");
			client->IsRunning = 0;
			break;
		}
	}
	return 0;
}

int ReceivePacket(WinClient* client, char* outBuffer, int maxLen) {
	if (!client || client->Connectsocket == INVALID_SOCKET) return -1;

	int payloadSize = 0;

	int headerReceived = 0;
	while (headerReceived < sizeof(int)) {
		int result = recv(client->Connectsocket, ((char*)&payloadSize) + headerReceived, sizeof(int) - headerReceived, 0);
		if (result <= 0) return -1; // Connection error should be checked in a higher level code
		headerReceived += result;
	}

	payloadSize = ntohl(payloadSize);

	if (payloadSize >= maxLen || payloadSize < 0) {
		printf("\r[Error] Protocol Desync: Invalid size %d\n", payloadSize);
		return -1;
	}

	int totalReceived = 0;
	while (totalReceived < payloadSize) {
		int result = recv(client->Connectsocket, outBuffer + totalReceived, payloadSize - totalReceived, 0);
		if (result <= 0) return -1;
		totalReceived += result;
	}

	outBuffer[totalReceived] = '\0'; // Null-terminate for printing
	return totalReceived;
}

void CleanupSocket(WinClient* client) {
	// shutdown the connection for sending since no more data will be sent
	// the client can still use the ConnectSocket for receiving data
	_iResult = shutdown(client->Connectsocket, SD_SEND);
	if (_iResult == SOCKET_ERROR) {
		printf("shutdown failed: %d\n", WSAGetLastError());
		closesocket(client->Connectsocket);
		WSACleanup();
		return 1;
	}
}

void DisconnectSocket(WinClient* client) {
	// cleanup
	closesocket(client->Connectsocket);
	WSACleanup();
}