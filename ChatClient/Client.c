#include "WinSocketClient.h"

int main() {
    WinClient client = { 0 };                           // Initialize struct members to 0/NULL
    char username[50] = { 0 };                          // Clear username buffer
    char message[DEFAULT_BUFLEN] = { 0 };               // Clear message buffer
    char protocolBuffer[DEFAULT_BUFLEN + 64] = { 0 };   // Clear protocol buffer
    int status;

    printf("--- WinSock C Client Starting ---\n");
    
    // 1- Initialize socket and connect to the server
    status = InitAndConnectSocket(&client);
    if (status != 0) {
        printf("Failed to connect. Make sure the C# Server is running!\n");
        return 1;
    }

    // 2- Get username from the user
    printf("Enter username: ");
    if (scanf_s("%49s", username, (unsigned)_countof(username)) != 1) return 1;
    getchar();

    // 3- Adjust the username to be a part of the message protocol used throughout this project
    sprintf_s(protocolBuffer, sizeof(protocolBuffer), "JOIN|%s", username);
    if (SendPacket(&client, protocolBuffer) < 0) {
        printf("Failed to send JOIN request.\n");
        CleanupSocket(&client);
        return 1;
    }

    // 4- Begin async receiving using background threads
    client.IsRunning = 1;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, ReceiveThread, &client, 0, NULL);

    if (hThread == NULL) {
        printf("Failed to create receiver thread.\n");
        CleanupSocket(&client);
        return 1;
    }
    printf("[Debug] Main Thread ID: %lu\n", GetCurrentThreadId());
    CloseHandle(hThread);

    // 5- Main console chat loop
    printf("\n--- Chat Active (Type 'exit' to quit) ---\n");
    printf("\rTo send: Target|Message (e.g., Alice|Hello)\nUser: ");
    fflush(stdout);

    while (client.IsRunning) {
        if (fgets(message, DEFAULT_BUFLEN, stdin) == NULL) break;
        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "exit") == 0) {
            client.IsRunning = 0;
            break;
        }

        if (strlen(message) > 0) {
            // Check if the user included the '|' delimiter
            char* separator = strchr(message, '|');

            // Adjusting the message to satisfy the needs of the protocol
            if (separator != NULL) {
                sprintf_s(protocolBuffer, sizeof(protocolBuffer), "MSG|%s", message);

                if (SendPacket(&client, protocolBuffer) < 0) {
                    printf("\n[System]: Failed to send message.\n");
                    break;
                }
            }
            else {
                printf("[System] Invalid format! Use: TargetName|Your Message\nUser: ");
                fflush(stdout);
            }
        }
    }

    printf("\nShutting down...\n");
    CleanupSocket(&client);
    DisconnectSocket(&client);

    return 0;
}