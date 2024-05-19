#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    int id;
    char proposal[BUFFER_SIZE];
    int charisma;
} client_t;

client_t clients[MAX_CLIENTS];
int client_count = 0;
int expected_client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t all_clients_received = PTHREAD_COND_INITIALIZER;

void *handle_client(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // Получение предложения
    if ((bytes_read = recv(client->socket, buffer, BUFFER_SIZE, 0)) <= 0) {
        printf("Client %d disconnected or error occurred.\n", client->id);
        close(client->socket);
        free(client);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    strncpy(client->proposal, buffer, BUFFER_SIZE);
    client->charisma = rand() % 100 + 1; 
    printf("Received proposal from client %d: %s\n", client->id, client->proposal);

    pthread_mutex_lock(&clients_mutex);
    clients[client->id] = *client; 
    if (client_count == expected_client_count) {
        pthread_cond_signal(&all_clients_received);
    }
    pthread_mutex_unlock(&clients_mutex);

    // Ожидание решения
    pthread_mutex_lock(&clients_mutex);
    while (client_count < expected_client_count) {
        pthread_cond_wait(&all_clients_received, &clients_mutex);
    }
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}

void notify_clients() {
    int best_client = 0;
    for (int i = 1; i < client_count; i++) {
        if (clients[i].charisma > clients[best_client].charisma) {
            best_client = i;
        }
    }
    printf("Best client: %d with charisma %d\n", best_client, clients[best_client].charisma);

    for (int i = 0; i < client_count; i++) {
        if (i == best_client) {
            send(clients[i].socket, "Accepted", 9, 0);
        } else {
            send(clients[i].socket, "Rejected", 8, 0);
        }
        close(clients[i].socket);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <IP> <Port> <Number of Clients>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    expected_client_count = atoi(argv[3]);

    if (expected_client_count > MAX_CLIENTS) {
        fprintf(stderr, "Number of clients should be less than or equal to %d\n", MAX_CLIENTS);
        return EXIT_FAILURE;
    }

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t tid;

    srand(time(NULL));

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(server_socket, expected_client_count) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }

    printf("Server listening on %s:%d\n", ip, port);

    while (client_count < expected_client_count) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            perror("accept");
            continue;
        }

        client_t *client = (client_t *)malloc(sizeof(client_t));
        client->socket = client_socket;
        client->id = client_count;
        clients[client_count] = *client;

        if (pthread_create(&tid, NULL, handle_client, (void *)client) != 0) {
            perror("pthread_create");
            free(client);
        }

        pthread_detach(tid);
        client_count++;
    }

    // Ждем завершения всех потоков
    pthread_mutex_lock(&clients_mutex);
    while (client_count < expected_client_count) {
        pthread_cond_wait(&all_clients_received, &clients_mutex);
    }
    pthread_mutex_unlock(&clients_mutex);

    notify_clients();
    close(server_socket);
    return EXIT_SUCCESS;
}
