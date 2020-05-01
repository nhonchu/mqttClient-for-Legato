/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/
//#include "legato.h"

#include "MQTTLinux.h"

#include "tlsSocket.h"

int expired(Timer* timer)
{
	struct timeval now, res;
	gettimeofday(&now, NULL);
	timersub(&timer->end_time, &now, &res);		

	//fprintf(stdout, "expired(%d, %d) ", res.tv_sec, res.tv_usec);
	//fflush(stdout);

	return res.tv_sec < 0 || (res.tv_sec == 0 && res.tv_usec <= 0);
}


void countdown_ms(Timer* timer, unsigned int timeout)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	struct timeval interval = {timeout / 1000, (timeout % 1000) * 1000};
	timeradd(&now, &interval, &timer->end_time);
}


void countdown(Timer* timer, unsigned int timeout)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	struct timeval interval = {timeout, 0};
	timeradd(&now, &interval, &timer->end_time);
}


int left_ms(Timer* timer)
{
	struct timeval now, res;
	gettimeofday(&now, NULL);
	timersub(&timer->end_time, &now, &res);
	//printf("left %d ms\n", (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000);
	return (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000;
}


void InitTimer(Timer* timer)
{
	timer->end_time = (struct timeval){0, 0};
}


int linux_read(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
	if (timeout_ms <= 0)
	{
		timeout_ms = 500;
	}
	
	if (n->useTLS)
	{
		tlsSocket_set_timeout(n->tlsSocketObject, timeout_ms);
		return tlsSocket_receive(n->tlsSocketObject, (char *) buffer, len);
	}
	else
	{
		struct timeval interval = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
		if (interval.tv_sec < 0 || (interval.tv_sec == 0 && interval.tv_usec <= 0))
		{
			interval.tv_sec = 0;
			interval.tv_usec = 100;
		}

		setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&interval, sizeof(struct timeval));
		//fprintf(stdout, "linux_read, setting timeout : %d(%d), %ld, %ld\n", n->my_socket, len, interval.tv_sec, interval.tv_usec);
		//fflush(stdout);

		int bytes = 0;
		while (bytes < len)
		{
			//fprintf(stdout, "linux_read, recv : %d(%d), %d\n", n->my_socket, len, bytes);
			//fflush(stdout);

			int rc = recv(n->my_socket, &buffer[bytes], (size_t)(len - bytes), 0);

			//fprintf(stdout, "linux_read, recv done: %d(%d)\n", n->my_socket, rc);
			//fflush(stdout);

			if (rc > 0)
			{
				#if 0
				int i;
				for (i=0; i<rc; i++)
				{
					fprintf(stdout, "%.2x ", buffer[bytes+i]);
	            }
	            fprintf(stdout, "\n");
	            fflush(stdout);
	            #endif
	            
				bytes += rc;
			}
			else if (rc == 0)
			{
				bytes = -3;	//CON_EOF, server closed the connection
				break;
			}
			else
			{
				//rc == -1 -> is read timeout
				fprintf(stdout, "linux_read(%d, %d)", rc, errno);
				fflush(stdout);

				break;
			}
			
		}

		return bytes;
	}
}


int linux_write(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
	if (n->useTLS)
	{
		tlsSocket_set_timeout(n->tlsSocketObject, timeout_ms);
		return tlsSocket_send(n->tlsSocketObject, (char *)buffer, len);
	}
	else
	{
		struct timeval tv;

		tv.tv_sec = 0;  /* 30 Secs Timeout */
		tv.tv_usec = timeout_ms * 1000;  // Not init'ing this can cause strange errors

		setsockopt(n->my_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
		int	rc = write(n->my_socket, buffer, len);
		return rc;
	}
}


void linux_disconnect(Network* n)
{
	if (n->useTLS)
	{
		if (n->tlsSocketObject)
		{
			tlsSocket_close(n->tlsSocketObject);
			n->tlsSocketObject = tlsSocket_delete(n->tlsSocketObject);
		}
	}
	else
	{
		if (n->my_socket != -1)
		{
			close(n->my_socket);
			n->my_socket = -1;
		}
	}
}


void NewNetwork(Network* n, int useTLS)
{
	n->tlsSocketObject = NULL;
	n->my_socket = -1;
	n->useTLS = useTLS;

	n->mqttread = 	linux_read;
	n->mqttwrite = 	linux_write;
	n->connect = 	linux_connect;
	n->disconnect = linux_disconnect;
}


int linux_connect(Network* n, const char* addr, int port, const char * rootCA,
		const char * certificate, const char * privateKey) {
	int rc = -1;
	int opt;
	struct timeval timeout;

	if (n->useTLS) {

		if (n->tlsSocketObject) {
			linux_disconnect(n);
		}
		n->tlsSocketObject = tlsSocket_create();

		rc = tlsSocket_connect(n->tlsSocketObject, addr, port, rootCA,
				certificate, privateKey);
	} else {
		if (n->my_socket != -1) {
			close(n->my_socket);
		}
		int type = SOCK_STREAM;
		struct sockaddr_in address;
		sa_family_t family = AF_INET;
		struct addrinfo *result = NULL;
		struct addrinfo hints = { 0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0,
				NULL, NULL, NULL };

		if ((rc = getaddrinfo(addr, NULL, &hints, &result)) == 0) {
			struct addrinfo* res = result;

			while (res) {
				if (res->ai_family == AF_INET) {
					result = res;
					break;
				}
				res = res->ai_next;
			}

			if (result->ai_family == AF_INET) {
				address.sin_port = htons(port);
				address.sin_family = family = AF_INET;
				address.sin_addr =
						((struct sockaddr_in*) (result->ai_addr))->sin_addr;
			} else
				rc = -1;

			freeaddrinfo(result);
		}

		if (rc == 0) {
			//fprintf(stdout, "Connect : opening socket...");
			n->my_socket = socket(family, type, 0);
			fprintf(stdout, "\nConnect : opening socket : %d\n", n->my_socket);

			if (n->my_socket != -1) {
				struct in_addr ipAddr = address.sin_addr;
				char str[INET_ADDRSTRLEN];
				inet_ntop( AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
				fprintf(stdout, "\nconnecting to %s:%d\n", str, port);

				// get socket flags
				if ((opt = fcntl(n->my_socket, F_GETFL, NULL)) < 0) {
					return -1;
				}

				// set socket non-blocking
				if (fcntl(n->my_socket, F_SETFL, opt | O_NONBLOCK) < 0) {
					return -1;
				}

				if ((rc = connect(n->my_socket, (struct sockaddr*) &address,
						sizeof(address))) < 0) {
					if (errno == EINPROGRESS) {
						fd_set wait_set;

						// make file descriptor set with socket
						FD_ZERO(&wait_set);
						FD_SET(n->my_socket, &wait_set);

						// wait for socket to be writable; return after given timeout
						timeout.tv_sec = 1;
						timeout.tv_usec = 0;
						rc = select(n->my_socket + 1, NULL, &wait_set, NULL,
								&timeout);
					}
				}
				// connection was successful immediately
				else {
					fprintf(stdout,
							"\nconnection was successful immediately : connection result : %d\n",
							rc);
					rc = 0;
				}
				// reset socket flags
				if (fcntl(n->my_socket, F_SETFL, opt) < 0) {
					fprintf(stdout,
							"\nreset socket flags : connection result : %d\n", rc);
					return -1;
				}

				// an error occured in connect or select
				if (rc < 0) {
					fprintf(stdout,
							"\nan error occured in connect or select : connection result : %d\n",
							rc);
					return -1;
				}
				// select timed out
				else if (rc == 0) {
					errno = ETIMEDOUT;
					fprintf(stdout, "\ntimed out : connection result : %d\n", rc);
					return -1;
				}
				// almost finished
				else {
					socklen_t len = sizeof(opt);

					// check for errors in socket layer
					if (getsockopt(n->my_socket, SOL_SOCKET, SO_ERROR, &opt,
							&len) < 0) {
						fprintf(stdout,
								"\ncheck for errors in socket layer : connection result : %d\n",
								rc);
						return -1;
					}

					// there was an error
					if (opt) {
						errno = opt;
						fprintf(stdout,
								"\nthere was an error : connection result : %d\n",
								rc);
						return -1;
					}
				}
				fprintf(stdout, "\nConnect : connection result : %d\n", rc);

			}
			fflush(stdout);
		}
	}

	return rc;
}
