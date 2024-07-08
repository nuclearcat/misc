/*
C TCP client
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024];

    if (argc != 2) {
        printf("Usage: %s <ip of server>\n", argv[0]);
        return 1;
    }

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        return 1;
    }

    // Initialize the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(0);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        return 1;
    }

    // Close the socket
    close(sockfd);

    return 0;
}