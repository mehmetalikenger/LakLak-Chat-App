#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>


#define PORT 8080
#define MAX_LISTEN_QUEUE 100
#define MAX_ACTIVE_CLIENTS 50
#define BUFFER_SIZE 1024
#define USER_FILE "users.txt"


typedef struct {
    char userName[50];
    char password[50];
    char name[50];
    char surname[50];
    char mood[50];
    int online;
    SOCKET socket;
} User;


User *users; // Array to store users
int activeClients = 0; // Number of users currently connected
int userCount = 0; // If there is no users file, userCount will be 0. If there is, it will be the number of users in the file
CRITICAL_SECTION cs;


void LoadUsers();
void SaveUsers();
DWORD WINAPI HandleClient(LPVOID clientSocket); 


// Get number of users from the file
int GetUserCount(FILE *file) {

    int count = 0;
    char line[255];

    while (fgets(line, sizeof(line), file)) {
        count++;
    }

    rewind(file);
    
    return count;
}


// Function to load users from the file
void LoadUsers() {

    FILE *file = fopen(USER_FILE, "r");

    if (file == NULL) {
        printf("\nERROR: User file not found.\n");
        return;
    }

    userCount = GetUserCount(file);
    users = (User*)malloc(userCount  * sizeof(User));
    if(users == NULL){
        printf("\nERROR: Memory allocation failed.\n");
        return;
    }

    int i = 0;

    // Read users from the file
    while (fscanf(file, "%[^,],%[^,],%[^,],%[^,],%[^,],%d\n", users[i].userName, users[i].password,             
                  users[i].name, users[i].surname, users[i].mood, &users[i].online) != EOF) {

        users[i].socket = INVALID_SOCKET;
        i++;
    }

    fclose(file);
}


// Function to save users to the file
void SaveUsers() { 

    FILE *file = fopen(USER_FILE, "w");
    if (file == NULL) {
        printf("\nERROR: User file not found.\n");
        return;
    }

    for (int i = 0; i < userCount; i++) {
        fprintf(file, "%s,%s,%s,%s,%s,%d\n", users[i].userName, users[i].password, users[i].name, users[i].surname,
                users[i].mood, users[i].online);
    }

    fclose(file);
}



void ShowMenu(SOCKET sock, int loggedIn) {
    if (loggedIn) {
        send(sock, "\nCommands\n-----------\nList: LIST *\nInfo: INFO <username>\nMessage: MSG <username> <message> or MSG * <message> (to send everyone)\n\n", strlen("\nCommands\n-----------\nList: LIST *\nInfo: INFO <username>\nMessage: MSG <username> <message> or MSG * <message> (to send everyone)\n\n"), 0);
    } else {
        send(sock, "\n** Welcome to LakLak **\n\nCommands\n-----------\nRegister: REGISTER <username> <password> <name> <surname>\nLogin: LOGIN <username> <password> <mood(optional)>\nEXIT: EXIT\n\n", strlen("\n** Welcome to LakLak **\n\nCommands\n-----------\nRegister: REGISTER <username> <password> <name> <surname>\nLogin: LOGIN <username> <password> <mood(optional)>\nEXIT: EXIT\n\n"), 0);
    }
}


// Function to handle client requests (commands)
DWORD WINAPI HandleClient(LPVOID clientSocket) {

    activeClients++;

    SOCKET client_socket = *(SOCKET*)clientSocket;
    char buffer[BUFFER_SIZE];
    int recval; // Number of bytes received
    char currentUser[50] = "";
    int loggedIn = 0; 

    ShowMenu(client_socket, loggedIn);

    while ((recval = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) { // Recieve command and parameters from the client

        buffer[recval] = '\0'; // Null terminate the buffer
        char command[50], param1[50], param2[50], param3[50], param4[50] = ""; // Command and parameters

        //param1: username or *
        //param2: password or message
        //param3: name or mood
        //param4: surname

        sscanf(buffer, "%s %s %s %s %[^\n]", command, param1, param2, param3, param4); // Parse the command and parameters

        if (strcmp(command, "REGISTER") == 0) {

            if(strcmp(param1, "") == 0 || strcmp(param2, "") == 0 || strcmp(param3, "") == 0 || strcmp(param4, "") == 0){
                send(client_socket, "\nERROR: Invalid parameters.\n", strlen("\nERROR: Invalid parameters.\n"), 0);
                continue;
            }

            if (loggedIn) { 
                send(client_socket, "\nERROR: Already logged in. Cannot register a new account.\n", strlen("\nERROR: Already logged in. Cannot register a new account.\n"), 0);
                continue;
            }

            EnterCriticalSection(&cs);

            int exists = 0; // Check if the username already exists

            for (int i = 0; i < userCount; i++) {

                if (strcmp(users[i].userName, param1) == 0) {
                    exists = 1; // Username already exists
                    break;
                }
            }

            if (exists) {

                send(client_socket, "\nERROR: Username already taken.\n", strlen("\nERROR: Username already taken.\n"), 0);

            } else { // Register the new user

                users = (User*)realloc(users, (userCount + 1) * sizeof(User)); // Allocate memory for the new user because if the LoadUsers() reallocated the memory, it means there isn't enough memory for the new user. 
                if(users == NULL){                                             // Because LoadUsers() reallocates the memory to the number of users in the file.
                    printf("\nERROR: Memory allocation failed.\n"); 
                    return 1;
                }
                
                strcpy(users[userCount].userName, param1);
                strcpy(users[userCount].password, param2);
                strcpy(users[userCount].name, param3);
                strcpy(users[userCount].surname, param4);
                strcpy(users[userCount].mood, "N/A");
                users[userCount].online = 0;
                users[userCount].socket = INVALID_SOCKET;
                userCount++;
                SaveUsers();
                send(client_socket, "\nSUCCESS: Registration successful.\n", strlen("\nSUCCESS: Registration successful.\n"), 0);

                // Clear the parameters
                param1[0] = '\0';
                param2[0] = '\0';
                param3[0] = '\0';
                param4[0] = '\0';
            }

            LeaveCriticalSection(&cs);

        } else if (strcmp(command, "LOGIN") == 0) {

            if(strcmp(param1, "") == 0 || strcmp(param2, "") == 0){
                send(client_socket, "\nERROR: Invalid parameters.\n", strlen("\nERROR: Invalid parameters.\n"), 0);
                continue;
            }

            if (loggedIn) {

                send(client_socket, "\nERROR: Already logged in. Cannot login again.\n", strlen("\nERROR: Already logged in. Cannot login again.\n"), 0);
                continue;
            }

            EnterCriticalSection(&cs);

            int found = 0; // Represents if the user is found

            for (int i = 0; i < userCount; i++) {

                if (strcmp(users[i].userName, param1) == 0 && strcmp(users[i].password, param2) == 0) { // Check if the username and password match

                    users[i].online = 1; // Set the user as online
                    users[i].socket = client_socket; // Set the user's socket

                    if (param3[0] != '\0') {
                        strcpy(users[i].mood, param3);
                    }

                    SaveUsers();
                    found = 1;
                    loggedIn = 1;
                    strcpy(currentUser, param1);
                    send(client_socket, "\nSUCCESS: Login successful.\n", strlen("\nSUCCESS: Login successful.\n"), 0);

                    // Clear the parameters
                    param1[0] = '\0';
                    param2[0] = '\0';
                    param3[0] = '\0';
                    param4[0] = '\0';

                    ShowMenu(client_socket, loggedIn);
                    break;
                }
            }
            if (!found) {

                send(client_socket, "\nERROR: Incorrect username or password.\n", strlen("\nERROR: Incorrect username or password.\n"), 0);
            }

            LeaveCriticalSection(&cs);

        } else if (strcmp(command, "LOGOUT") == 0) {

            if (!loggedIn) { 
                send(client_socket, "\nERROR: Not logged in. Cannot logout.\n", strlen("\nERROR: Not logged in. Cannot logout.\n"), 0);
                continue;
            }

            EnterCriticalSection(&cs);

            for (int i = 0; i < userCount; i++) {

                if (strcmp(users[i].userName, currentUser) == 0 && users[i].online == 1) {

                    users[i].online = 0;
                    users[i].socket = INVALID_SOCKET;
                    SaveUsers();
                    loggedIn = 0;
                    send(client_socket, "\nSUCCESS: Logout successful.\n", strlen("\nSUCCESS: Logout successful.\n"), 0);
                    ShowMenu(client_socket, loggedIn);
                    break;
                }
            }

            LeaveCriticalSection(&cs);

        } else if (strcmp(command, "MSG") == 0) { // Send a message

            if (!loggedIn) {
                send(client_socket, "\nERROR: Not logged in. Cannot send messages.\n", strlen("\nERROR: Not logged in. Cannot send messages.\n"), 0);
                continue;
            }

            EnterCriticalSection(&cs);

            char message[BUFFER_SIZE];

            char msg_command[50]; // MSG - Unused - Just for parsing
            char direction[50]; // Username or * - Unused - Just for parsing
            char actuall_message[50]; // Message

            sscanf(buffer, "%s %s %[^\n]", msg_command, direction, actuall_message);
            
            snprintf(message, BUFFER_SIZE, "\nMSG from %s: %s\n", currentUser, actuall_message); 

            if (strcmp(param1, "*") == 0) { // Send the message to all users

                for (int i = 0; i < userCount; i++) {

                    if (users[i].online == 1 && users[i].socket != client_socket) {
                        send(users[i].socket, message, strlen(message), 0);
                    }
                }

            } else {

                int found = 0; // Represents if the user is found

                // Send the message to the specified user
                for (int i = 0; i < userCount; i++) { 

                    if (strcmp(users[i].userName, param1) == 0) { // Check if the user exists

                        if (users[i].online == 1) { 
                            send(users[i].socket, message, strlen(message), 0);
                        } else {
                            send(client_socket, "\nERROR: User is offline. Cannot send message.\n", strlen("\nERROR: User is offline. Cannot send message.\n"), 0);
                        }

                        found = 1; 
                        break;
                    }
                }
                if (!found) {
                    send(client_socket, "\nERROR: User not found.\n", strlen("\nERROR: User not found.\n"), 0);
                }
            }

            actuall_message[0] = '\0'; // Clear the message

            LeaveCriticalSection(&cs);

        } else if (strcmp(command, "LIST") == 0) { // List all users

            if (!loggedIn) {
                send(client_socket, "\nERROR: Not logged in. Cannot list users.\n", strlen("\nERROR: Not logged in. Cannot list users.\n"), 0);
                continue;
            }

            EnterCriticalSection(&cs);

            char list[BUFFER_SIZE] = "";

            for (int i = 0; i < userCount; i++) {

                char userInfo[200];
                snprintf(userInfo, 200, "%s: %s\n", users[i].userName, users[i].online ? "online" : "offline"); // Display the user and their status
                strcat(list, userInfo); // Append the user's status to the list
            }

            strcat(list, "\n"); // For formatting purposes

            send(client_socket, list, strlen(list), 0); // Send the list to the client
            LeaveCriticalSection(&cs);

        } else if (strcmp(command, "INFO") == 0) { // Get specific user information

            if (!loggedIn) {
                send(client_socket, "\nERROR: Not logged in. Cannot get user info.\n", strlen("\nERROR: Not logged in. Cannot get user info.\n"), 0);
                continue;
            }

            EnterCriticalSection(&cs);

            int found = 0; // Represents if the user is found

            for (int i = 0; i < userCount; i++) {

                if (strcmp(users[i].userName, param1) == 0) { // Check if the user exists

                    char info[BUFFER_SIZE];
                    snprintf(info, BUFFER_SIZE, "\nInfo\n----------\nName: %s\nSurname: %s\nMood: %s\n\n", users[i].name, users[i].surname, users[i].mood);
                    send(client_socket, info, strlen(info), 0);
                    found = 1;
                    break;
                }
            }

            if (!found) {
                send(client_socket, "\nERROR: User not found.\n", strlen("\nERROR: User not found.\n"), 0);
            }

            LeaveCriticalSection(&cs);

        } else if (strcmp(command, "EXIT") == 0) {
            
            if(strcmp(currentUser, "") != 0){ // If the user is logged in and types "EXIT", log them out
                printf("\n%s disconnected.\n", currentUser);
            
            } else { // If the user is not logged in and types "EXIT", close the connection
                printf("\nClient disconnected.\n");
            }

            break;

        } else {
            send(client_socket, "\nERROR: Invalid command.\n", strlen("\nERROR: Invalid command.\n"), 0);
        }
    }

    EnterCriticalSection(&cs);

    // Change the user's status to offline and set their socket to INVALID_SOCKET
    for (int i = 0; i < userCount; i++) {

        if (users[i].socket == client_socket) {
            users[i].online = 0;
            users[i].socket = INVALID_SOCKET;
            SaveUsers();
            break;
        }
    }

    LeaveCriticalSection(&cs);

    closesocket(client_socket);
    free(clientSocket); // Free the memory allocated for the client socket
    return 0;
}

int main() {

    WSADATA wsa; // Winsock data
    SOCKET serverSocket, clientSocket;
    struct sockaddr_in server, client; // Structs to store server and client information
    int clientSize = sizeof(struct sockaddr_in); // Size of the client struct

    InitializeCriticalSection(&cs); 

    LoadUsers();

    printf("\nInitializing Winsock...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("\nWinsock initialization failed. Error Code : %d\n", WSAGetLastError());
        return 1;
    }

    // Create a socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("\nSocket creation failed. Error Code : %d\n", WSAGetLastError());
        return 1;
    }

    // Server configurations
    server.sin_family = AF_INET; // IPv4
    server.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP address
    server.sin_port = htons(PORT); // Port number

    // Bind the socket
    if (bind(serverSocket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("\nBind failed. Error Code : %d\n", WSAGetLastError());
        return 1;
    }

    // Listen for connections
    if(listen(serverSocket, MAX_LISTEN_QUEUE) == SOCKET_ERROR) {
        printf("\nQueue is full, try again later.");
        return 1;
    }

    printf("\nServer started. Waiting for connections...\n");

    while (1) { 

        if(activeClients >= MAX_ACTIVE_CLIENTS) {
            printf("\nServer is full. Try again later.\n");
            break;
        }

        // Accept a connection
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&client, &clientSize)) == INVALID_SOCKET) {
            printf("\nAccept failed. Error Code : %d\n", WSAGetLastError());
            return 1;
        }

        printf("Connection accepted.\n");

        SOCKET *newSock = (SOCKET*)malloc(sizeof(SOCKET)); // Allocate memory for the client socket
        *newSock = clientSocket; // Set the client socket

        // Create a thread to handle the client
        HANDLE clientThread = CreateThread(NULL, 0, HandleClient, (LPVOID)newSock, 0, NULL); 
        if (clientThread == NULL) {
            printf("\nClient thread creation failed. Error Code : %d\n", GetLastError());
            return 1;
        }
    }

    if (clientSocket == INVALID_SOCKET) { 
        printf("\nAccept failed. Error Code : %d\n", WSAGetLastError());
        return 1;
    }

    closesocket(serverSocket);
    WSACleanup(); // Clean up Winsock
    DeleteCriticalSection(&cs); 
    return 0;
}
