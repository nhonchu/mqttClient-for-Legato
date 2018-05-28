/* socketInterface.h */
/* 
 *
 * Provides C interface to C++ socket classes
 *
 * Nhon Chu
 *
 */

/** \file
HTTP Interface header file
*/

#ifndef SOCKET_INTERFACE_H
#define SOCKET_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Connect
 *		return a pointer to a socket instance, required for subsequent calls
 */
//--------------------------------------------------------------------------------------------------
void* SOCKET_connect
(
	const char*		serverUrl,
	int 			port
);

//--------------------------------------------------------------------------------------------------
/**
 * Close
 *
 */
//--------------------------------------------------------------------------------------------------
void* SOCKET_close
(
	void*  			pInstance
);

//--------------------------------------------------------------------------------------------------
/**
 * IsConnect
 *
 */
//--------------------------------------------------------------------------------------------------
int SOCKET_isConnected
(
	void*  			pInstance
);

//--------------------------------------------------------------------------------------------------
/**
 * Timeout
 *		Set timeout in ms for send and receive
 */
//--------------------------------------------------------------------------------------------------
void SOCKET_setTimeout
(
	void*  			pInstance,
	unsigned int 	timeout_ms
);

//--------------------------------------------------------------------------------------------------
/**
 * Receive
 *		returns number of bytes received, -1 if fails
 */
//--------------------------------------------------------------------------------------------------
int SOCKET_receive
(
	void*  			pInstance,
	char*			pData,
	int 			dataLength
);

//--------------------------------------------------------------------------------------------------
/**
 * Send
 *		return number of bytes sent. 0 if fails
 */
//--------------------------------------------------------------------------------------------------
int SOCKET_send
(
	void*  			pInstance,
	const char*		pData,
	int 			dataLength
);


#ifdef __cplusplus
}
#endif

#endif  //SOCKET_INTERFACE_H
