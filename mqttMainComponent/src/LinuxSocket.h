/*
 * LinuxSocket Class :  Linux classic socket
 *
 * Nhon Chu - March 2016 
 *
 */
 
#ifndef LINUXSOCKET_H
#define LINUXSOCKET_H

#include "BaseSocket.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>
/**
TCP socket connection
*/
class LinuxSocket : public BaseSocket
{
    
public:
    /** TCP socket connection
    */
    LinuxSocket();

    ~LinuxSocket();
    
    /** Connects this TCP socket to the server
    \param host The host to connect to. It can either be an IP Address or a hostname that will be resolved with DNS.
    \param port The host's port to connect to.
    \return 0 on success, -1 on failure.
    */
    int connect(const char* host, const int port);

    /** Close the TCP socket
    */
    void close();
    
    /** Check if the socket is connected
    \return true if connected, false otherwise.
    */
    bool is_connected(void);
    
    /** Set blocking or non-blocking mode of the socket and a timeout on
        blocking socket operations
    \param blocking  true for blocking mode, false for non-blocking mode.
    \param timeout   timeout in ms [Default: (1500)ms].
    */
    void set_blocking(bool blocking, unsigned int timeout_ms=1500);

    /** Send data to the remote host.
    \param data The buffer to send to the host.
    \param length The length of the buffer to send.
    \return the number of written bytes on success (>=0) or -1 on failure
     */
    int send(const char* data, int length);
    
    /** Send all the data to the remote host.
    \param data The buffer to send to the host.
    \param length The length of the buffer to send.
    \return the number of written bytes on success (>=0) or -1 on failure
    */
    int send_all(const char* data, int length);
    
    /** Receive data from the remote host.
    \param data The buffer in which to store the data received from the host.
    \param length The maximum length of the buffer.
    \return the number of received bytes on success (>=0) or -1 on failure
     */
    int receive(char* data, int length);

    /** Receive data from the remote host.
    \param data The buffer in which to store the data received from the host.
    \param dataSize The maximum length of the buffer.
    \param searchPattern : the pattern to search for, reading will stop when pattern is found
    \return the number of received bytes on success (>=0) or -1 on failure
     */
    int receive(char* data, int dataSize, const char* searchPattern);


    

private:
    int     _sock_fd;
    int     _timeout_ms;

};

#endif
