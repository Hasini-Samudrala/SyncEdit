#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define MAX_QUEUE 10

int client_sockets[MAX_CLIENTS];
int client_count = 0;

int live_clients[MAX_CLIENTS];
int live_count = 0;

int edit_queue[MAX_QUEUE];
int queue_count = 0;

int is_editing = 0;
int current_editor = -1;

int version_number = 1;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

// ===== helper =====
int exists(int arr[], int size, int val) {
    for (int i = 0; i < size; i++)
        if (arr[i] == val) return 1;
    return 0;
}

// ===== broadcast =====
void broadcast(char* msg, int sender) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] != sender)
            send(client_sockets[i], msg, strlen(msg), 0);
    }
    pthread_mutex_unlock(&lock);
}

// ===== live update =====
void send_live_update() {
    FILE *fp = fopen("shared.txt", "r");
    if (!fp) return;

    char content[4096] = {0}, line[256];

    while (fgets(line, sizeof(line), fp))
        strcat(content, line);

    fclose(fp);

    pthread_mutex_lock(&lock);
    for (int i = 0; i < live_count; i++)
        send(live_clients[i], content, strlen(content), 0);
    pthread_mutex_unlock(&lock);
}

// ===== remove client =====
void remove_client(int sock) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == sock) {
            for (int j = i; j < client_count - 1; j++)
                client_sockets[j] = client_sockets[j + 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
}

// ===== read file =====
void read_file(char *filename, char *buffer) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp))
        strcat(buffer, line);

    fclose(fp);
}

void remove_live_client(int sock) {
    pthread_mutex_lock(&lock);

    for (int i = 0; i < live_count; i++) {
        if (live_clients[i] == sock) {

            for (int j = i; j < live_count - 1; j++) {
                live_clients[j] = live_clients[j + 1];
            }

            live_count--;
            break;
        }
    }

    pthread_mutex_unlock(&lock);
}

// ===== client =====
void* handle_client(void* arg) {
    int sock = *(int*)arg;
    free(arg);

    char name[50] = {0};
    read(sock, name, sizeof(name));

    printf("Connected: %s\n", name);

    while (1) {
        char buffer[1024] = {0};
        int n = read(sock, buffer, sizeof(buffer) - 1);

        if (n <= 0) break;

        buffer[n] = '\0';
        buffer[strcspn(buffer, "\n")] = 0;

        // ===== VIEW =====
        if (strcmp(buffer, "view") == 0) {
            char content[4096] = {0};
            read_file("shared.txt", content);
            send(sock, content, strlen(content), 0);
        }

        // ===== LIVE =====
        else if (strcmp(buffer, "live") == 0) {
            if (!exists(live_clients, live_count, sock) && live_count < MAX_CLIENTS)
                live_clients[live_count++] = sock;

            send(sock, "LIVE MODE\n", 10, 0);
        }
        
        //=======EXIT FROM LIVE MODE=======
        else if (strcmp(buffer, "exit_live") == 0) {
          remove_live_client(sock);
          send(sock, "Exited LIVE mode\n", 18, 0);
        }

        // ===== REQUEST =====
        else if (strcmp(buffer, "request") == 0) {
            pthread_mutex_lock(&file_lock);

            if (!is_editing) {
                is_editing = 1;
                current_editor = sock;
                send(sock, "You can edit\n", 14, 0);
            } else {
                if (!exists(edit_queue, queue_count, sock) && queue_count < MAX_QUEUE)
                    edit_queue[queue_count++] = sock;

                send(sock, "Added to queue\n", 15, 0);
            }

            pthread_mutex_unlock(&file_lock);
        }

        // ===== EDIT =====
        else if (strcmp(buffer, "edit") == 0) {
            if (sock != current_editor) {
                send(sock, "Request first\n", 14, 0);
                continue;
            }

            send(sock, "Send content:\n", 14, 0);

            char content[2048] = {0};
            int m = read(sock, content, sizeof(content) - 1);
            if (m > 0) content[m] = '\0';

            FILE *fp = fopen("shared.txt", "a");
            if (fp) {
                fprintf(fp, "[%s]: %s\n", name, content);
                fclose(fp);
            }

            // version save
            char fname[50];
            sprintf(fname, "versions/v%d.txt", version_number++);

            FILE *vfp = fopen(fname, "w");
            if (vfp) {
                fprintf(vfp, "%s", content);
                fclose(vfp);
            }

            send_live_update();

            pthread_mutex_lock(&file_lock);

            if (queue_count > 0) {
                int next = edit_queue[0];

                for (int i = 1; i < queue_count; i++)
                    edit_queue[i - 1] = edit_queue[i];

                queue_count--;
                current_editor = next;

                send(next, "You can edit now\n", 18, 0);
            } else {
                is_editing = 0;
                current_editor = -1;
            }

            pthread_mutex_unlock(&file_lock);

            send(sock, "Done\n", 5, 0);
        }

        // ===== HISTORY =====
        else if (strcmp(buffer, "history") == 0) {
            char result[1024] = "Versions:\n";

            for (int i = 1; i < version_number; i++) {
                char temp[50];
                sprintf(temp, "v%d.txt\n", i);
                strcat(result, temp);
            }

            send(sock, result, strlen(result), 0);
        }

        // ===== VERSION READ =====
        else if (strncmp(buffer, "version ", 8) == 0) {
            int num = atoi(buffer + 8);

            char fname[50];
            sprintf(fname, "versions/v%d.txt", num);

            char content[2048] = {0};
            read_file(fname, content);

            send(sock, content, strlen(content), 0);
        }

        // ===== DIFF =====
        else if (strncmp(buffer, "diff ", 5) == 0) {
            int a, b;
            sscanf(buffer + 5, "%d %d", &a, &b);

            char f1[50], f2[50];
            sprintf(f1, "versions/v%d.txt", a);
            sprintf(f2, "versions/v%d.txt", b);

            char c1[2048] = {0}, c2[2048] = {0};
            read_file(f1, c1);
            read_file(f2, c2);

            char result[4096];

            sprintf(result,
                "----- VERSION %d -----\n%s\n"
                "----- VERSION %d -----\n%s\n",
                a, c1, b, c2);

            send(sock, result, strlen(result), 0);
        }

        // ===== CHAT =====
        else {
            char msg[1100];
            sprintf(msg, "[%s]: %s\n", name, buffer);
            printf("%s", msg);
            broadcast(msg, sock);
        }
    }

    printf("%s disconnected\n", name);
    remove_client(sock);
    close(sock);
    return NULL;
}

// ===== MAIN =====
int main() {
    int server_fd;
    struct sockaddr_in addr;
    mkdir("versions", 0777);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("Server started...\n");

    while (1) {
        int *sock = malloc(sizeof(int));
        *sock = accept(server_fd, NULL, NULL);

        pthread_mutex_lock(&lock);
        if (client_count < MAX_CLIENTS)
            client_sockets[client_count++] = *sock;
        pthread_mutex_unlock(&lock);

        pthread_t t;
        pthread_create(&t, NULL, handle_client, sock);
        pthread_detach(t);
    }

    return 0;
}
