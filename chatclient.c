#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "util.h"

// author @ Cavin Gada

int client_socket = -1;
char username[MAX_NAME_LEN + 1];
char inbuf[BUFLEN + 1];
char outbuf[MAX_MSG_LEN + 1];


int handle_stdin() {
    int userNameInput = get_string(outbuf, MAX_MSG_LEN+1); // use get string to get the user input 
    if (userNameInput!=TOO_LONG) { // if its not too long we have a couple cases..
        int sendBytes = send(client_socket, outbuf, strlen(outbuf), 0); // try sending
        if (sendBytes < 0) { // damn, our send failed :()
            fprintf(stderr, "Warning: Failed to send message to server. %s.\n", strerror(errno));
        }
        if (strcmp("bye", outbuf) == 0) { // if the user entered bye, we gotta get outta here
            printf("Goodbye.\n");
            return 1;
        }
        return 0;
    }
    else { // god damn the user entering hella stuff
        printf("Sorry, limit your message to %d characters.\n", MAX_MSG_LEN);
    }
    return 0;
}

int handle_client_socket() {
    int recvbytes = recv(client_socket, inbuf, BUFLEN-1, 0); // recieve, code is very similar to main function receiving
    if (recvbytes < 0) {
        fprintf(stderr, "Warning: Failed to receive incoming message. %s.\n", strerror(errno));
    }
    if (recvbytes == 0) {
        fprintf(stderr, "\nConnection to server has been lost.\n");
        return 1;
    }
    if (strcmp("bye", inbuf)==0) { // similar cases to handling stdin
        printf("\nServer initiated shutdown.\n");
        return 1;
    }
    inbuf[recvbytes] = '\0'; // THIS IS VERY IMPORATNT. we must set null terminator so we can print the CORRECT msg contents
    printf("\n%s\n", inbuf); // the new line b4 and after is important. withotu it, the messages will get jumbled on other user's lines
    return 0;
}

int main(int argc, char *argv[]) { 

    /* check if number of arguments in command line is correct!*/
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server IP> <port>\n", argv[0]);
        return 1;
    }

        /* set up socket address */ // all of this from sock folder notes
    struct sockaddr_in tower_addr;
    socklen_t addr_size = sizeof(tower_addr);;
    memset(&tower_addr, 0, sizeof(tower_addr));
    tower_addr.sin_family = AF_INET;

    /* check if IP is valid! */
        
    // https://stackoverflow.com/questions/791982/determine-if-a-string-is-a-valid-ipv4-address-in-c
    int ip = inet_pton(AF_INET, argv[1], &(tower_addr.sin_addr));

    if (ip == 0) {
        fprintf(stderr, "Error: Invalid IP address '%s'.\n", argv[1]);
        return 1;
    }

    int portParsing; // need to keep a variable to hold the port number after parsing
    bool portParsingSuccess = parse_int(argv[2], &portParsing, "port number");

    if (portParsingSuccess == false) { // parsing failed
        return 1;
    }

    if (!(portParsing <= 65535 && portParsing >= 1024)) { // number is out of range
        fprintf(stderr, "Error: Port must be in range [1024, 65535].\n");
        return 1;
    }

    tower_addr.sin_port = htons(portParsing); // set the port

    /* Username time! */

    int userNameInput; // keeps track of whether username is valid or not !
    bool firstIteration = true; // we need this mark because my loop wasn't working the first iteration. 
    while(true || firstIteration) { 
        printf("Enter your username: ");
        fflush(stdout); // flush since no new line
        userNameInput = get_string(username, 1+MAX_NAME_LEN); // account for null terminator. also get string will put username into username and set its status to userNameInput
        if (userNameInput == TOO_LONG) {
            printf("Sorry, limit your username to %d characters.\n", MAX_NAME_LEN);
        } 
        if (userNameInput == OK) { // finally break if our username is valid!
            break;
        }
        firstIteration = false; // clearly it isnt the first it anymore
    }

    printf("Hello, %s. Let's try to connect to the server.\n", username);

    client_socket = socket(AF_INET, SOCK_STREAM, 0); // create socket
    
    if (client_socket < 0 ) { // if socket fails to create, throw error
        fprintf(stderr, "Error: Failed to create socket. %s.\n", strerror(errno));
    	close(client_socket);
        return 1;
    }

    // WORKS WORKS WORKS!

    int connection = connect(client_socket, (struct sockaddr *) &tower_addr, addr_size); // connect and error check

    if (connection < 0) {
        fprintf(stderr, "Error: Failed to connect to server. %s.\n", strerror(errno));
        close(client_socket);
        return 1;
    }
    
    int recvbytes = recv(client_socket, inbuf, BUFLEN-1, 0); //recieve the bytes and error check
    if (recvbytes < 0) {
        fprintf(stderr, "Error: Failed to receive message from server. %s.\n", strerror(errno));
        close(client_socket);
        return 1;
    }
    if (recvbytes == 0) {
        fprintf(stderr, "All connections are busy. Try again later.\n");
        close(client_socket);
        return 1;
    }

    inbuf[recvbytes] = '\0'; // null terminate string to prevent garbage from being shown (thanks jacob :D)
    printf("\n%s\n\n", inbuf); 

    int sendBytes = send(client_socket, username, strlen(username), 0); // send bytes and error check

    if (sendBytes < 0 ) {
        fprintf(stderr, "Error: Failed to send username to server. %s.\n", strerror(errno));
        close(client_socket);
        return 1;
    }

    fd_set myfdset;

    while(1) {
        printf("[%s]: ", username);
        fflush(stdout);
        /* initializes the set pointed to by fdset to be empty. */
        FD_ZERO(&myfdset);
        /* adds the file descriptor fd to the set pointed to by fdset. */
        FD_SET(STDIN_FILENO, &myfdset);
        FD_SET(client_socket, &myfdset);

        // https://stackoverflow.com/questions/14544621/why-is-select-used-in-linux
        int selectStat = select(client_socket + 1, &myfdset, NULL, NULL, NULL);
        if (selectStat < 0 ) { /* select error*/
            fprintf(stderr, "Error: select() failed. %s.\n", strerror(errno));
            close(client_socket);
            return 1;
        }

        int handleSTDIN = FD_ISSET(STDIN_FILENO, &myfdset); // if both of these are 0, then just move on to next iteration
        int handleClient = FD_ISSET(client_socket, &myfdset);

        int success;

        // if we are handling the STDIN then update the success
        if (handleSTDIN != 0) {
            success = handle_stdin();
        }

        // if we are handling the client, then update success
        if (handleClient != 0) {
            success = handle_client_socket();
        }

        // if we said goodbye, close socket and exit!
        if (success == 1) {
            close(client_socket);
            return 0;
        }
        
        
    }
    return 0;
}