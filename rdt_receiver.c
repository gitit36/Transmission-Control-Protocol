/*
Professor Thomas Potsh
Authors: Sangjin Lee (sl5583) and Runay Fan (rf1888)
Project 1
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"
// An arbitrary window size for the receiver
#define window_size 100000

/*
 * You ar required to change the implementation to support
 * window size greater than one.
 * In the currenlt implemenetation window size is one, hence we have
 * onlyt one send and receive packet
 */
tcp_packet *recvpkt;
tcp_packet *sndpkt;

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp; // pointer to a file to write data in
    char buffer[MSS_SIZE];
    struct timeval tp;
   
    /*
     * check command line arguments
     */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr,
                sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /*
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time,  data size,  seqno  \n");

    clientlen = sizeof(clientaddr);

    // A receiver buffer: used calloc so that memory is allocatd
    // for the receive window and every space is initially filled with 0s.
    int* window = (int*) calloc(window_size, sizeof(int));

    int idx = 0; // index for window position
    int ack = 0; // counter to update the latest packet # to send
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
            error("EROR in recvfrom");
        }
        recvpkt = (tcp_packet *) buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        
       
        
        /*
         * sendto: ACK back to the client
         */
        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);
        
        // if this one is the last packet received
        if ( recvpkt->hdr.data_size == 0) { 
            VLOG(INFO, "Reached the end of file %100");
            fclose(fp);
            break;
        }
        
        // We will figure out the position of the pactet in the window by
        // dividing the seqno by the total size of the file
        int ackno = 0;
        if(recvpkt->hdr.seqno != 0){ackno =(recvpkt->hdr.seqno) / DATA_SIZE;}

        // Since we are applying a circular buffer for the window, 
        // We need to make sur that when the index exceeds the window size,
        // We come back around to the front of the buffer.
        int diff = (ackno - ack);
        while(diff >= window_size){diff -= window_size;}

        int jump = 0;
        if(window[(idx + diff)] == 0){
            window[idx + diff] = 1; // Once the packet is received we check true (=1) 
            fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
            fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
            // Until the end of the received/checked_true packets
            while (1){
                if (window[idx] == 0){break;}
                jump ++; // Add the interval to get the next expected seqno
                window[idx] = 0; // for the packets cheked true, reset to 0
                idx = (idx + 1) % window_size; // Next packet
            }
            ack += jump;
        }
        
        sndpkt = make_packet(0);
        sndpkt->hdr.ctr_flags = ACK;
        sndpkt->hdr.ackno = ack;
        sndpkt->hdr.time_stamp = recvpkt->hdr.time_stamp;
        
        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
                (struct sockaddr *) &clientaddr, clientlen) < 0) {
            error("ERROR in sendto");
        } 
    }

    return 0;
}