// TODO :
// handle shown messages on terminal
// printf && must be removed
// more readable code IF possible!

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
#include <unistd.h>
#include <fcntl.h>
#define SSIZE_MAX 100000

int get_download_server(char* buf){
    int ans=0;
    ans = (buf[9]-'0')*10000 + (buf[10]-'0')*1000 + (buf[11]-'0')*100 + (buf[12]-'0')*10 + (buf[13]-'0');
    return ans;
}

char* get_filename(char* buf){
    int end=0;
    char filename[256];

    for(end=15;end<strlen(buf);end++){
        filename[end-15]=buf[end];
        if(buf[end]=='\0' || buf[end]=='\n'){
            filename[end-15]='\0';
            break;
        }
    }
    return (char*)filename;
}

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
    char buf[1048576];
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
                        //printf("%s: New connection from %s on socket %d\n", argv[0], inet_ntoa(clientaddr.sin_addr), newfd);

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
                        int dl_sockfd;
                        char fname[1024];
                        /* we got some data from a client*/
                        for(j = 0; j <= fdmax; j++) {
                            /* send to everyone! */
                            if(FD_ISSET(j, &read_fds)) {
                                if(j==STDIN_FILENO ){
                                    //Download
                                    if(buf[0]=='D'){
                                        strncpy(fname,buf,1024);
                                        struct sockaddr_in download_server;
                                        int dl_server_port = get_download_server(buf);
                                        dl_sockfd = socket(AF_INET, SOCK_STREAM, 0);
                                        if (dl_sockfd < 0)
                                            perror("ERROR opening socket");
                                        bzero((char *) &download_server, sizeof(download_server));
                                        download_server.sin_family = AF_INET;
                                        download_server.sin_addr.s_addr = INADDR_ANY;
                                        download_server.sin_port = htons(dl_server_port);
                                        if (connect(dl_sockfd ,(struct sockaddr *)&download_server,sizeof(download_server)) < 0)
                                            perror("ERROR connecting");
                                        if((send(dl_sockfd,buf, nbytes,0)) == -1)
                                            perror("could not connect to Download server");
                                        if(dl_sockfd>fdmax)
                                            fdmax=dl_sockfd;
                                        FD_SET(dl_sockfd,&master);
                                        if(send(dl_sockfd,buf,strlen(buf),0) == -1){
                                            perror("problem in send buffer to Download server!");
                                        }
                                    }

                                    if(buf[0]=='L' || buf[0]=='R'){
                                        if((send(lookup_sockfd,buf, nbytes,0)) == -1)
                                            perror("could not connect to Lookup server");
                                    }
                                    //TODO : close command
                                }
                                else if(j==dl_sockfd){
                                    int write_fd = open (get_filename(fname), O_WRONLY | O_CREAT | O_APPEND,0777);
                                    if((write(write_fd,buf,nbytes)<=0)){
                                        printf("%s\n",get_filename(fname));
                                        perror("error writing in client:");
                                    }
                                }
                                else{
                                    if(buf[0]=='D'){
                                            int fd = open(get_filename(buf), O_RDONLY);
                                            if (fd == -1) {
                                              perror("problem opening file");
                                              exit(1);
                                            }

                                            /* get the size of the file to be sent */
                                            struct stat stat_buf;
                                            fstat(fd, &stat_buf);

                                            int rc;
                                            /* copy file using sendfile */
                                            off_t offset = 0;
                                             /* copy file using sendfile */
                                            size_t part = (stat_buf.st_size/10);
                                            while (offset < stat_buf.st_size) {
                                              size_t count;
                                              off_t remaining = stat_buf.st_size- offset;
                                              if (remaining > part)
                                                  count = part;
                                               else
                                                  count = remaining;
                                              rc = sendfile(j, fd, &offset, count);
                                              if (rc == 0) {
                                                 break;
                                              }
                                              if (rc == -1) {
                                                perror("error :(");
                                                 exit(1);
                                              }
                                            }

                                            if (offset != stat_buf.st_size) {
                                              perror("error :(");
                                              exit(1);
                                            }

                                            close(fd);
                                            close(j);
                                            FD_CLR(j,&master);
                                    }
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
