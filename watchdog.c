#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#define PORT 3000
#define INTERVAL 10 // timer interval in seconds

int main(int argc, char* argv[]) {
  // Set up TCP connection
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
  listen(sockfd, 5);
  int clientfd = accept(sockfd, NULL, NULL);

  // Set up timer
  timer_t timerid;
  struct sigevent sev;
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGALRM;
  sev.sigev_value.sival_ptr = &timerid;
  timer_create(CLOCK_REALTIME, &sev, &timerid);

  struct itimerspec its;
  its.it_value.tv_sec = INTERVAL;
  its.it_value.tv_nsec = 0;
  its.it_interval.tv_sec = INTERVAL;
  its.it_interval.tv_nsec = 0;
  timer_settime(timerid, 0, &its, NULL);

  // Wait for timer expirations and check for ICMP replies
  while (1) {
    pause(); // wait for timer expiration
    char reply;
    read(clientfd, &reply, sizeof(reply)); // check for ICMP reply
    if (reply == 0) { // no reply received
      printf("server %s cannot be reached\n", argv[1]);
      close(clientfd);
      close(sockfd);
      exit(0);
    }
  }

  return 0;
}