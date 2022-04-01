#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
int RETRY = 120; //millisecond
float estimated_RTT = 10;
float sample_RTT = 50;
float dev_RTT = 0;
float alpha = 0.125;
float beta = 0.25;

int next_seqno = 0; // used to indicate from which packet sender should send now
int send_base = 0;
double window_size = 1;
int ssthreash = 64;
int recvdACK = -1;
int dupCnt = 0; // number of duplicate ACKs received 
bool slowstart = true;

int sockfd, serverlen, len;
long byteCnt; // number of bytes of the file to be transfered
long pktCnt; // number of packets of the file to be transfered
struct sockaddr_in serveraddr;
struct itimerval timer; 
struct timeval begin_time, end_time;
tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;  
FILE *fp; 
char buffer[DATA_SIZE];    

void start_timer() {
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}

void stop_timer() {
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}

void send_packets(int starting_pkt, int ending_pkt) {
    // error checking
    if (starting_pkt > ending_pkt) {
        return;
    }

    if (ending_pkt >= pktCnt) {
        ending_pkt = pktCnt - 1;
    }

    int curr_pkt = starting_pkt;
    fseek(fp, starting_pkt * DATA_SIZE, SEEK_SET);
    while (curr_pkt <= ending_pkt) {
        
        char buffer[DATA_SIZE]; 
        len = fread(buffer, 1, DATA_SIZE, fp);
        tcp_packet *sndpkt = make_packet(len);
        memcpy(sndpkt->data, buffer, len);
        sndpkt->hdr.seqno = curr_pkt * DATA_SIZE;
        gettimeofday(&begin_time, NULL);
        sndpkt->hdr.time_stamp = (begin_time.tv_sec * 1000LL + (begin_time.tv_usec/1000));
        VLOG(DEBUG, "Sending packet %d to %s, sendbase: %d, nextseqno: %d, starting pkt: %d, ending pkt: %d, window size: %f", 
                send_base, inet_ntoa(serveraddr.sin_addr), send_base, next_seqno, starting_pkt, ending_pkt, window_size);

        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                    ( const struct sockaddr *)&serveraddr, serverlen) < 0) {
            error("sendto");
        }
        next_seqno = ((next_seqno > curr_pkt + 1) ? next_seqno : curr_pkt + 1);
        curr_pkt++;
    }
}

void resend_packets(int sig) {
    if (sig == SIGALRM) {
        //Resend all packets range between 
        //sendBase and nextSeqNum
        VLOG(INFO, "Timout happend");
        slowstart = true;
        window_size = 1;
        ssthreash = (window_size / 2 > 2) ? (window_size / 2) : 2;
        send_packets(send_base, next_seqno - 1);
        start_timer();
    }
}


/*
 * init_timer: Initialize timer
 * delay: delay in milliseconds
 * sig_handler: signal handler function for re-sending unACKed packets
 */
void init_timer(int delay, void (*sig_handler)(int)) 
{
    signal(SIGALRM, resend_packets);
    timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;  
    timer.it_value.tv_sec = delay / 1000;       // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

void update_timer(tcp_packet *pkt) {
    gettimeofday(&end_time, NULL);
    sample_RTT = (float)((end_time.tv_sec * 1000LL + (end_time.tv_usec/1000)) - pkt->hdr.time_stamp);

    estimated_RTT = ((1.0 - alpha) * estimated_RTT) + (alpha * sample_RTT);
    dev_RTT = ((1.0 - beta) * dev_RTT) + (beta * fabs(sample_RTT - estimated_RTT));

    RETRY = (int)(estimated_RTT + (4 * dev_RTT));
}

int main (int argc, char **argv)
{
    int portno;
    char *hostname;

    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "r");
    if (fp == NULL) {
        error(argv[3]);
    }

    FILE *plot;
    plot = fopen("CWND.csv", "w");

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");


    /* initialize server server details */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
        fprintf(stderr,"ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    // get total number of packets needed to transfer the file
    fseek(fp, 0, SEEK_END);
    byteCnt = ftell(fp);
    pktCnt = (byteCnt + DATA_SIZE) / DATA_SIZE;
    printf("The byte count is %ld, the packet count is %ld\n", byteCnt, pktCnt);
    fseek(fp, 0, SEEK_SET);

    init_timer(RETRY, resend_packets);

    //send out the first packet
    send_packets(0, 0);

    clock_t initial_time;
    initial_time = clock();

    start_timer();

    int whileCnt = 0;

    while (1) {
        whileCnt++;
        clock_t curr_time = clock();
        double time_taken = ((double)curr_time - initial_time)/CLOCKS_PER_SEC;
        fprintf(plot, "%f, %d, %d\n", time_taken, (int)(window_size), whileCnt);
        // get ACK from the receiver
        if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
                    (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0) {
            error("recvfrom");
        }

        recvpkt = (tcp_packet *)buffer;
        printf("%d \n", get_data_size(recvpkt));
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        recvdACK = recvpkt->hdr.ackno;
        update_timer(recvpkt);
        // no duplicate ACK
        if (recvdACK > send_base) {
            stop_timer();
            dupCnt = 0;

            // handle reaching EOF
            if (recvdACK >= pktCnt) {
                sndpkt = make_packet(0);
                if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                            ( const struct sockaddr *)&serveraddr, serverlen) < 0) {
                    error("sendto");
                }
                return 0;
            }

            // increment window size based on slow start / congestion avoidance 
            if (slowstart) {
                window_size += (recvdACK - send_base);
                if (window_size >= ssthreash) {
                    slowstart = false;
                }
            } else {
                window_size += (recvdACK - send_base) / window_size;
            }

            // update window and send out new packets
            send_base = recvdACK;
            next_seqno = next_seqno > recvdACK ? next_seqno : recvdACK;
            send_packets(next_seqno, send_base + (int)window_size - 1);
            start_timer();
        } else { 
            if (recvdACK >= pktCnt) {
                sndpkt = make_packet(0);
                if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                            ( const struct sockaddr *)&serveraddr, serverlen) < 0) {
                    error("sendto");
                }
                return 0;
            }
            // duplicate ACK
            dupCnt++;

            if (dupCnt == 3) {
                dupCnt = 0;
                slowstart = true;
                window_size = 1;
                ssthreash = (window_size / 2 > 2) ? (window_size / 2) : 2;
                // resend packets under fast retransmit mechanism
                send_packets(recvdACK, send_base + (int)window_size - 1);
                
                start_timer();
            }
        }

        free(sndpkt);
    }

    return 0;

}



