#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char *argv[]) {


    int lookup_sockfd,n;
    int lookup_server_port;

    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    lookup_server_port = atoi(argv[2]);
    lookup_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lookup_sockfd < 0)
        perror("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(lookup_server_port);
    if (connect(lookup_sockfd ,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        perror("ERROR connecting");

    int my_portno = atoi(argv[3]);
    fd_set master;
    /* temp file descriptor list for select() */
    fd_set read_fds;
    /* server address */
    struct sockaddr_in serveraddr;
    /* client address */
    struct sockaddr_in clientaddr;
    /* maximum file descriptor number */
    int fdmax;
    /* listening socket descriptor */
    int listener;
    /* newly accept()ed socket descriptor */
    int newfd;
    /* buffer for client data */
    char buf[1024];
    int nbytes;
    /* for setsockopt() SO_REUSEADDR, below */
    int yes = 1;
    int addrlen;
    int i, j;
    /* clear the master and temp sets */
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    /* get the listener */
    if((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Server-socket() error lol!");
        /*just exit lol!*/
        exit(1);
    }
    printf("Server-socket() is OK...\n");
    /*"address already in use" error message */
    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("Server-setsockopt() error lol!");
        exit(1);
    }
    printf("Server-setsockopt() is OK...\n");

    /* bind */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(my_portno);
    memset(&(serveraddr.sin_zero), '\0', 8);

    if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
        perror("Server-bind() error lol!");
        exit(1);
    }
    printf("Server-bind() is OK...\n");
    /* listen */
    if(listen(listener, 10) == -1) {
        perror("Server-listen() error lol!");
        exit(1);
    }
    printf("Server-listen() is OK...\n");

    /* add the listener to the master set */
    FD_SET(listener, &master);
    //FD_SET(0,&master);
    FD_SET(lookup_sockfd, &master);
    FD_SET(STDIN_FILENO,&master);
    /* keep track of the biggest file descriptor */
    fdmax = listener; /* so far, it's this one*/
    printf("listerner = %d\n",listener);
    printf("lookup server = %d\n",lookup_sockfd);
    /* loop */
    for(;;) {
        /* copy it */
        read_fds = master;

        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Server-select() error lol!");
            exit(1);
        }
        printf("Server-select() is OK...\n");

        /*run through the existing connections looking for data to be read*/
        for(i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &read_fds)) { /* we got one... */
                printf("valid descriptor is:%d\n",i);
                if(i == listener) {
                    /* handle new connections */
                    addrlen = sizeof(clientaddr);
                    if((newfd = accept(listener, (struct sockaddr *)&clientaddr, &addrlen)) == -1) {
                        perror("Server-accept() error lol!");
                    }
                    else {
                        printf("Server-accept() is OK...\n");
                        FD_SET(newfd, &master); /* add to master set */
                        if(newfd > fdmax) { /* keep track of the maximum */
                            fdmax = newfd;
                        }
                        printf("%s: New connection from %s on socket %d\n", argv[0], inet_ntoa(clientaddr.sin_addr), newfd);
                    }
                }
                else {
                    /* handle data from a client */
                    if((nbytes = read(i, buf, sizeof(buf))) <= 0) {
                        /* got error or connection closed by client */
                        if(nbytes == 0)
                            /* connection closed */
                            printf("%s: socket %d hung up\n", argv[0], i);

                        else{
                            perror("recv() error lol!");
                        }
                        /* close it... */
                        close(i);
                        /* remove from master set */
                        FD_CLR(i, &master);
                    }
                    else {
                        /* we got some data from a client*/
                        for(j = 0; j <= fdmax; j++) {
                            /* send to everyone! */
                            if(FD_ISSET(j, &master)) {
                                /* except the listener and ourselves */
                                /* if(j==listener){
                                    //printf("hellllo on terminal");
                                    send(j,"You wrote in Terminal!\0",255,0);
                                } */
                                if(j==0){
                                    if((send(j,buf, nbytes, 0)) == -1)
                                        perror("send() ## error lol!");
                                }
                                if(j != listener && j == i) {
                                    //if(send(j,response(buf,&all_clients), nbytes, 0) == -1)
                                    if((send(j,buf, nbytes, 0)) == -1)
                                        perror("send() -- error lol!");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
