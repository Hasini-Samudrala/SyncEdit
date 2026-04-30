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
char roles[MAX_CLIENTS][10];
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

// ===== get role =====
char* get_role(int sock) {
    for (int i = 0; i < client_count; i++)
        if (client_sockets[i] == sock)
            return roles[i];
    return "editor";
}

// ===== broadcast =====
void broadcast(char* msg, int sender) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++)
        if (client_sockets[i] != sender)
            send(client_sockets[i], msg, strlen(msg), 0);
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
            for (int j = i; j < client_count - 1; j++) {
                client_sockets[j] = client_sockets[j + 1];
                strcpy(roles[j], roles[j + 1]);
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
}

// ===== remove live =====
void remove_live_client(int sock) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < live_count; i++) {
        if (live_clients[i] == sock) {
            for (int j = i; j < live_count - 1; j++)
                live_clients[j] = live_clients[j + 1];
            live_count--;
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

// ===== client =====
void* handle_client(void* arg) {
    int sock = *(int*)arg;
    free(arg);

    char name[50] = {0};
    read(sock, name, sizeof(name));
    name[strcspn(name, "\n")] = 0;

    printf("Connected: %s\n", name);

    pthread_mutex_lock(&lock);
    int index = client_count - 1;

    // ===== ROLE + PASSWORD =====
    if (strcmp(name, "admin") == 0) {
        char password[50] = {0};

        send(sock, "Enter admin password:\n", 23, 0);
        int p = read(sock, password, sizeof(password)-1);

        if (p > 0) password[p] = '\0';
        password[strcspn(password, "\n")] = 0;

        if (strcmp(password, "admin1234") == 0) {
            strcpy(roles[index], "admin");
            send(sock, "Admin access granted\n", 21, 0);
        } else {
            strcpy(roles[index], "editor");
            send(sock, "Wrong password → editor mode\n", 32, 0);
        }
    } else {
        strcpy(roles[index], "editor");
    }

    pthread_mutex_unlock(&lock);

    while (1) {
        char buffer[1024] = {0};
        int n = read(sock, buffer, sizeof(buffer)-1);
        if (n <= 0) break;

        buffer[n] = '\0';
        buffer[strcspn(buffer, "\n")] = 0;

        char *role = get_role(sock);

        // ===== VIEW =====
        if (strcmp(buffer, "view") == 0) {
            char content[4096] = {0};
            read_file("shared.txt", content);
            send(sock, content, strlen(content), 0);
        }

        // ===== LIVE =====
        else if (strcmp(buffer, "live") == 0) {
            if (!exists(live_clients, live_count, sock))
                live_clients[live_count++] = sock;
            send(sock, "LIVE MODE\n", 10, 0);
        }

        else if (strcmp(buffer, "exit_live") == 0) {
            remove_live_client(sock);
            send(sock, "Exited LIVE\n", 12, 0);
        }

        // ===== REQUEST =====
        else if (strcmp(buffer, "request") == 0) {

            pthread_mutex_lock(&file_lock);

            if (!exists(edit_queue, queue_count, sock))
                edit_queue[queue_count++] = sock;

            if (!is_editing && queue_count > 0) {
                current_editor = edit_queue[0];

                for (int i = 1; i < queue_count; i++)
                    edit_queue[i - 1] = edit_queue[i];

                queue_count--;
                is_editing = 1;

                send(current_editor, "You can edit now\n", 18, 0);
            } else {
                send(sock, "Added to queue\n", 15, 0);
            }

            pthread_mutex_unlock(&file_lock);
        }

        // ===== FORCE EDIT (FIXED) =====
        else if (strcmp(buffer, "force_edit") == 0) {

            if (strcmp(role, "admin") != 0) {
                send(sock, "Only admin allowed\n", 19, 0);
                continue;
            }

            pthread_mutex_lock(&file_lock);

            // 🔥 push interrupted editor to FRONT
            if (is_editing && current_editor != -1) {

                if (!exists(edit_queue, queue_count, current_editor)) {

                    for (int i = queue_count; i > 0; i--)
                        edit_queue[i] = edit_queue[i - 1];

                    edit_queue[0] = current_editor;
                    queue_count++;
                }
            }

            current_editor = sock;
            is_editing = 1;

            pthread_mutex_unlock(&file_lock);

            send(sock, "Admin override granted\n", 24, 0);
        }

        // ===== EDIT =====
        else if (strcmp(buffer, "edit") == 0) {

            pthread_mutex_lock(&file_lock);

            if (sock != current_editor) {
                pthread_mutex_unlock(&file_lock);
                send(sock, "Not your turn\n", 14, 0);
                continue;
            }

            pthread_mutex_unlock(&file_lock);

            send(sock, "Send content:\n", 14, 0);

            char content[2048] = {0};
            int m = read(sock, content, sizeof(content)-1);
            if (m > 0) content[m] = '\0';

            pthread_mutex_lock(&file_lock);

            if (sock != current_editor) {
                pthread_mutex_unlock(&file_lock);
                send(sock, "Edit cancelled\n", 15, 0);
                continue;
            }

            pthread_mutex_unlock(&file_lock);

            FILE *fp = fopen("shared.txt", "a");
            if (fp) {
                fprintf(fp, "[%s]: %s\n", name, content);
                fclose(fp);
            }

            // ===== VERSION SAVE =====
            char fname[50];
            sprintf(fname, "versions/v%d.txt", version_number++);

            FILE *main_fp = fopen("shared.txt", "r");
            FILE *vfp = fopen(fname, "w");

            if (main_fp && vfp) {
                char line[256];
                while (fgets(line, sizeof(line), main_fp))
                    fputs(line, vfp);
            }

            if (main_fp) fclose(main_fp);
            if (vfp) fclose(vfp);

            send_live_update();

            pthread_mutex_lock(&file_lock);

            if (queue_count > 0) {
                current_editor = edit_queue[0];

                for (int i = 1; i < queue_count; i++)
                    edit_queue[i - 1] = edit_queue[i];

                queue_count--;

                send(current_editor, "You can edit now\n", 18, 0);
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

        // ===== VERSION =====
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

    mkdir("versions", 0777);

    int server_fd;
    struct sockaddr_in addr;

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