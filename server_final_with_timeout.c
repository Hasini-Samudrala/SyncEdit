#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>

#define PORT 8080

void* handle_client(void* socket_desc) {
    int client_socket = *(int*)socket_desc;
    char buffer[1024] = {0};
    char name[50] = {0};

    // receive client name
    read(client_socket, name, sizeof(name));
    printf("Client connected: %s\n", name);

    while (1) {
        int valread = read(client_socket, buffer, sizeof(buffer));

        if (valread <= 0) {
            printf("%s disconnected\n", name);
            break;
        }

        printf("[%s]: %s\n", name, buffer);

        memset(buffer, 0, sizeof(buffer));
    }

    close(client_socket);
    free(socket_desc);
    return NULL;
}

int main() {
    int server_fd, *new_socket;
    struct sockaddr_in server_addr, client_addr;
    int addrlen = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    printf("Server started... Waiting for clients...\n");

    while (1) {
        fd_set set;
        struct timeval timeout;

        FD_ZERO(&set);
        FD_SET(server_fd, &set);

        timeout.tv_sec = 60;   // 60 seconds
        timeout.tv_usec = 0;

        printf("Waiting for client (timeout 60s)...\n");

        int activity = select(server_fd + 1, &set, NULL, NULL, &timeout);

        if (activity == 0) {
            printf("No clients connected for 60 seconds. Server exiting...\n");
            break;
        }

        if (activity < 0) {
            printf("Error in select\n");
            break;
        }

        new_socket = malloc(sizeof(int));

        *new_socket = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);

        if (*new_socket < 0) {
            printf("Accept failed\n");
            continue;
        }

        printf("New client connected!\n");

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void*)new_socket);
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
