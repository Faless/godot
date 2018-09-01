/*************************************************************************/
/*  net_socket_unix.h                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef NET_SOCKET_UNIX_H
#define NET_SOCKET_UNIX_H

#include "io/net_socket.h"

#if defined(UNIX_ENABLED)

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef NO_FCNTL
#ifdef __HAIKU__
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#else
#include <sys/ioctl.h>
#endif
#include <netinet/in.h>

#include <sys/socket.h>
#ifdef JAVASCRIPT_ENABLED
#include <arpa/inet.h>
#endif

#include <netinet/tcp.h>

#if defined(OSX_ENABLED) || defined(IPHONE_ENABLED)
#define MSG_NOSIGNAL SO_NOSIGPIPE
#endif

#define SOCKET_TYPE int
#define SOCKET_EMPTY -1
#define BUF_T(x) x

#elif defined(WINDOWS_ENABLED)

#include <winsock2.h>
#include <ws2tcpip.h>

#define SOCKET_TYPE SOCKET
#define SOCKET_EMPTY INVALID_SOCKET
#define BUF_T(x) ((char *)x)

#if defined(__MINGW32__) && (!defined(__MINGW64_VERSION_MAJOR) || __MINGW64_VERSION_MAJOR < 4)
// Workaround for mingw-w64 < 4.0
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif
#endif

#endif

class NetSocketUnix : public NetSocket {

private:
	SOCKET_TYPE _sock;
	IP::Type _ip_type;

	enum NetError {
		ERR_NET_WOULD_BLOCK,
		ERR_NET_IS_CONNECTED,
		ERR_NET_IN_PROGRESS,
		ERR_NET_OTHER
	};

	NetError _get_socket_error();

protected:
	static NetSocket *_create_func();

	size_t _set_address(struct sockaddr_storage *p_addr, const IP_Address &p_ip, uint16_t p_port);
	bool _can_use_ip(const IP_Address p_ip, const bool p_for_bind) const;

public:
	static void make_default();

	virtual Error open(Type p_sock_type, IP::Type &ip_type);
	virtual void close();
	virtual Error bind(IP_Address p_addr, uint16_t p_port);
	virtual Error listen(int p_max_pending);
	virtual Error connect_to_host(IP_Address p_addr, uint16_t p_port);
	virtual Error poll(PollType p_type, int timeout);
	virtual Error recv(uint8_t *p_buffer, int p_len, int &r_read);
	virtual Error recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IP_Address &r_ip, uint16_t &r_port);
	virtual Error send(uint8_t *p_buffer, int p_len, int &r_sent);
	virtual Error sendto(uint8_t *p_buffer, int p_len, int &r_sent, IP_Address p_ip, uint16_t p_port);
	virtual void set_broadcasting_enabled(bool p_enabled);
	virtual void set_blocking_enabled(bool p_enabled);
	virtual void set_ipv6_only_enabled(bool p_enabled);
	virtual void set_tcp_no_delay_enabled(bool p_enabled);
	virtual void set_reuse_address_enabled(bool p_enabled);
	virtual void set_reuse_port_enabled(bool p_enabled);

	NetSocketUnix();
	~NetSocketUnix();
};

#endif
