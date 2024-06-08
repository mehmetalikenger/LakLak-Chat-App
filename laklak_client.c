#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define BUFFER_SIZE 1024

// Function to receive messages from the server
void ReceiveMessages(SOCKET client_socket) {

    char buffer[BUFFER_SIZE];
    int recval; // Number of bytes received

    // Receive messages in a loop
    while ((recval = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {

        buffer[recval] = '\0';

        printf("%s", buffer);
    }
}

int main() {

    WSADATA wsa;
    SOCKET client_socket;
    struct sockaddr_in server_socket;
    char buffer[BUFFER_SIZE];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("\nWinsock initialization failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("\nSocket creation failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Server configurations
    server_socket.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_socket.sin_family = AF_INET;
    server_socket.sin_port = htons(8080);

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_socket, sizeof(server_socket)) < 0) {
        printf("\nConnection failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Create a thread to receive messages
    HANDLE receiver_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceiveMessages, (LPVOID)client_socket, 0, NULL);
    if (receiver_thread == NULL) {
        printf("\nMessage receiver thread creation failed. Error Code: %d\n", GetLastError());
        return 1;
    }

    while (1) {

        fgets(buffer, BUFFER_SIZE, stdin); // Get input from user

        send(client_socket, buffer, strlen(buffer), 0); // Send the message to the server
        
        if (strcmp(buffer, "EXIT\n") == 0) { // If the user types "EXIT", close the connection
            break;
        }
    }

    closesocket(client_socket); // Close the socket
    WSACleanup(); // Clean up Winsock
    return 0;
}
