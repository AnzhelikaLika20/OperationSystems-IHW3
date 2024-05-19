#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Server IP> <Monitor Port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    int monitor_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    if ((monitor_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(monitor_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(monitor_socket, 1) == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }

    printf("Monitor listening on %s:%d\n", ip, port);

    int client_socket;
    socklen_t client_addr_len = sizeof(server_addr);
    if ((client_socket = accept(monitor_socket, (struct sockaddr *)&server_addr, &client_addr_len)) == -1) {
        perror("accept");
        return EXIT_FAILURE;
    }

    printf("Connected to server.\n");

    while (1) {
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            perror("recv");
            break;
        }

        buffer[bytes_received] = '\0'; // Null-terminate the received string
        printf("%s", buffer);
    }

    close(client_socket);
    close(monitor_socket);
    return EXIT_SUCCESS;
}
