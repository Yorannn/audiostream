#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "library.h"
#include "audio.h"

#define BUFSIZE 1024

int main(int argc, char **argv)
{
    if (argc<3) {
        printf("Usage: client <host_name> <wav_file> \n\n");
        return 1;
    }
    int server_fd, audio_fd;
    char *msg = argv[2];
    int err;
    int len = 0;
    socklen_t flen;
    struct sockaddr_in dest;
    server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct hostent *resolv;
    struct in_addr *ip_addr;
    int sample_size, sample_rate, channels;
    char buffer[BUFSIZE];
    int init = 0;
    int lowervolume = 0;
    int mono = 0;

    sample_size = 4;
    sample_rate = 44100;
    channels = 2;

    resolv = gethostbyname(argv[1]);

    if (resolv==NULL) {
        printf("Address not found for %s\n",argv[1]);
        return -1;
    }
    else {
        ip_addr = (struct in_addr*) resolv->h_addr_list[0];
    }

    if (server_fd<0) {
        return server_fd;
    }

    flen = sizeof(struct sockaddr_in);
    dest.sin_family = AF_INET;
    dest.sin_port = htons(1234);
    dest.sin_addr.s_addr = inet_addr(inet_ntoa(*ip_addr));

    err = sendto(server_fd, msg, strlen(msg)+1, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));

    if (err<0) {
        return err;
    }


    sendto(server_fd, "sample_rate", strlen("sample_rate")+1, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
    if (recvfrom(server_fd, msg, sizeof(msg), 0, (struct sockaddr*) &dest, &flen) > 0) {
        sample_rate = atoi(msg);
        printf("Sample Rate: %d\n", atoi(msg));
        init++;
    }
    sendto(server_fd, "sample_size", strlen("sample_size")+1, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
    if (recvfrom(server_fd, msg, sizeof(msg), 0, (struct sockaddr*) &dest, &flen) > 0) {
        sample_size = atoi(msg);
        printf("Sample Size: %d\n", atoi(msg));
        init++;
    }
    sendto(server_fd, "channels", strlen("channels")+1, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
    if (recvfrom(server_fd, msg, sizeof(msg), 0, (struct sockaddr*) &dest, &flen) > 0) {
        channels = atoi(msg);
        printf("Channels: %d\n", atoi(msg));
        init++;
    }
    if (init == 3) {
        if (argv[3] && (strcmp(argv[3],"higherpitch") == 0)) {
            printf("Raising pitch\n");
            sample_rate = sample_rate*1.1;
        }
        if (argv[3] && (strcmp(argv[3],"mono") == 0)) {
            if (channels != 1) {
                printf("Applying mono filter\n");
                channels = 1;
                mono = 1;
                sendto(server_fd, "mono", strlen("mono")+1, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
            } else {
                printf("File is mono already. Skipping filter request.\n");
            }
        }
        if (argv[3] && (strcmp(argv[3],"lowervolume") == 0)) {
            printf("Lowering Volume\n");
            lowervolume = 1;
        }
        audio_fd = aud_writeinit(sample_rate, sample_size, channels);

        if (audio_fd < 0){
            printf("error: unable to open audio output.\n");
            return -1;
        }
        sendto(server_fd, "start", strlen("start")+1, 0, (struct sockaddr*) &dest, sizeof(struct sockaddr_in));
        printf("Starting the stream\n");
    } else {
        printf("Something went wrong initialising.\n");
        return -1;
    }

    if (len<0) {
        return len;
    }

    int bytesread;

    bytesread = read(server_fd, buffer, BUFSIZE);
    write(audio_fd, buffer, BUFSIZE);

    int i;


    while (bytesread > 0){
        if (strcmp(buffer,"end_of_stream") != 0) {
            if (lowervolume == 1) {
                for (i = 0; i<BUFSIZE+1; i++) {
                    buffer[i] = buffer[i]/4;
                }
            }

            if (mono != 1) {
                write(audio_fd, buffer, BUFSIZE);
                bytesread = read(server_fd, buffer, BUFSIZE);
            } else {
                write(audio_fd, buffer, BUFSIZE/2);
                bytesread = read(server_fd, buffer, BUFSIZE/2);
            }
        } else {
            sleep(1);
            printf("Stream ended\n");
            return 0;
        }
    }

    return 0;
}
