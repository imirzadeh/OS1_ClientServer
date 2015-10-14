// TODO :
// server port is not dyanmic
// server ip is not dynamic


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* port we're listening on */
#define PORT 2020
#define MAXCLIENTS 10

struct client{
        int serve_port;
        int file_count;
        int load;
        char files[10][25];
};

void init_clients(struct client(*all_clients)[MAXCLIENTS]){
    int i,j;
    for(i=0; i<MAXCLIENTS; i++){
        (*all_clients)[i].serve_port = -1;
        (*all_clients)[i].file_count = 0;
        (*all_clients)[i].load = 0;
        for(j=0;j<10;j++){
            (*all_clients)[i].files[j][0]='-';
        }
    }
}

bool exist_client(int client_port_no,struct client(*all_clients)[MAXCLIENTS]){
    int i;
    for(i=0;i<MAXCLIENTS;i++){
        if((*all_clients)[i].serve_port == client_port_no){
            return true;
        }
    }
    return false;
}

void create_client(int client_port_no,struct client(*all_clients)[MAXCLIENTS]){
    int i;
    if (exist_client(client_port_no,&(*all_clients))){
        return;
    }
    for(i=0;i<MAXCLIENTS;i++){
        if ((*all_clients)[i].serve_port == -1){
            (*all_clients)[i].serve_port = client_port_no;
            break;
        }
    }
}

void inc_load(int client_port_no,struct client(*all_clients)[MAXCLIENTS]){
    int i;
    for (i=0;i<MAXCLIENTS;i++){
        if((*all_clients)[i].serve_port == client_port_no){
            (*all_clients)[i].load += 1;
        }
    }
}

int disconnect_client(int client_port_no,struct client(*all_clients)[MAXCLIENTS]){
    int i,j;
    bool found = exist_client(client_port_no,&(*all_clients));
    if(!found){
        return 0;
    }
    for(i=0;i<MAXCLIENTS;i++){
        if( (*all_clients)[i].serve_port == client_port_no ) {
            (*all_clients)[i].serve_port = -1;
            (*all_clients)[i].file_count = 0;
            (*all_clients)[i].load = 0;
            for(j=0;j<10;j++){
                (*all_clients)[i].files[j][0]='-';
            }
            return 1;
        }
    }
    return 0;
}

void add_file_to_client(int client_port_no,char* file_name,struct client(*all_clients)[MAXCLIENTS]){
    int i;
    bool found = exist_client(client_port_no,&(*all_clients));
    if(!found){
        create_client(client_port_no,&(*all_clients));
    }
    for(i=0;i<MAXCLIENTS;i++){
        if( (*all_clients)[i].serve_port == client_port_no ) {
                    strcpy((*all_clients)[i].files[(*all_clients)[i].file_count],file_name);
                    (*all_clients)[i].file_count += 1;
                    break;
        }
    }
}


int find_best_server(char* filename,struct client(*all_clients)[MAXCLIENTS]){
    int min_load = 100;
    int best_server = -1;
    int i,j;
    for(i=0;i<MAXCLIENTS;i++){
        for(j=0;j<10;j++){
            if(strcmp((*all_clients)[i].files[j],filename) == 0){
                if((*all_clients)[i].load < min_load){
                    min_load = (*all_clients)[i].load;
                    best_server =(*all_clients)[i].serve_port;
                }
            }
        }
    }
    return best_server;
}


void cout(char* msg){
    write(STDIN_FILENO,msg,strlen(msg));
}

char* response(char* buf,struct client(*all_clients)[MAXCLIENTS]){
    int end=0;
    for(end=0;end<(strlen(buf));end++){
        if (buf[end]=='\0' || buf[end]=='\n')
            break;
    }

    char answer[1024]={0};
    //Lookup
    char filename[1024]={0};
    char cmd[1024]={0};
    char ip[1024]={0};
    int i,port;
    if(buf[0] == 'L'){
        sscanf(buf,"%s %s %s %d",cmd,filename,ip,&port);
        char tmpmsg[512]={0};
        sprintf(tmpmsg,"INFO | a client is looking for %s\n",filename);
        cout(tmpmsg);
        int res = find_best_server(filename,&(*all_clients));
        if(res == -1){
            sprintf(answer,"SERVER RESP | requested file not found on any client!\0");
        }
        else{
            inc_load(res,&(*all_clients));
            sprintf(answer,"SERVER RESP | file found on 127.0.0.1:%d!\0",res);
        }
        //return res;
    }

    // Register
    else if(buf[0]=='R'){
        sscanf(buf,"%s %s %s %d",cmd,filename,ip,&port);
        char tmpmsg[512]={0};
        sprintf(tmpmsg,"INFO | %s:%d registred %s\n",ip,port,filename);
        cout(tmpmsg);
        add_file_to_client(port,filename,&(*all_clients));
        sprintf(answer,"SERVER RESP | successfully registered your file!\0");
    }
    else if(buf[0]=='D'){
        int port_no = (buf[11]-'0')*10000 + (buf[12]-'0')*1000 + (buf[13]-'0')*100+(buf[14]-'0')*10+(buf[15]-'0');
        disconnect_client(port_no,&(*all_clients));
    }
    else{
        sprintf(answer,"SERVER RESP | meaningless command!\0");
    }
    return (char*)answer;
}


int main(int argc, char *argv[]) {

    struct client all_clients[MAXCLIENTS];
    init_clients(&all_clients);

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
        cout("ERROR | server socket() problem\n");
        /*just exit lol!*/
        exit(1);
    }
    cout("INFO | server socket created successfully!\n");
    /*"address already in use" error message */
    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        cout("ERROR | listen socket problem!\n");
        exit(1);
    }

    cout("INFO | socket set successfully\n");

    /* bind */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(PORT);
    memset(&(serveraddr.sin_zero), '\0', 8);

    if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
        cout("ERROR | server bind problem!\n");
        exit(1);
    }
    cout("INFO | binded successfully\n");
    /* listen */
    if(listen(listener, 10) == -1) {
        cout("ERROR | listen problem\n");
        exit(1);
    }

    cout("INFO | listen socket set sucessfully\n");

    /* add the listener to the master set */
    FD_SET(listener, &master);
    //FD_SET(STDIN_FILENO,&master);
    /* keep track of the biggest file descriptor */
    fdmax = listener; /* so far, it's this one*/
    /* loop */
    for(;;) {
        /* copy it */
        read_fds = master;

        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            cout("ERROR | select() method\n");
            exit(1);
        }

        /*run through the existing connections looking for data to be read*/
        for(i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &read_fds)) { /* we got one... */
                if(i == listener) {
                    /* handle new connections */
                    addrlen = sizeof(clientaddr);
                    if((newfd = accept(listener, (struct sockaddr *)&clientaddr, &addrlen)) == -1) {
                        cout("ERROR | server-accept() function problem!\n");
                    }
                    else {
                        FD_SET(newfd, &master); /* add to master set */
                        if(newfd > fdmax) { /* keep track of the maximum */
                            fdmax = newfd;
                        }
                        char tmp[1024];
                        sprintf(tmp,"INFO | New connection from %s on socket %d\n", inet_ntoa(clientaddr.sin_addr), newfd);
                        cout(tmp);
                    }
                }
                else {
                    if((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
                        if(nbytes == 0)
                            cout("INFO | a client disconnected!\n");

                        else{
                            cout("ERROR | recv() problem\n");
                        }
                        close(i);
                        FD_CLR(i, &master);
                    }
                    else {
                        /* we got some data from a client*/
                        for(j = 0; j <= fdmax; j++) {
                            /* send to everyone! */
                            if(FD_ISSET(j, &master)) {
                                /* except the listener and ourselves */
                                if(j != listener && j == i) {
                                    char resp[1024]={0};
                                    sprintf(resp,"%s",response(buf,&all_clients));

                                    int end;
                                    int len=strlen(resp);
                                    for(end=0;end<len;end++){
                                        if(resp[end]=='\0' || resp[end]=='!')
                                            break;
                                    }
                                    for(end=end;end<len;end++){
                                        resp[end]='\0';
                                    }
                                    if((send(j,resp,end, 0)) == -1)
                                        cout("ERROR | send() error\n");
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
