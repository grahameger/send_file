/*
 
 sendfile.c
 
 Copyright 2018 Graham Eger
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#ifndef HOST_NAME_MAX
#if defined(__APPLE__)
#define HOST_NAME_MAX 255
#else
#define HOST_NAME_MAX 64
#endif /* __APPLE__ */
#endif /* HOST_NAME_MAX */

static void client(char ** args);
static void server(void);
static void server_helper(int sockfd);
static void cmd_error(void);
static void help(void);
static unsigned long long send_helper(int sockfd, void * message, unsigned long long len);
static void error(int error_number);
static void error_print(const char * to_print);

static const char HELP_MESSAGE[] = "This program sends and receives files over a network.\nCall the program with ./sendfile --help to print this message.\nCall the program with argument 'r' to start the program in server mode.\nCall the program with argument 's' to start the program in client mode.\nFollow the prompts to send a file.";
static const unsigned int PORT_LEN_MAX = 5;
pthread_mutex_t print_lock;

enum ERRORS {
    FILE_OPEN,
    OPEN_SOCK,
    HOST_ERROR,
    CONNECT_ERROR,
    CLOSED_BY_SERVER,
    RETURNED_NEG,
    SETSOCKOPT_FAIL,
    BIND_ERROR,
    LISTEN_ERROR,
    GETSOCKNAME_ERROR,
    ACCEPT_ERROR,
    MAIN_RETURNED,
    RECV_ERROR,
    THREAD_CREATE_ERROR
};

// requires: argv[1] == "r" || "s"
// modifies: modifies filesystem when
// effects : calls the client() or server() funciton
//              depending on the command line arg
int main(int argc, char ** argv) {
    // run the server
    if (argc == 2 && strcmp(argv[1], "r") == 0) {
        server();
    }
    // run the client
    else if (argc == 5 && strcmp(argv[1], "s") == 0) {
        client(argv);
    }
    // print the help message
    else if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        help();
    }
    // any other error
    else {
        cmd_error();
    }
}

// requires: nothing
// modifies: nothing
// effects : exit(1)
static void cmd_error() {
    fprintf(stderr, "%s\n", "invalid command line arguments");
    exit(1);
}

// requires: nothing
// modifies: nothing
// effects : exit(1)
static void help() {
    fprintf(stderr, "%s\n", HELP_MESSAGE);
    exit(2);
}

// requires: a valid filename, hostname, and port number (in that order)
// modifies: nothing (in this context)
// effects : sends file to hostname, port over the network
static void client(char ** args) {
    char filename[FILENAME_MAX];
    char hostname[HOST_NAME_MAX];
    char port_str[HOST_NAME_MAX];
    int sockfd, port;
    unsigned long long file_size, total, bytes;
    FILE * fptr;
    struct sockaddr_in serv_addr;
    struct hostent * server;
    char null_char = '\0';
    
    // get the filename, hostname, and port in
    strcpy(filename, args[2]);
    strcpy(hostname, args[3]);
    strcpy(port_str, args[4]);
    port = atoi(port_str);
    
    // open the file and error check
    fptr = fopen(filename, "rb");
    if (fptr == NULL) {
        error(FILE_OPEN);
    }
    // get the size of the file
    fseek(fptr, 0, SEEK_END); // seek to end of file
    file_size = ftell(fptr); // get current file pointer
    fseek(fptr, 0, SEEK_SET); // seek back to beginning of file
    
    // setup the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error(OPEN_SOCK);
    }
    
    // get the server info
    server = gethostbyname(hostname);
    if (server == NULL) {
        error(HOST_ERROR);
    }
    
    // zero and prepare the sockaddr_in structs
    bzero((char *) &serv_addr, sizeof(server));
    serv_addr.sin_family = AF_INET; // IPv4
    bcopy((char*)server->h_addr_list, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    // connect to the server
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        error(CONNECT_ERROR);
    }
    
    // now we're going to send the stuff in multiple calls
    // the message will look something like this
    // <64BIT UNSIGNED INT><FILENAME><NULL><FILEDATA>
    // size = sizeof(uint64) + strlen(filename) + sizeof(char) + filelength
    total = sizeof(file_size) + strlen(filename) + sizeof(char) + file_size;
    
    // send the whole payload
    send_helper(sockfd, &total, sizeof(total));
    send_helper(sockfd, filename, strlen(filename));
    send_helper(sockfd, &null_char, sizeof(null_char));
    send_helper(sockfd, fptr, file_size);
    
    close(sockfd);
}

// requires: valid sockfd, valid message pointer, good length
// modifies: the network
// effects : sends message of length len over the socket sockfd, returns the total bytes sent
static unsigned long long send_helper(int sockfd, void * message, unsigned long long len) {
    unsigned long long return_val = send(sockfd, message, len, 0);
    if (return_val == -1) {
        error(RETURNED_NEG);
    } else if (return_val == 0) {
        error(CLOSED_BY_SERVER);
    }
    return 0;
}

// requires: argv[1] == "r" || "s"
// modifies: modifies filesystem when
// effects : calls the client() or server() funciton
static void server() {
    int port = 0; // we can change this somewhere if we want to assign a port
    
    // this is the socket that clients connect to
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        error(OPEN_SOCK);
    }
    //
    int new_fd, rval, optval = 1, sockopt_ret;
    sockopt_ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (sockopt_ret != 0) {
        error(SETSOCKOPT_FAIL);
    }
    
    // now do the bind stuff
    struct sockaddr_in server;
    bzero((char *) &server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)(&server), sizeof(server)) == -1) {
        error(BIND_ERROR);
    }
    
    // call listen
    rval = listen(sockfd, 10); // queue of incoming connection of size 10
    if (rval == -1) {
        error(LISTEN_ERROR);
    }
    
    // get and print the actual port
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1) {
        error(GETSOCKNAME_ERROR);
    } else {
        port = ntohs(sin.sin_port);
    }
    
    // print the port and hostname
    pthread_mutex_lock(&print_lock);
    fprintf(stdin, "Port: %d\n", port);
    pthread_mutex_unlock(&print_lock);
    
    // thread creation loop
    while (1) {
        new_fd = accept(sockfd, NULL, NULL);
        if (new_fd != -1) {
            error(ACCEPT_ERROR);
        }
        // create the new thread
        pthread_t thread;
        int result = pthread_create(&thread, NULL, server_helper, new_fd);
        if (result < 0) {
            error(THREAD_CREATE_ERROR);
        }
    }
    error(MAIN_RETURNED);
}

static void server_helper(int sockfd) {
    // do one recv call of the size of a 64-bit integer
    int rtval, bytes;
    unsigned long long total;
    bytes = recv(sockfd, &total, sizeof(total), 0);
    
    // allocate a buffer and recv the rest
    char * buf = malloc(total - bytes);
    recv(sockfd, buf, total - bytes, 0);
    
    // get the filename in a buffer
    unsigned int filename_len = strlen(buf);
    unsigned long long file_len = total - sizeof(unsigned long long) - filename_len - sizeof(char);
    char * filename = malloc(filename_len + 1);
    strcpy(filename, buf);
    
    // get a pointer to the start of the file
    char * file_start = buf + filename_len + sizeof(char);
    
    // write the file
    pthread_mutex_lock(&print_lock);
    fprintf(stdout, "writing file: %s\n", filename);
    pthread_mutex_unlock(&print_lock);
    
    FILE * fptr = fopen(filename, "wb");
    fwrite(file_start, file_len, file_len, fptr);
    
    // close the socket
    close(sockfd);
}

static void error_print(const char * to_print) {
    pthread_mutex_lock(&print_lock);
    fprintf(stderr, "%s\n", to_print);
    pthread_mutex_unlock(&print_lock);
}

static void error(int error_number) {
    switch (error_number) {
        case FILE_OPEN:
            error_print("file opening error");
            break;
        case OPEN_SOCK:
            error_print("error opening socket");
            break;
        case HOST_ERROR:
            error_print("host not found");
            break;
        case CONNECT_ERROR:
            error_print("connect error");
            break;
        case RETURNED_NEG:
            error_print("send returned negative in client");
            break;
        case CLOSED_BY_SERVER:
            error_print("client: server closed connection");
            break;
        case SETSOCKOPT_FAIL:
            error_print("setsockopt failed");
            break;
        case BIND_ERROR:
            error_print("bind failed");
            break;
        case LISTEN_ERROR:
            error_print("listen failed");
            break;
        case GETSOCKNAME_ERROR:
            error_print("getsockname error");
            break;
        case ACCEPT_ERROR:
            error_print("accept error");
            break;
        case MAIN_RETURNED:
            error_print("main returned");
            break;
        case RECV_ERROR:
            error_print("recv error");
            break;
        case THREAD_CREATE_ERROR:
            error_print("thread creation error");
            break;
        default:
            break;
    }
    exit(1);
}

