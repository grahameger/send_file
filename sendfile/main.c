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

#ifndef HOST_NAME_MAX
#if defined(__APPLE__)
#define HOST_NAME_MAX 255
#else
#define HOST_NAME_MAX 64
#endif /* __APPLE__ */
#endif /* HOST_NAME_MAX */

static void client(char ** args);
static void server(void);
static void cmd_error(void);
static void help(void);
static void send_wrapper(int socket, void * message, unsigned long long len);

static const char HELP_MESSAGE[] = "This program sends and receives files over a network.\nCall the program with ./sendfile --help to print this message.\nCall the program with argument 'r' to start the program in server mode.\nCall the program with argument 's' to start the program in client mode.\nFollow the prompts to send a file.";

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
    else if (argc == 4 && strcmp(argv[1], "s") == 0) {
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

static void cmd_error() {
    fprintf(stderr, "%s\n", "invalid command line arguments");
    exit(1);
}

static void help() {
    fprintf(stderr, "%s\n", HELP_MESSAGE);
    exit(2);
}

static void client(char ** args) {
    
}

static void server() {
    
}
