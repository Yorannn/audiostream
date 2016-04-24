#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "library.h"
#include "audio.h"

#define BUFSIZE 1024

int main() {

    int client_fd, err;
    struct sockaddr_in addr;

    char msg[64];
    int len=0;
    socklen_t flen;
    struct sockaddr_in from;

    struct timespec tim, tim2;
    tim.tv_sec = 0;
    tim.tv_nsec = 82000000L;

    int data_fd;
    int channels, sample_size, sample_rate;
    char *datafile;
    char buffer[BUFSIZE];
    int mono = 0;

    client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (client_fd<0) {
        return client_fd;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = bind(client_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    if (err<0) {
        return err;
    }

    flen = sizeof(struct sockaddr_in);

    while (1==1) {

        len = recvfrom(client_fd, msg, sizeof(msg), 0, (struct sockaddr*) &from, &flen);

        if (len<0) {
            return len;
        }

        datafile = strdup(msg);

        data_fd = aud_readinit(datafile, &sample_rate, &sample_size, &channels);
        if (data_fd < 0){
            printf("failed to open datafile %s, skipping request\n",datafile);
            return -1;
        }
        printf("Got a request: Opened datafile %s\n",datafile);

        recvfrom(client_fd, msg, sizeof(msg), 0, (struct sockaddr*) &from, &flen);
        if (strcmp(msg,"sample_rate") == 0) {
            printf("Sent sample rate\n");
            sprintf(msg, "%d", sample_rate);
            sendto(client_fd, msg, strlen(msg)+1, 0, (struct sockaddr*) &from, flen);
        }

        recvfrom(client_fd, msg, sizeof(msg), 0, (struct sockaddr*) &from, &flen);
        if (strcmp(msg,"sample_size") == 0) {
            printf("Sent sample size\n");
            sprintf(msg, "%d", sample_size);
            sendto(client_fd, msg, strlen(msg)+1, 0, (struct sockaddr*) &from, flen);
        }

        recvfrom(client_fd, msg, sizeof(msg), 0, (struct sockaddr*) &from, &flen);
        if (strcmp(msg,"channels") == 0) {
            printf("Sent channels\n");
            sprintf(msg, "%d", channels);
            sendto(client_fd, msg, strlen(msg)+1, 0, (struct sockaddr*) &from, flen);
        }
        if (sample_size == 16) {
            tim.tv_nsec = sample_rate*110;
        } else {
            tim.tv_nsec = sample_rate*8000;
        }

        recvfrom(client_fd, msg, sizeof(msg), 0, (struct sockaddr*) &from, &flen);
        if (strcmp(msg,"mono") == 0) {
            mono = 1;
            recvfrom(client_fd, msg, sizeof(msg), 0, (struct sockaddr*) &from, &flen);
        }
        if (strcmp(msg,"start") == 0) {
            printf("Starting stream\n");
            int bytesread;
            int i;
            char modbuffer[BUFSIZE/2];

            bytesread = read(data_fd, buffer, BUFSIZE);
            while (bytesread > 0){
                if (mono == 0) {
                    err = sendto(client_fd, buffer, BUFSIZE, 0, (struct sockaddr*) &from, flen);
                } else {
                    for (i = 0; i<BUFSIZE/2;i++) {
                        modbuffer[i] = buffer[i*2-1];
                    }

                    err = sendto(client_fd, modbuffer, BUFSIZE/2+1, 0, (struct sockaddr*) &from, flen);
                }
                bytesread = read(data_fd, buffer, BUFSIZE);
                nanosleep(&tim,&tim2);
            }
            sendto(client_fd, "end_of_stream", strlen("end_of_stream")+1, 0, (struct sockaddr*) &from, flen);
            mono = 0;
            printf("Done Streaming. Waiting for new client request.\n");
        } else {
            printf("Expected start. Waiting for new client request.\n");
        }

        if (err<0) {
            return err;
        }


    }


    return 0;
}
