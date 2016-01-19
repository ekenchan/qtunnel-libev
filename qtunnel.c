#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/evp.h>
#include "qtunnel.h"

struct struct_options options;
struct struct_setting setting;

int serv_sock;
struct sockaddr_in serv_adr;
//int clnt_adr_size;
int *cofds;
int *lofds;
int *isused;
int colen;
//struct sockaddr_in *clnt_adr;
//struct sockaddr_in *remote_adr;
RC4_KEY *cokey;
RC4_KEY *lokey;
int maxfd;
byte** coinput;
byte** cooutput;
const int BUFSIZE = 4096;

int main(int argc, char *argv[]){
    int i;
    colen = 20;
    maxfd = -1;
    cofds = (int *)malloc(sizeof(int) * colen);
    lofds = (int *)malloc(sizeof(int) * colen);
    isused = (int *)malloc(sizeof(int) * colen);
    //clnt_adr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in) * colen);
    //remote_adr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in) * colen);
    coinput = (byte **)malloc(sizeof(byte*) * colen);
    cooutput = (byte **)malloc(sizeof(byte*) * colen);
    cokey = (RC4_KEY *)malloc(sizeof(RC4_KEY) * colen);
    lokey = (RC4_KEY *)malloc(sizeof(RC4_KEY) * colen);

    //clear
    for(i = 0; i < colen; ++i) {
        isused[i] = 0;
        coinput[i] = (byte*)malloc(sizeof(byte) * BUFSIZE);
        cooutput[i] = (byte*)malloc(sizeof(byte) * BUFSIZE);
    }


    //malloc buf



    get_param(argc, argv);

//    options.faddr = "0.0.0.0:8765";
//    options.baddr = "";
//    options.cryptoMethod = "RC4";
//    options.secret = "secret";
//    options.clientMod = 1;

    build_server();

    while(1) {
        fdset io;
        FD_ZERO(&io);
        FD_SET(serv_sock, &io);
        for(i = 0; i < colen; ++i) {
            if(isused[i] != 0) {
                FD_SET(cofds[i], &io);
                FD_sET(lofds[i], &io);
            }
        }
        if ( select(maxfd+1, &io, NULL, NULL, NULL) < 0) {
            perror("select error!");
            exit(1);
        }
        if(FD_ISSET(serv_sock, &io)) {
            handle_accept();
        }

        for(i = 0; i < colen; ++i) {
            if(isused[i] != 0) {
                handle_local(i);
                handle_remote(i);
            }
        }

        int clnt_adr_size;
        clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_adr, &clnt_adr_size);
        if(fork() == 0) {
            close(serv_sock);
            handle_client(clnt_sock);
            exit(0);
        }
        close(clnt_sock);
    }

    return 0;
}

void handle_local(int pos) {

}

void handle_remote(int pod) {
    
}

void handle_accept() {
    int nfd, i, remote_sock;
    int clnt_adr_size;
    struct sockaddr_in addr, remote_adr;
    nfd = accept(serv_sock, (struct sockaddr*) &addr, &clnt_adr_size);

    if(nfd > maxfd) maxfd = nfd;

    int pos = -1;

    for(i = 0; i < colen; ++i) {
        if(isused[i] == 0) pos = i;
    }

    if(pos != -1) {
        isused[pos] = 1;
        cofds[pos] = nfd;
        memset(&remote_adr, 0, sizeof(remote_adr));
        remote_adr.sin_family = AF_INET;
        remote_adr.sin_port = htons(atoi(setting.baddr_port));
        remote_adr.sin_addr.s_addr = inet_addr(setting.baddr_host);

        remote_sock = socket(PF_INET, SOCK_STREAM, 0);
        lofds[pos] = remote_sock;

        if(remote_sock > maxfd) maxfd = remote_sock;

        if(remote_sock < 0) {
            perror("socket error");
            isused[pos] = 0;
            return ;
        }
        if ( connect(remote_sock, (struct sockaddr *) &remote_adr, sizeof(remote_adr)) < 0) {
            perror("connect remote error");
            exit(1);
        }

        RC4_set_key(&cokey[pos], 16, setting.secret);
        RC4_set_key(&lokey[pos], 16, setting.secret);
    } else {
        perror("out of memory!");
        exit(1);
    }
}

void get_param(int argc, char *argv[]) {
    char c;
    unsigned long p;
    while((c = getopt_long (argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch(c) {
            case 'h': {
                print_usage();
                exit(0);
            }
            case 'b': {
                options.baddr = optarg;
                p = strchr(optarg, ':') - optarg;
                strncpy(setting.baddr_host, optarg, p);
                strcpy(setting.baddr_port, optarg + p + 1);
                //printf("badd = %s  %s\n", setting.baddr_port, setting.baddr_host);
                break;
            }
            case 'l': {
                options.faddr = optarg;
                p = strchr(optarg, ':') - optarg;
                strcpy(setting.faddr_port, optarg + p + 1);
                //printf("fadd = %s\n", setting.faddr_port);
                break;
            }
            case 'c': {
                options.clientMod = optarg;
                if(strcmp(optarg, "true") == 0) {
                    setting.clientMod = CLIENTMOD;
                } else {
                    setting.clientMod = SERVERMOD;
                }
                break;
            }
            case 's': {
                options.secret = optarg;
                strncpy(setting.secret, secretToKey(optarg, 16), 16);
                //setting.secret = secretToKey(optarg, 16);
                //printf("sec == %s\n", setting.secret);
                break;
            }
            default: {
                printf("unknow option of %c\n", optopt);
                break;
            }
        }
    }
    if(strcmp(setting.baddr_port, "") == 0) {
        perror("missing option --backend");
        exit(1);
    }
    if(strcmp(setting.baddr_host, "") == 0) {
        perror("missing option --backend");
        exit(1);
    }
    if(strcmp(setting.secret, "") == 0) {
        perror("mission option --secret");
        exit(1);
    }

    //printf("%s %s %s\n",setting.faddr_port, setting.baddr_port, setting.baddr_host);
}

void print_usage() {
    printf("Options:\n\
  --help\n\
  --backend=remotehost:remoteport    remote\n\
  --listen=localhost:localport   local\n\
  --clientmod=true or false  buffer size\n\
  --secret=secret secret\
\n");
}

byte* secretToKey(char* sec, int size) {
    byte buf[16];
    byte buf2[16];
    MD5_CTX h;
    MD5_Init(&h);
    int count = size / 16;
    int i,j;
    for(i = 0; i < count; ++i) {
        MD5_Update(&h, sec, strlen(sec));
        MD5_Final(buf2, &h);
        strncpy(buf, buf2, 15);
    }
    buf[15]=0;

    return buf;
}

int build_server() {
    memset(&serv_adr, 0, sizeof(serv_adr));

    serv_adr.sin_port = htons(atoi(setting.faddr_port));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    if(serv_sock > maxfd) {
        maxfd = serv_sock;
    }

    if(serv_sock < 0) {
        perror("socket error");
        exit(1);
    }

    int optval = 1;
    if(setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt error");
        exit(1);
    }

    if ( bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
        perror("bind error");
        exit(1);
    }

    if( listen(serv_sock,1) == -1 ) {
        perror("listen error");
        exit(1);
    }
}
