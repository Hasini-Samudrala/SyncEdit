#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 10

int client_sockets[MAX_CLIENTS];
int client_count = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

// broadcast function
void broadcast(char* message, int sender_socket) {
    pthread_mutex_lock(&lock);

    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] != sender_socket) {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&lock);
}

void* handle_client(void* socket_desc) {
    int client_socket = *(int*)socket_desc;
    char buffer[1024];
    char name[50];

    // receive name
    read(client_socket, name, sizeof(name));

    pthread_mutex_lock(&print_lock);
    printf("Client connected: %s\n", name);
    fflush(stdout);
    pthread_mutex_unlock(&print_lock);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(client_socket, buffer, sizeof(buffer));

        if (valread <= 0) {
            pthread_mutex_lock(&print_lock);
            printf("%s disconnected\n", name);
            fflush(stdout);
            pthread_mutex_unlock(&print_lock);
            break;
        }

        char full_msg[1100];
        sprintf(full_msg, "[%s]: %s\n", name, buffer);

        // print safely
        pthread_mutex_lock(&print_lock);
        printf("%s", full_msg);
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);

        // broadcast
        broadcast(full_msg, client_socket);
    }

    close(client_socket);
    free(socket_desc);
    return NULL;
}

int main() {
    int server_fd, *new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 5);

    printf("Server started...\n");
    fflush(stdout);

    while (1) {
        new_socket = malloc(sizeof(int));

        *new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);

        if (*new_socket < 0) {
            printf("Accept failed\n");
            continue;
        }

        pthread_mutex_lock(&lock);
        client_sockets[client_count++] = *new_socket;
        pthread_mutex_unlock(&lock);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void*)new_socket);
        pthread_detach(thread_id);
    }

    return 0;
}
