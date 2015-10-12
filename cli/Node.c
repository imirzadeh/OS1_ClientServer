// TODO :
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

#define MB (1024*1024)
#define SSIZE_MAX 100000
#define MAXCLIENTS 10
#define MAXFNLEN 256
#define BUFFERLEN 4096

int get_download_server(char* buf){
    int ans=0;
    ans = (buf[9]-'0')*10000 + (buf[10]-'0')*1000 + (buf[11]-'0')*100 + (buf[12]-'0')*10 + (buf[13]-'0');
    return ans;
}

int proper_sector_size(int filesize){
    if(0 <= filesize && filesize <= 1*MB )
        return 1024;
    else if( 1*MB < filesize && filesize < 10*MB )
        return 2048;
    else
        return 4096;
}


char* get_filename(char* buf){
    int end=0;
    char filename[MAXFNLEN];

    for(end=15;end<strlen(buf);end++){
        filename[end-15]=buf[end];
        if(buf[end]=='\0' || buf[end]=='\n'){
            filename[end-15]='\0';
            break;
        }
    }
    return (char*)filename;
}

void cout(char* msg){
    write(STDIN_FILENO,msg,strlen(msg));
}

int main(int argc, char *argv[]) {
    int lookup_sockfd,n;
    int lookup_server_port;

    struct sockaddr_in serv_addr;
    struct hostent *server;

    lookup_server_port = atoi(argv[2]);
    lookup_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lookup_sockfd < 0)
        cout("could\'nt creat socket\n");

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        cout("ERROR | coudl\'t get host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(lookup_server_port);
    if (connect(lookup_sockfd ,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        cout("ERROR | couldn\'t connect\n");

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
    char buf[BUFFERLEN];
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
        cout("ERROR | Server Socket Problem\n");
        /*just exit lol!*/
        exit(1);
    }
    cout("INFO | server socket created successfully!\n");
    /*"address already in use" error message */
    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        cout("ERROR | couldn\'t set socket\n");
        exit(1);
    }
    cout("INFO | server socket set successfully!\n");
    /* bind */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(my_portno);
    memset(&(serveraddr.sin_zero), '\0', 8);


    if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
        cout("ERROR | server bind problem!\n");
        exit(1);
    }
    cout("INFO | server binded to it\'s socket\n");
    /* listen */
    //XXX : MACRO for 10
    if(listen(listener, MAXCLIENTS) == -1) {
        cout("ERROR | listening problem \n");
        exit(1);
    }
    cout("INFO | listen successfully!\n");

    /* add the listener to the master set */
    FD_SET(listener, &master);
    //FD_SET(0,&master);
    FD_SET(lookup_sockfd, &master);
    FD_SET(STDIN_FILENO,&master);
    /* keep track of the biggest file descriptor */
    fdmax = listener; /* so far, it's this one*/

    //printf("listerner = %d\n",listener);
    //printf("lookup server = %d\n",lookup_sockfd);

    /* loop */
    for(;;) {
        /* copy it */
        read_fds = master;

        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Server-select() error lol!");
            cout("ERROR | problem in select()\n");
            exit(1);
        }

        /*run through the existing connections looking for data to be read*/
        for(i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &read_fds)) { /* we got one... */
                if(i == listener) {
                    /* handle new connections */
                    addrlen = sizeof(clientaddr);
                    if((newfd = accept(listener, (struct sockaddr *)&clientaddr, &addrlen)) == -1) {
                        cout("ERROR | Server-accept() problem! \n");
                    }
                    else {
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
                            cout("INFO | Download completed\n");
                            //printf("%s: socket %d hung up\n", argv[0], i);
                        else{
                            cout("ERROR | recieve() problem!\n");
                        }
                        close(i);
                        /* remove from master set */
                        FD_CLR(i, &master);
                    }
                    else {
                        int dl_sockfd;
                        char fname[MAXFNLEN];
                        /* we got some data from a client*/
                        for(j = 0; j <= fdmax; j++) {
                            /* send to everyone! */
                            if(FD_ISSET(j, &read_fds)) {
                                if(j==STDIN_FILENO ){
                                    //Download
                                    if(buf[0]=='D' && buf[1]=='o'){
                                        strncpy(fname,buf,1024);
                                        struct sockaddr_in download_server;
                                        int dl_server_port = get_download_server(buf);
                                        dl_sockfd = socket(AF_INET, SOCK_STREAM, 0);
                                        if (dl_sockfd < 0)
                                            cout("ERROR | opening socket\n");
                                        bzero((char *) &download_server, sizeof(download_server));
                                        download_server.sin_family = AF_INET;
                                        download_server.sin_addr.s_addr = INADDR_ANY;
                                        download_server.sin_port = htons(dl_server_port);
                                        if (connect(dl_sockfd ,(struct sockaddr *)&download_server,sizeof(download_server)) < 0)
                                            cout("ERROR | connecting\n");
                                        if((send(dl_sockfd,buf, nbytes,0)) == -1)
                                            cout("ERROR | could not connect to Download server\n");
                                        cout("INFO | socket created for download...\n");
                                        if(dl_sockfd>fdmax)
                                            fdmax=dl_sockfd;
                                        FD_SET(dl_sockfd,&master);
                                        if(send(dl_sockfd,buf,strlen(buf),0) == -1){
                                            cout("ERROR | problem in send buffer to Download server!\n");
                                        }
                                        cout("INFO | starting download...\n");
                                    }

                                    if(buf[0]=='L' || buf[0]=='R'){
                                        if((send(lookup_sockfd,buf, nbytes,0)) == -1)
                                            cout("ERROR | could not connect to Lookup server\n");
                                    }
                                    if(buf[0]=='D' && buf[1]=='i' && buf[2]=='s'){
                                        char msg[512];
                                        sprintf(msg,"Disconnect %d",my_portno);
                                        write(lookup_sockfd,msg,strlen(msg));
                                        close(lookup_sockfd);
                                        cout("INFO | closed socket to Lookup server\n");
                                        exit(0);
                                    }
                                }

                                else if(j==lookup_sockfd){
                                    cout(buf);
                                    cout("\n");
                                }
                                else if(j==dl_sockfd){
                                    int write_fd = open (get_filename(fname), O_WRONLY | O_CREAT | O_APPEND,0777);
                                    if((write(write_fd,buf,nbytes)<=0)){
                                        cout("ERROR | problem in writing downloaded file!\n");
                                    }
                                    close(write_fd);
                                }

                                else{
                                    if(buf[0]=='D'){
                                            int fd = open(get_filename(buf), O_RDONLY);
                                            if (fd == -1) {
                                              cout("ERROR | problem opening file\n");
                                              exit(1);
                                            }

                                            /* get the size of the file to be sent */
                                            struct stat stat_buf;
                                            fstat(fd, &stat_buf);

                                            int rc;
                                            off_t offset = 0;

                                            int sector_size = proper_sector_size(stat_buf.st_size);
                                            //int sector_size = 1024;
                                            cout("INFO | sending file to client...\n");
                                            while (offset < stat_buf.st_size) {

                                              size_t count;
                                              off_t remaining = stat_buf.st_size- offset;
                                              if (remaining > sector_size)
                                                  count = sector_size;
                                              else
                                                  count = remaining;
                                              rc = sendfile(j, fd, &offset, count);
                                              if (rc == 0) {
                                                 break;
                                              }
                                              if (rc == -1) {
                                                 exit(1);
                                              }
                                            }

                                            if (offset != stat_buf.st_size) {
                                              cout("ERROR | data transfer problem!\n");
                                              exit(1);
                                            }
                                            cout("INFO | sent file sucessfully!\n");

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
