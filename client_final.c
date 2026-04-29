#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char message[1024];
    char name[50];

    // create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    // connect
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection failed\n");
        return 1;
    }

    printf("Connected to server\n");

    // get username
    printf("Enter your name: ");
    fgets(name, sizeof(name), stdin);

    // remove newline
    name[strcspn(name, "\n")] = 0;

    // send name to server
    send(sock, name, strlen(name), 0);

    while (1) {
        printf("You: ");
        fgets(message, sizeof(message), stdin);

        // remove newline
        message[strcspn(message, "\n")] = 0;

        // exit condition
        if (strcmp(message, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        send(sock, message, strlen(message), 0);
    }

    close(sock);
    return 0;
}
