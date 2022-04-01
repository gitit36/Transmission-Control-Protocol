# Transmission-Control-Protocol
Computer Networks (CS-UH 3012) - Spring 2022 / Professor Tomasch Pötsch


The project for this course contains two parts:
1. Simplified TCP sender/receiver
2. TCP Congestin Control
    
       
1) Simplified TCP sender/receiver

● Sending packets to the network based on a fixed sending window size (e.g. WND of 10
packets)

● Sending acknowledgments back from the receiver and handling what to do when
receiving ACKs at the sender

● A timeout mechanism to deal with packet loss and retransmission
   
  
2) TCP Congestin Control

● Slow-start

● Congestion avoidance

● Fast retransmit (no fast recovery)
   
      
The TCP implementation was run on a Mahimahi server.
