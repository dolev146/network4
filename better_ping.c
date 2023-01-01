#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>

#define IP4_HDRLEN 20
#define ICMP_HDRLEN 8
#define WATCHDOG_PORT 3000
#define SERVER_IP_ADDRESS "127.0.0.1"

unsigned short calculate_checksum(unsigned short *paddress, int len);
int main(int argc, char *argv[]);

// Compute checksum (RFC 1071).
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}


int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./ping <ip address>\n");
        exit(1);
    }

    char ip[INET_ADDRSTRLEN];               // space to hold the IPv4 string
    strcpy(ip, argv[1]);                    // get ip-address from command line
    struct in_addr addr;                    // IPv4 address
    if (inet_pton(AF_INET, ip, &addr) != 1) // convert IPv4 and IPv6 addresses from text to binary form
    {
        printf("Invalid ip-address\n");
        return 0;
    }
    char *args[2];
    // compiled watchdog.c by makefile
    args[0] = "./watchdog";
    args[1] = argv[1];
    int status;
    int pid = fork();
    if (pid == 0)
    {
        execvp(args[0], args);
    }
    //open a socket for watchdog
    int watchdogSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (watchdogSocket == -1) {
        printf("Could not create socket : %d\n", errno);
        return -1;
    } else printf("New socket opened\n");
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(WATCHDOG_PORT);
    int rval = inet_pton(AF_INET, (const char *) SERVER_IP_ADDRESS, &serverAddress.sin_addr);
    if (rval <= 0) {
        printf("inet_pton() failed");
        return -1;
    }

    sleep(3); // wait for watchdog to start

    // Make a connection with watchdog
    int connectResult = connect(watchdogSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    if (connectResult == -1) {
        printf("connect() failed with error code : %d\n", errno);
        close(watchdogSocket);
        return -1;
    } 
    // open a socket for ping


    int sockfd; // socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        fprintf(stderr, "socket() failed with error: %d", errno);
        fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
        exit(1);
    }

    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;
    dest_in.sin_addr.s_addr = inet_addr(argv[1]); // destination ip address
    //Change the socket into non-blocking state
    fcntl(watchdogSocket, F_SETFL, O_NONBLOCK); //`fcntl` is used to change the socket into non-blocking state

    // icmp packet
    struct icmp icmphdr; 
    char data[IP_MAXPACKET] = "This is the ping.\n";
    int datalen = strlen(data) + 1;
      //for calculate the time
    struct timeval start, end;
    int  icmp_seq_counter = 0;//seq counter
    printf("PING %s (%s): %d data bytes\n", argv[1], argv[1], datalen);
    while (true) {

        icmphdr.icmp_type = ICMP_ECHO;// Message Type (8 bits): ICMP_ECHO_REQUEST
        icmphdr.icmp_code = 0;// Message Code (8 bits): echo request
        icmphdr.icmp_id = 18;
        icmphdr.icmp_seq = 0;// Sequence Number (16 bits): starts at 0
        icmphdr.icmp_cksum = 0;// ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
        char packet[IP_MAXPACKET];// Combine the packet
        memcpy((packet), &icmphdr, ICMP_HDRLEN); // Next, ICMP header
        memcpy(packet + ICMP_HDRLEN, data, datalen);// After ICMP header, add the ICMP data.
        icmphdr.icmp_cksum = calculate_checksum((unsigned short *) (packet),
                                                ICMP_HDRLEN + datalen);// Calculate the ICMP header checksum
        memcpy((packet), &icmphdr, ICMP_HDRLEN);

      

        gettimeofday(&start, 0);
        
        // Send the packet using sendto() for sending datagrams.
        int bytes_sent = sendto(sockfd, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *) &dest_in, sizeof(dest_in));
        if (bytes_sent == -1) {
            fprintf(stderr, "sendto() failed with error: %d", errno);
            return -1;
        }


        // Get the ping response
        bzero(packet, IP_MAXPACKET);
        socklen_t len = sizeof(dest_in);
        ssize_t bytes_received = -1;
        struct iphdr *iphdr ;
        struct icmphdr *icmphdr ;
        
        while ((bytes_received = recvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&dest_in, &len)))
        {
            if (bytes_received > 0)
            {

                iphdr = (struct iphdr *)packet;
                icmphdr = (struct icmphdr *)(packet + (iphdr->ihl * 4));
                inet_ntop(AF_INET, &(iphdr->saddr), ip, INET_ADDRSTRLEN);
                break;
            }
        }
        gettimeofday(&end, 0);

        //calculate the time
        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%f ms)\n", bytes_received, argv[1], icmphdr->un.echo.sequence,iphdr->ttl,
               milliseconds);
       
        icmp_seq_counter++;
       

    }

    // Close the raw socket descriptor.
    close(sockfd);


    wait(&status);// waiting for child to finish before exiting
    return 0;
}

    
    

