#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 9000
#define HOST "127.0.0.1"
#define BUFFER_SIZE 4096

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to DB Server on port %d\n", PORT);
    printf("Type HELP for available commands\n");
    printf("Type EXIT to quit\n\n");

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        FD_SET(STDIN_FILENO, &fds);

        select(sock + 1, &fds, NULL, NULL, NULL);

        // Ada data dari server
        if (FD_ISSET(sock, &fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes <= 0) {
                printf("Server disconnected\n");
                break;
            }
            printf("%s", buffer);
            fflush(stdout);
        }

        // Ada input dari user
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            memset(command, 0, BUFFER_SIZE);
            printf("db > ");
            fflush(stdout);
            if (fgets(command, BUFFER_SIZE, stdin) == NULL) break;
            send(sock, command, strlen(command), 0);
            if (strncmp(command, "EXIT", 4) == 0) break;
        }
    }

    close(sock);
    return 0;
}