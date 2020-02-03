#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include<pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_LEN 1024


int client;
int sock_sctp;


void * downlink_thread()
{
    char buffer[BUFFER_LEN];
    int len;
    printf("Downlink Thread: Started correctly\n");
    /* Forward all data from the SCTP server to the TCP client */
    while(1)
    {
        bzero(buffer, BUFFER_LEN);
        len = (int)recv(sock_sctp, buffer, BUFFER_LEN, 0);
        printf("Downlink packet forwarded\n");
        send(client, buffer, len, 0);
    }
}

void * uplink_thread()
{
    printf("Uplink Thread: Started correctly\n");
    char buffer[BUFFER_LEN];
    int len;
    /* Forward all data from the TCP client to the SCTP server */
    while(1)
    {
        bzero(buffer, BUFFER_LEN);
        len = (int)recv(client, buffer, BUFFER_LEN, 0);
        printf("Uplink packet forwarded\n");
        send(sock_sctp, buffer, len, 0);
    }
}



int main(int argc, char const *argv[])
{
    int sock, addrlen, bytes_readed, i;
    const char * tunnel_ip, * sctp_server_ip;
    int tunnel_port, sctp_server_port;
    pthread_t downlink_id, uplink_id;
    struct sctp_event_subscribe events;
    struct sockaddr_in my_addr, remote_addr;
    struct sockaddr_in sctp_server_addr;



    if(argc != 5)
    {
        printf("USE: ./server <TUNNEL_IP> <TUNNEL_PORT> <SCTP_SERVER_IP> <SCTP_SERVER_PORT>\n");
        exit(1);
    }
    /* Getting parameters */
    tunnel_ip = argv[1];
    tunnel_port = atoi(argv[2]);
    sctp_server_ip = argv[3];
    sctp_server_port = atoi(argv[4]);


    /*
    * Tunnel Socket Set Up
    */
    /* Creating TCP socket*/ 
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1)
    {
        perror("socket");
        exit(1);
    }

    /* Set up tunnel parameters */
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr(tunnel_ip);
    my_addr.sin_port = htons(tunnel_port);
    memset(&(my_addr.sin_zero), 0, 8);
    
    /*Binding*/
    if(bind(sock, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        close(sock);
        exit(1);
    }
    /* Listening */
    if(listen(sock, 5) == -1)
    {
        perror("listen");
        close(sock);
        exit(1);
    }

    addrlen = sizeof(struct sockaddr);
    client = accept(sock, (struct sockaddr *)&remote_addr, &addrlen);
    printf("OK: Tunnel client connected\n");
    /* TCP Tunnel established */


    /*
    * SCTP Connection with the SCTP Server
    */

    sock_sctp = socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (sock_sctp < 0)
    {
        perror("sctp socket error");
        close(client);
        close(sock);
        exit(1);
    }

    /* Sets the data_io_event to be able to use sendrecv_info */
    /* Subscribes to the SCTP_SHUTDOWN event, to handle graceful shutdown */
    bzero(&events, sizeof(events));
    events.sctp_data_io_event          = 1;
    events.sctp_shutdown_event         = 1;
    if (setsockopt(sock_sctp, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) != 0) {
        perror("setsockopt SCTP error");
        close(client);
        close(sock);
        exit(1);
    }

    /* Set up SCTP Server address */
    memset(&sctp_server_addr, 0, sizeof(sctp_server_addr));
    sctp_server_addr.sin_family = AF_INET;
    sctp_server_addr.sin_addr.s_addr = inet_addr(sctp_server_ip);
    sctp_server_addr.sin_port = htons(sctp_server_port);

    /* Connect to the SCTP server */
    if (connect(sock_sctp, (struct sockaddr*)&sctp_server_addr, sizeof(sctp_server_addr)))
    {
        perror("sctp connect error");
        close(client);
        close(sock);
        exit(1);
    }
    printf("OK: Connected to the SCTP Server\n");
    /* SCTP Connection established */


    /* Init both threads to handle downlink and uplink messages */
    pthread_create(&downlink_id, NULL, &downlink_thread, NULL);
    pthread_create(&uplink_id, NULL, &uplink_thread, NULL);

    pthread_join(downlink_id, NULL);
    pthread_join(uplink_id, NULL);

    return 0;
}