
/*
 * BaseSocket Class :  abstract socket class
 *
 * Nhon Chu - March 2016 
 *
 */
 
#ifndef BASESOCKET_H
#define BASESOCKET_H

class BaseSocket
{
    
public:
    /** TCP socket connection
    */
    BaseSocket();

    virtual ~BaseSocket();
    
    /** Connects this TCP socket to the server
    \param host The host to connect to. It can either be an IP Address or a hostname that will be resolved with DNS.
    \param port The host's port to connect to.
    \return 0 on success, -1 on failure.
    */
    virtual int connect(const char* host, const int port) = 0;

    /** Close the TCP socket
    */
    virtual void close() = 0;
    
    /** Check if the socket is connected
    \return true if connected, false otherwise.
    */
    virtual bool is_connected(void) = 0;
    
    /** Set blocking or non-blocking mode of the socket and a timeout on
        blocking socket operations
    \param blocking  true for blocking mode, false for non-blocking mode.
    \param timeout   timeout in ms [Default: (1500)ms].
    */
    virtual void set_blocking(bool blocking, unsigned int timeout_ms=1500) = 0;

    /** Send data to the remote host.
    \param data The buffer to send to the host.
    \param length The length of the buffer to send.
    \return the number of written bytes on success (>=0) or -1 on failure
     */
    virtual int send(const char* data, int length) = 0;
    
    /** Send all the data to the remote host.
    \param data The buffer to send to the host.
    \param length The length of the buffer to send.
    \return the number of written bytes on success (>=0) or -1 on failure
    */
    virtual int send_all(const char* data, int length) = 0;
    
    /** Receive data from the remote host.
    \param data The buffer in which to store the data received from the host.
    \param length The maximum length of the buffer.
    \return the number of received bytes on success (>=0) or -1 on failure
     */
    virtual int receive(char* data, int length) = 0;

    /** Receive data from the remote host.
    \param data The buffer in which to store the data received from the host.
    \param dataSize The maximum length of the buffer.
    \param searchPattern : the pattern to search for, reading will stop when pattern is found
    \return the number of received bytes on success (>=0) or -1 on failure
     */
    virtual int receive(char* data, int dataSize, const char* searchPattern) = 0;


    

protected:
    bool    _is_connected;

};


#endif  //BASESOCKET_H
