/*
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
 * LinuxTLSSocket Class :  Integrating mbed TLS for Linux usage
 *
 * Nhon Chu - March 2016 
 *
 */



#include "legato.h"
#include <string.h>

#include "LinuxTLSSocket.h"



#define DEBUG_LEVEL 1

static void my_debug( void *ctx, int level,
					  const char *file, int line,
					  const char *str )
{
	((void) level);

	LE_INFO("%s:%04d: %s", file, line, str );
}



LinuxTLSSocket::LinuxTLSSocket()
{
	//memset(_trustedCaFolderName, sizeof(_trustedCaFolderName), 0);
	strcpy(_trustedCaFolderName, "certs");
}

LinuxTLSSocket::~LinuxTLSSocket()
{
	close();
}

int LinuxTLSSocket::connect(const char* host, const int port)
{
	int 						ret;
	uint32_t 					flags;
	const char *				pers = "L1nuxS0ck@t2";
	char 						szPort[8] = {0};

#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold( DEBUG_LEVEL );
#endif

	sprintf(szPort, "%d", port);

	/*
	 * 0. Initialize the RNG and the session data
	 */
	mbedtls_net_init( &_server_fd );
	mbedtls_ssl_init( &_ssl );
	mbedtls_ssl_config_init( &_conf );
	mbedtls_x509_crt_init( &_cacert );
	mbedtls_ctr_drbg_init( &_ctr_drbg );

	LE_INFO( "\n  . Seeding the random number generator..." );

	mbedtls_entropy_init( &_entropy );
	if( ( ret = mbedtls_ctr_drbg_seed( &_ctr_drbg, mbedtls_entropy_func, &_entropy,
							   (const unsigned char *) pers,
							   strlen( pers ) ) ) != 0 )
	{
		LE_INFO( " failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret );
		getSSLerror(ret);
		freeSSL();
		return ret;
	}

	LE_INFO( " ok\n" );

	/*
	 * 0. Initialize certificates
	 */
	LE_INFO( "  . Loading the CA root certificate ..." );

	if (strlen(_trustedCaFolderName) == 0)
	{
		ret = mbedtls_x509_crt_parse( &_cacert, (const unsigned char *) mbedtls_test_cas_pem, mbedtls_test_cas_pem_len );
	}
	else
	{
		//ret = mbedtls_x509_crt_parse_file(&_cacert, "/legato/systems/current/apps/socialService/read-only/certs/Comodo_Trusted_Services_root.pem");
		LE_INFO( " load certs from %s", _trustedCaFolderName);
		ret = mbedtls_x509_crt_parse_path(&_cacert, _trustedCaFolderName);
	}
	if( ret < 0 )
	{
		LE_INFO( " failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n", -ret );

		//let's do another attempt for (Legato prior 16.04)
		#if 1
		if (strlen(_trustedCaFolderName) > 0)
		{
			strcpy(_trustedCaFolderName, "read-only/certs");
			LE_INFO( " load certs from %s", _trustedCaFolderName);
			ret = mbedtls_x509_crt_parse_path(&_cacert, _trustedCaFolderName);
			if (ret < 0)
			{
				LE_INFO( " failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n", -ret );
			}
		}
		#endif

		if (ret < 0)
		{
			getSSLerror(ret);
			freeSSL();
			return ret;
		}
	}

	LE_INFO( " ok (%d skipped)\n", ret );

	/*
	 * 1. Start the connection
	 */
	LE_INFO( "  . Connecting to tcp/%s/%s...", host, szPort);
	fflush(stdout);

	if( ( ret = mbedtls_net_connect( &_server_fd, host, szPort, MBEDTLS_NET_PROTO_TCP ) ) != 0 )
	{
		LE_INFO( " failed\n  ! mbedtls_net_connect returned %d\n\n", ret );
		getSSLerror(ret);
		freeSSL();
		return ret;
	}

	LE_INFO( " ok\n" );

	/*
	 * 2. Setup stuff
	 */
	LE_INFO( "  . Setting up the TLS structure..." );

	if( ( ret = mbedtls_ssl_config_defaults( &_conf,
					MBEDTLS_SSL_IS_CLIENT,
					MBEDTLS_SSL_TRANSPORT_STREAM,
					MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
	{
		LE_INFO( " failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret );
		getSSLerror(ret);
		freeSSL();
		return ret;
	}

	LE_INFO( " ok\n" );

	/* OPTIONAL is not optimal for security,
	 * but makes interop easier in this simplified example */
	mbedtls_ssl_conf_authmode( &_conf, MBEDTLS_SSL_VERIFY_OPTIONAL );
	mbedtls_ssl_conf_ca_chain( &_conf, &_cacert, NULL );
	mbedtls_ssl_conf_rng( &_conf, mbedtls_ctr_drbg_random, &_ctr_drbg );
	mbedtls_ssl_conf_dbg( &_conf, my_debug, stdout );

	if( ( ret = mbedtls_ssl_setup( &_ssl, &_conf ) ) != 0 )
	{
		LE_INFO( " failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret );
		getSSLerror(ret);
		freeSSL();
		return ret;
	}

	if( ( ret = mbedtls_ssl_set_hostname( &_ssl, host ) ) != 0 )
	{
		LE_INFO( " failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret );
		getSSLerror(ret);
		freeSSL();
		return ret;
	}

	//mbedtls_ssl_set_bio( &_ssl, &_server_fd, mbedtls_net_send, mbedtls_net_recv, NULL );
	mbedtls_ssl_set_bio( &_ssl, &_server_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
	mbedtls_ssl_conf_read_timeout(&_conf, 10000);

	/*
	 * 4. Handshake
	 */
	LE_INFO( "  . Performing the TLS handshake..." );

	while( ( ret = mbedtls_ssl_handshake( &_ssl ) ) != 0 )
	{
		if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE )
		{
			LE_INFO( " failed\n  ! mbedtls_ssl_handshake returned -0x%x\n\n", -ret );
			getSSLerror(ret);
			freeSSL();
			return ret;
		}
	}

	LE_INFO( " ok\n" );

	/*
	 * 5. Verify the server certificate
	 */
	LE_INFO( "  . Verifying peer X.509 certificate..." );

	/* In real life, we probably want to bail out when ret != 0 */
	if( ( flags = mbedtls_ssl_get_verify_result( &_ssl ) ) != 0 )
	{
		char vrfy_buf[512];

		LE_INFO( " failed\n" );

		mbedtls_x509_crt_verify_info( vrfy_buf, sizeof( vrfy_buf ), "  ! ", flags );

		LE_INFO( "%s\n", vrfy_buf );
	}
	else
	{
		LE_INFO( " ok\n" );
	}



	_is_connected = (ret == 0 ? true : false);

	return( ret );
}

void LinuxTLSSocket::close()
{
	if (_is_connected)
	{
		mbedtls_ssl_close_notify( &_ssl );
		freeSSL();
	}
}

bool LinuxTLSSocket::is_connected(void)
{
	return _is_connected;
}

void LinuxTLSSocket::set_blocking(bool blocking, unsigned int timeout_ms)
{
	mbedtls_ssl_conf_read_timeout(&_conf, timeout_ms);
}

int LinuxTLSSocket::send(const char* data, int length)
{
	if (!_is_connected)
	{
		return -1;
	}

	int rc = mbedtls_ssl_write(&_ssl, (const unsigned char*) data, length);

	//_is_connected = (rc != 0);

	return rc;
}

int LinuxTLSSocket::send_all(const char* data, int length)
{
	return send(data, length);
}

int LinuxTLSSocket::receive(char* data, int length)
{
	if (!_is_connected)
	{
		return -1;
	}

	int bytes = 0;
	while (bytes < length)
	{
		//LE_INFO("TLS-Socket::receive - reading %d bytes", (size_t)(length - bytes));

		int rc = mbedtls_ssl_read( &_ssl, (unsigned char*)&data[bytes], (size_t)(length - bytes) );

		if (rc < 0)
		{
			//_is_connected = false;
			if (errno != ENOTCONN && errno != ECONNRESET)
			{
				bytes = -1;
				break;
			}
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


int LinuxTLSSocket::receive(char* data, int dataSize, const char* searchPattern)
{
	if (!_is_connected)
	{
		return -1;
	}

	if (NULL == searchPattern)
	{
		LE_INFO("TLS-Socket::receive - error : No Pattern");
		return 0;
	}

	if (strlen(searchPattern) == 0)
	{
		LE_INFO("TLS-Socket::receive - error : Empty Pattern");
		return 0;
	}

	LE_INFO("TLS-Socket::receive - search for Pattern %s", searchPattern);

	int 	index = 0;
	int     nMatchCount = strlen(searchPattern);
	int     nSearchIndex = 0;
	char 	buf;

	while (mbedtls_ssl_read(&_ssl, (unsigned char *) &buf, 1) == 1)
	{
		data[index++] = buf;

		if (buf == searchPattern[nSearchIndex])
		{
			nSearchIndex++;
			if (nSearchIndex == nMatchCount)
			{
				LE_INFO("TLS-Socket::receive - Found Pattern. Data size = %d", index);
				break;
			}
		}
		else
		{
			nSearchIndex = 0;
		}

		if (index >= dataSize)
		{
			LE_INFO("TLS-Socket::receive - Pattern not found. Buffer too short");
			break;		
		}
	}

	LE_INFO("TLS-Socket::receive - read count = %d", index);
	data[index] = '\0';
	return index;
}



void LinuxTLSSocket::getSSLerror(int errorCode)
{
	if( errorCode != 0 )
	{
		char error_buf[100];
		mbedtls_strerror( errorCode, error_buf, 100 );
		LE_INFO("Last error was: %d - %s\n\n", errorCode, error_buf );
	}
}

void LinuxTLSSocket::freeSSL()
{
	if (_is_connected)
	{
		_is_connected = false;

		mbedtls_net_free( &_server_fd );

		mbedtls_x509_crt_free( &_cacert );
		mbedtls_ssl_free( &_ssl );
		mbedtls_ssl_config_free( &_conf );
		mbedtls_ctr_drbg_free( &_ctr_drbg );
		mbedtls_entropy_free( &_entropy );
	}
}