/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

 /*
 *  SSL client leveraging TLS-MBED
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

/*
 * LinuxSocket Class :  Integrating mBed's httpClient & mBed's TLS for Linux usage
 *
 * Nhon Chu - March 2016 
 *
 */




#include <string.h>

#include "tlsSocket.h"


typedef struct {
	char						trustedCaFolderName[256];

	mbedtls_net_context 		server_fd;
	mbedtls_entropy_context     entropy;
	mbedtls_ctr_drbg_context    ctr_drbg;
	mbedtls_ssl_context         ssl;
	mbedtls_ssl_config          conf;
	mbedtls_x509_crt            cacert;
	mbedtls_x509_crt            clicert;
	mbedtls_pk_context          pkey;
	int							is_connected;
} tlsSocket_st;

#define SOCKET_OBJECT(obj)	tlsSocket_st* socket = (tlsSocket_st * ) obj

#define DEBUG_LEVEL 1

static void my_debug( void *ctx, int level,
					  const char *file, int line,
					  const char *str )
{
	((void) level);

	fprintf(stdout, "%s:%04d: %s", file, line, str );
}



void* tlsSocket_create()
{
	tlsSocket_st * socket = (tlsSocket_st *) malloc(sizeof(tlsSocket_st));

	memset(socket, 0, sizeof(tlsSocket_st));
	strcpy(socket->trustedCaFolderName, "certs");

	return socket;
}

void *	tlsSocket_delete(void* sockObj)
{
	SOCKET_OBJECT(sockObj);

	tlsSocket_free(sockObj);

	if (socket)	free(socket);

	return NULL;
}

int tlsSocket_connect(void* sockObj, const char* host, const int port, const char * rootCA, const char* certificate, const char * privateKey)
{
	int 						ret;
	uint32_t 					flags;
	const char *				pers = "tlsSocket";
	char 						szPort[8] = {0};

	SOCKET_OBJECT(sockObj);

#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold( DEBUG_LEVEL );
#endif

	sprintf(szPort, "%d", port);

	/*
	 * 0. Initialize the RNG and the session data
	 */
	mbedtls_net_init( &socket->server_fd );
	mbedtls_ssl_init( &socket->ssl );
	mbedtls_ssl_config_init( &socket->conf );
	mbedtls_x509_crt_init( &socket->cacert );
	mbedtls_ctr_drbg_init( &socket->ctr_drbg );

	mbedtls_x509_crt_init( &socket->clicert );
	mbedtls_pk_init( &socket->pkey );

	fprintf(stdout,  "\n  . Seeding the random number generator..." );

	mbedtls_entropy_init( &socket->entropy );
	if( ( ret = mbedtls_ctr_drbg_seed( &socket->ctr_drbg, mbedtls_entropy_func, &socket->entropy,
							   (const unsigned char *) pers,
							   strlen( pers ) ) ) != 0 )
	{
		fprintf(stdout,  " failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret );
		tlsSocket_get_error(socket, ret);
		tlsSocket_free(socket);
		return ret;
	}

	fprintf(stdout,  " ok\n" );

	/*
	 * 0. Initialize certificates
	 */
	fprintf(stdout,  "  . Loading the CA root certificate ..." );

	if (rootCA && strlen(rootCA))
	{
		fprintf(stdout,  " %s", rootCA );
		ret = mbedtls_x509_crt_parse_file(&socket->cacert, rootCA);
	}
	else if (strlen(socket->trustedCaFolderName) == 0)
	{
		ret = mbedtls_x509_crt_parse( &socket->cacert, (const unsigned char *) mbedtls_test_cas_pem, mbedtls_test_cas_pem_len );
	}
	else
	{
		//ret = mbedtls_x509_crt_parse_file(&socket->cacert, "/legato/systems/current/apps/socialService/read-only/certs/Comodo_Trusted_Services_root.pem");
		fprintf(stdout,  " load certs from %s", socket->trustedCaFolderName);
		ret = mbedtls_x509_crt_parse_path(&socket->cacert, socket->trustedCaFolderName);
	}
	if( ret < 0 )
	{
		fprintf(stdout,  " failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n", -ret );

		//let's do another attempt for (Legato prior 16.04)
		#if 1
		if (strlen(socket->trustedCaFolderName) > 0)
		{
			strcpy(socket->trustedCaFolderName, "read-only/certs");
			fprintf(stdout,  " load certs from %s", socket->trustedCaFolderName);
			ret = mbedtls_x509_crt_parse_path(&socket->cacert, socket->trustedCaFolderName);
			if (ret < 0)
			{
				fprintf(stdout,  " failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n", -ret );
			}
		}
		#endif

		if (ret < 0)
		{
			tlsSocket_get_error(socket, ret);
			tlsSocket_free(socket);
			return ret;
		}
	}

	fprintf(stdout,  " ok (%d skipped)\n", ret );

	if (certificate && strlen(certificate))
	{
		fprintf(stdout,  "  . Loading the client certificate... %s", certificate);
		ret = mbedtls_x509_crt_parse_file(&socket->clicert, certificate);
		if(ret != 0) {
			fprintf(stdout,  " failed\n  !  mbedtls_x509_crt_parse_file returned -0x%x while parsing device cert\n\n", -ret);
			tlsSocket_get_error(socket, ret);
			tlsSocket_free(socket);
			return ret;
		}
		fprintf(stdout,  " ok\n" );
	}

	if (privateKey && strlen(privateKey))
	{
		fprintf(stdout,  "  . Loading the client private key... %s", privateKey);
		ret = mbedtls_pk_parse_keyfile(&socket->pkey, privateKey, "");
		if(ret != 0) {
			fprintf(stdout,  " failed\n  !  mbedtls_pk_parse_keyfile returned -0x%x while parsing private key\n\n", -ret);
			fprintf(stdout,  " path : %s ", privateKey);
			tlsSocket_get_error(socket, ret);
			tlsSocket_free(socket);
			return ret;
		}
		fprintf(stdout,  " ok\n" );
	}


	/*
	 * 1. Start the connection
	 */
	fprintf(stdout,  "  . Connecting to tcp/%s/%s...", host, szPort);
	fflush(stdout);

	if( ( ret = mbedtls_net_connect( &socket->server_fd, host, szPort, MBEDTLS_NET_PROTO_TCP ) ) != 0 )
	{
		fprintf(stdout,  " failed\n  ! mbedtls_net_connect returned %d\n\n", ret );
		tlsSocket_get_error(socket, ret);
		tlsSocket_free(socket);
		return ret;
	}

	fprintf(stdout,  " ok\n" );

	/*
	 * 2. Setup stuff
	 */
	fprintf(stdout,  "  . Setting up the TLS structure..." );

	if( ( ret = mbedtls_ssl_config_defaults( &socket->conf,
					MBEDTLS_SSL_IS_CLIENT,
					MBEDTLS_SSL_TRANSPORT_STREAM,
					MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
	{
		fprintf(stdout,  " failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret );
		tlsSocket_get_error(socket, ret);
		tlsSocket_free(socket);
		return ret;
	}

	fprintf(stdout,  " ok\n" );

	/* OPTIONAL is not optimal for security,
	 * but makes interop easier in this simplified example */
	mbedtls_ssl_conf_authmode( &socket->conf, MBEDTLS_SSL_VERIFY_OPTIONAL );
	mbedtls_ssl_conf_ca_chain( &socket->conf, &socket->cacert, NULL );
	mbedtls_ssl_conf_rng( &socket->conf, mbedtls_ctr_drbg_random, &socket->ctr_drbg );
	mbedtls_ssl_conf_dbg( &socket->conf, my_debug, stdout );

	if (certificate && strlen(certificate) && privateKey && strlen(privateKey))
	{
		if( (ret = mbedtls_ssl_conf_own_cert(&socket->conf, &socket->clicert, &socket->pkey)) != 0)
		{
			fprintf(stdout, " failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
			tlsSocket_get_error(socket, ret);
			tlsSocket_free(socket);
			return ret;
		}
	}

	if( ( ret = mbedtls_ssl_setup( &socket->ssl, &socket->conf ) ) != 0 )
	{
		fprintf(stdout,  " failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret );
		tlsSocket_get_error(socket, ret);
		tlsSocket_free(socket);
		return ret;
	}

	if( ( ret = mbedtls_ssl_set_hostname( &socket->ssl, host ) ) != 0 )
	{
		fprintf(stdout,  " failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret );
		tlsSocket_get_error(socket, ret);
		tlsSocket_free(socket);
		return ret;
	}

	//mbedtls_ssl_set_bio( &socket->ssl, &socket->server_fd, mbedtls_net_send, mbedtls_net_recv, NULL );
	mbedtls_ssl_set_bio( &socket->ssl, &socket->server_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
	mbedtls_ssl_conf_read_timeout(&socket->conf, 10000);

	/*
	 * 4. Handshake
	 */
	fprintf(stdout,  "  . Performing the TLS handshake..." );

	while( ( ret = mbedtls_ssl_handshake( &socket->ssl ) ) != 0 )
	{
		if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE )
		{
			fprintf(stdout,  " failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", -ret );
			tlsSocket_get_error(socket, ret);
			tlsSocket_free(socket);
			return ret;
		}
	}

	fprintf(stdout,  " ok\n" );

	fprintf(stdout, "[ Protocol is %s ]\n[ Ciphersuite is %s ]\n", mbedtls_ssl_get_version(&socket->ssl), mbedtls_ssl_get_ciphersuite(&socket->ssl));

	/*
	 * 5. Verify the server certificate
	 */
	fprintf(stdout,  "  . Verifying peer X.509 certificate..." );

	/* In real life, we probably want to bail out when ret != 0 */
	if( ( flags = mbedtls_ssl_get_verify_result( &socket->ssl ) ) != 0 )
	{
		char vrfy_buf[512];

		fprintf(stdout,  " failed\n" );

		mbedtls_x509_crt_verify_info( vrfy_buf, sizeof( vrfy_buf ), "  ! ", flags );

		fprintf(stdout,  "%s\n", vrfy_buf );
	}
	else
	{
		fprintf(stdout,  " ok\n" );
	}



	socket->is_connected = (ret == 0 ? 1 : 0);

	return( ret );
}

void tlsSocket_close(void* sockObj)
{
	SOCKET_OBJECT(sockObj);

	if (socket->is_connected)
	{
		fprintf(stdout, "tlsSocket_close %d", socket->is_connected);
		fflush(stdout);

		mbedtls_ssl_close_notify( &socket->ssl );
		tlsSocket_free(socket);
	}
}

int tlsSocket_is_connected(void* sockObj)
{
	SOCKET_OBJECT(sockObj);

	return socket->is_connected;
}

void tlsSocket_set_timeout(void* sockObj, unsigned int timeout_ms)
{
	SOCKET_OBJECT(sockObj);

	if (timeout_ms == 0)
	{
		timeout_ms = 1500;
	}

	mbedtls_ssl_conf_read_timeout(&socket->conf, timeout_ms);
}

int tlsSocket_send(void* sockObj, const char* data, int length)
{
	SOCKET_OBJECT(sockObj);

	if (!socket->is_connected)
	{
		return -1;
	}

	int rc = mbedtls_ssl_write(&socket->ssl, (const unsigned char*) data, length);

	//socket->is_connected = (rc != 0);

	return rc;
}

int tlsSocket_receive(void* sockObj, char* data, int length)
{
	SOCKET_OBJECT(sockObj);

	if (!socket->is_connected)
	{
		return -1;
	}

	int bytes = 0;
	while (bytes < length)
	{
		//fprintf(stdout, "TLS-Socket::receive - reading %d bytes", (size_t)(length - bytes));

		int rc = mbedtls_ssl_read( &socket->ssl, (unsigned char*)&data[bytes], (size_t)(length - bytes) );

		if (rc < 0)
		{
			//fprintf(stdout, "tlsRead(%d, %d)", rc, errno);
			//fflush(stdout);

			//socket->is_connected = false;
			if (rc == MBEDTLS_ERR_SSL_CONN_EOF)
			{
				bytes = -3;	//CON_EOF
			}
			else if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
			{
				bytes = -3;	//CON_EOF
			}
			else
			{
				bytes = -1;
			}

			break;
			/*
			else if (errno != ENOTCONN && errno != ECONNRESET)
			{
				bytes = -3;	//CON_EOF
				break;
			}
			*/
		}
		else if (rc > 0)
		{
			bytes += rc;
		}
		else
		{
			break;
		}
	}

	return bytes;
}


int tlsSocket_receive_pattern(void* sockObj, char* data, int dataSize, const char* searchPattern)
{
	SOCKET_OBJECT(sockObj);

	if (!socket->is_connected)
	{
		return -1;
	}

	if (NULL == searchPattern)
	{
		fprintf(stdout, "TLS-Socket::receive - error : No Pattern");
		return 0;
	}

	if (strlen(searchPattern) == 0)
	{
		fprintf(stdout, "TLS-Socket::receive - error : Empty Pattern");
		return 0;
	}

	fprintf(stdout, "TLS-Socket::receive - search for Pattern %s", searchPattern);

	int 	index = 0;
	int     nMatchCount = strlen(searchPattern);
	int     nSearchIndex = 0;
	char 	buf;

	while (mbedtls_ssl_read(&socket->ssl, (unsigned char *) &buf, 1) == 1)
	{
		data[index++] = buf;

		if (buf == searchPattern[nSearchIndex])
		{
			nSearchIndex++;
			if (nSearchIndex == nMatchCount)
			{
				fprintf(stdout, "TLS-Socket::receive - Found Pattern. Data size = %d", index);
				break;
			}
		}
		else
		{
			nSearchIndex = 0;
		}

		if (index >= dataSize)
		{
			fprintf(stdout, "TLS-Socket::receive - Pattern not found. Buffer too short");
			break;		
		}
	}

	fprintf(stdout, "TLS-Socket::receive - read count = %d", index);
	data[index] = '\0';
	return index;
}



void tlsSocket_get_error(void* sockObj, int errorCode)
{
	//SOCKET_OBJECT(sockObj);

	if( errorCode != 0 )
	{
		char error_buf[100];
		mbedtls_strerror( errorCode, error_buf, 100 );
		fprintf(stdout, "Last error was: %d - %s\n\n", errorCode, error_buf );
	}
}

void tlsSocket_free(void* sockObj)
{
	SOCKET_OBJECT(sockObj);

	if (!socket->is_connected)
	{
		mbedtls_net_free( &socket->server_fd );
		mbedtls_x509_crt_free( &socket->cacert );
		mbedtls_x509_crt_free( &socket->clicert );
		mbedtls_pk_free( &socket->pkey );
		mbedtls_ssl_free( &socket->ssl );
		mbedtls_ssl_config_free( &socket->conf );
		mbedtls_ctr_drbg_free( &socket->ctr_drbg );
		mbedtls_entropy_free( &socket->entropy );

		memset(socket, 0, sizeof(tlsSocket_st));
		strcpy(socket->trustedCaFolderName, "certs");
	}
}