#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addrlen = sizeof(client_addr);

    char buffer[1024] = {0};

    // create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Socket creation failed\n");
        return 1;
    }

    // setup address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // bind
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bind failed\n");
        return 1;
    }

    // listen
    listen(server_fd, 5);
    printf("Server started... Waiting for clients...\n");

    // accept client
    client_socket = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);
    if (client_socket < 0) {
        printf("Accept failed\n");
        return 1;
    }

    printf("Client connected successfully!\n");

    // receive message
    read(client_socket, buffer, sizeof(buffer));
    printf("Received message: %s\n", buffer);

    close(client_socket);
    close(server_fd);

    return 0;
}
