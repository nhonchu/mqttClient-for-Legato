/* socketInterface.cpp */
/* 
 *
 * Provides C interface to C++ socket classes
 *
 * Nhon Chu
 *
 */


#include "legato.h"
#include "SocketInterface.h"
#include "LinuxSocket.h"
#include "LinuxTLSSocket.h"

#define	MQTT_PORT			1883
#define MQTT_SECURED_PORT	8883


#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Connect
 *
 */
//--------------------------------------------------------------------------------------------------
void* SOCKET_connect
(
	const char*		serverUrl,
	int 			port
)
{
	BaseSocket* 	pSock = NULL;

	if (MQTT_SECURED_PORT == port)
	{
		LE_INFO("*** Using TLS Socket ***");
		fflush(stdout);
		pSock = new LinuxTLSSocket();
	}
	else
	{
		LE_INFO("*** Using REGULAR Socket ***");
		fflush(stdout);
		pSock = new LinuxSocket();
	}

	if (pSock)
	{
		LE_INFO("Connecting to server");
		int ret = pSock->connect(serverUrl, port);
		if (ret < 0)
		{
			pSock->close();
			LE_INFO("Could not connect: %d", ret);
			delete pSock;
			pSock = NULL;
		}
	}
	else
	{
		LE_INFO("Cannot instantiate Socket object");
	}

	return pSock;
}

//--------------------------------------------------------------------------------------------------
/**
 * Close
 *
 */
//--------------------------------------------------------------------------------------------------
void* SOCKET_close
(
	void*  			pInstance
)
{
	if (pInstance)
	{
		BaseSocket* 	pSock = (BaseSocket *) pInstance;

		pSock->close();
		delete pSock;
	}

	return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * IsConnect
 *
 */
//--------------------------------------------------------------------------------------------------
int SOCKET_isConnected
(
	void*  			pInstance
)
{
	if (pInstance)
	{
		BaseSocket* 	pSock = (BaseSocket *) pInstance;

		return pSock->is_connected();
	}

	return false;
}


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
)
{
	if (pInstance)
	{
		BaseSocket* 	pSock = (BaseSocket *) pInstance;

		//LE_INFO("SOCKET_setTimeout: %u", timeout_ms);
		pSock->set_blocking(true, timeout_ms);
	}
}

//--------------------------------------------------------------------------------------------------
/**
 * Receive
 *
 */
//--------------------------------------------------------------------------------------------------
int SOCKET_receive
(
	void*  			pInstance,
	char*			pData,
	int 			dataLength
)
{
	if (pInstance)
	{
		BaseSocket* 	pSock = (BaseSocket *) pInstance;

		return pSock->receive(pData, dataLength);
	}

	return -1;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send
 *
 */
//--------------------------------------------------------------------------------------------------
int SOCKET_send
(
	void*  			pInstance,
	const char*		pData,
	int 			dataLength
)
{
	if (pInstance)
	{
		BaseSocket* 	pSock = (BaseSocket *) pInstance;

		return pSock->send(pData, dataLength);
	}

	return -1;
}


#ifdef __cplusplus
}
#endif