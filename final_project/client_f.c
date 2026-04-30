#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080

void* receive_messages(void* sock_ptr) {
    int sock = *(int*)sock_ptr;
    char buffer[2048];

    while (1) {
        int valread = read(sock, buffer, sizeof(buffer));

        if (valread <= 0) break;

        printf("\n%s\nYou: ", buffer);
        fflush(stdout);

        memset(buffer, 0, sizeof(buffer));
    }

    return NULL;
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char message[1024];
    char name[50];

    sock = socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    printf("Connected to server\n");

    printf("Enter name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = 0;

    send(sock, name, strlen(name), 0);

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sock);

    while (1) {
        printf("You: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        send(sock, message, strlen(message), 0);
    }

    close(sock);
    return 0;
}
