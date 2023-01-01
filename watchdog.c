#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

#define WATCHDOG_PORT 3000

int main(int argc, char *argv[]){
    int socket_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_server == -1)
    {
        printf("Could not create socket\n");
        return 1;
    }
    int enableReuse = 1;
    int ret = setsockopt(socket_server, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(int));
    if (ret < 0)
    {
        printf("setsockopt() failed with error code : %d\n", errno);
        return 1;
    }
    // creating the struct with the name and the address of the server
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));

    //saving the ipv4 type and the server port using htons to convert the server port to network's order
    server_address.sin_family = AF_INET;
    //allow any IP at this port to address the server
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(WATCHDOG_PORT);

    // Bind the socket to the port with any IP at this port
    int bind_res = bind(socket_server, (struct sockaddr *)&server_address, sizeof(server_address));
    if (bind_res == -1)
    {
        printf("Bind failed with error code : %d\n", errno);
        close(socket_server);
        return -1;
    }

    // Make the socket listen.
    int listen_value = listen(socket_server, 3);
    if (listen_value == -1)
    {
        printf("listen() failed with error code : %d\n", errno);
        close(socket_server);
        return -1;
    }
    else
        printf("Waiting for incoming TCP-connections...\n");

    // Accept and incoming connection
    struct sockaddr_in clientAddress;
    memset(&clientAddress, 0, sizeof(clientAddress));
    socklen_t len_clientAddress = sizeof(clientAddress);
    int clientSocket = accept(socket_server, (struct sockaddr *)&clientAddress, &len_clientAddress);
    if (clientSocket == -1)
    {
        printf("listen failed with error code : %d\n", errno);
        close(socket_server);
        return -1;
    }
    else
        printf("Connection accepted\n");

    // Change the sockets into non-blocking state 
    fcntl(socket_server, F_SETFL, O_NONBLOCK);   // Change the socket into non-blocking state
    fcntl(clientSocket, F_SETFL, O_NONBLOCK); // Change the socket into non-blocking state
    // this is insted setsockopt

    // for calculate the time
    struct timeval start, end;
    float seconds = 0;
    sleep(1);

    //checking if watchdog receive something from ping if yes 
    //then update the start time if no then the time will stop updating and then second>10
    while (seconds <= 10)
    {
        int ping = 1 ;
        int rec = recv(clientSocket, &ping, sizeof(int), 0);
        if (rec > 0)
        {
            gettimeofday(&start, 0);
        }
        gettimeofday(&end, 0);
        seconds = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0f;
    }


    close(clientSocket);    // close the clientSocket
    close(socket_server);
     printf("server <%s> cannot be reached.\n", argv[1]); 
      //reboot the system
    kill(getppid(),SIGKILL);

    return 0;
}