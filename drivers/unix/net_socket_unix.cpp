/*************************************************************************/
/*  net_socket_unix.cpp                                                  */
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

#include "net_socket_unix.h"

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

// Some custom defines to minimize ifdefs
#define SOCK_EMPTY -1
#define SOCK_BUF(x) x
#define SOCK_CBUF(x) x
#define SOCK_POLL ::poll
#define SOCK_CLOSE ::close

/* Windows */
#elif defined(WINDOWS_ENABLED)
#include <winsock2.h>
#include <ws2tcpip.h>
// Some custom defines to minimize ifdefs
#define SOCK_EMPTY INVALID_SOCKET
#define SOCK_BUF(x) (char *)(x)
#define SOCK_CBUF(x) (const char *)(x)
#define SOCK_POLL WSAPoll
#define SOCK_CLOSE closesocket

#endif

size_t _set_addr_storage(struct sockaddr_storage *p_addr, const IP_Address &p_ip, uint16_t p_port, IP::Type p_ip_type) {

	memset(p_addr, 0, sizeof(struct sockaddr_storage));
	if (p_ip_type == IP::TYPE_IPV6 || p_ip_type == IP::TYPE_ANY) { // IPv6 socket

		// IPv6 only socket with IPv4 address
		ERR_FAIL_COND_V(!p_ip.is_wildcard() && p_ip_type == IP::TYPE_IPV6 && p_ip.is_ipv4(), 0);

		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)p_addr;
		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = htons(p_port);
		if (p_ip.is_valid()) {
			copymem(&addr6->sin6_addr.s6_addr, p_ip.get_ipv6(), 16);
		} else {
			addr6->sin6_addr = in6addr_any;
		}
		return sizeof(sockaddr_in6);
	} else { // IPv4 socket

		// IPv4 socket with IPv6 address
		ERR_FAIL_COND_V(!p_ip.is_ipv4(), 0);

		struct sockaddr_in *addr4 = (struct sockaddr_in *)p_addr;
		addr4->sin_family = AF_INET;
		addr4->sin_port = htons(p_port); // short, network byte order

		if (p_ip.is_valid()) {
			copymem(&addr4->sin_addr.s_addr, p_ip.get_ipv4(), 4);
		} else {
			addr4->sin_addr.s_addr = INADDR_ANY;
		}

		copymem(&addr4->sin_addr.s_addr, p_ip.get_ipv4(), 16);
		return sizeof(sockaddr_in);
	}
}

NetSocket *NetSocketUnix::_create_func() {
	return new NetSocketUnix();
}

void NetSocketUnix::make_default() {
	_create = _create_func;
}

NetSocketUnix::NetSocketUnix() {
	_sock = SOCK_EMPTY;
	_ip_type = IP::TYPE_NONE;
}

NetSocketUnix::~NetSocketUnix() {
	close();
}

NetSocketUnix::NetError NetSocketUnix::_get_socket_error() {
#if defined(WINDOWS_ENABLED)
	int err = WSAGetLastError();

	if (err == WSAEISCONN)
		return ERR_NET_IS_CONNECTED;
	if (err == WSAEINPROGRESS || errno == WSAEALREADY)
		return ERR_NET_IN_PROGRESS;
	if (err == WSAEWOULDBLOCK)
		return ERR_NET_WOULD_BLOCK;
	return ERR_NET_OTHER;
#else
	if (errno == EISCONN)
		return ERR_NET_IS_CONNECTED;
	if (errno == EINPROGRESS || errno == EALREADY)
		return ERR_NET_IN_PROGRESS;
	if (errno == EAGAIN || errno == EWOULDBLOCK)
		return ERR_NET_WOULD_BLOCK;
	return ERR_NET_OTHER;
#endif
}

bool NetSocketUnix::_can_use_ip(const IP_Address p_ip, const bool p_for_bind) const {

	if (p_for_bind && !(p_ip.is_valid() || p_ip.is_wildcard())) {
		return false;
	} else if (!p_for_bind && !p_ip.is_valid()) {
		return false;
	}
	// Check if socket support this IP type.
	IP::Type type = p_ip.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;
	if (_ip_type != IP::TYPE_ANY && !p_ip.is_wildcard() && _ip_type != type) {
		return false;
	}
	return true;
}

Error NetSocketUnix::open(Type p_sock_type, IP::Type &ip_type) {
	ERR_FAIL_COND_V(ip_type > IP::TYPE_ANY || ip_type < IP::TYPE_NONE, ERR_INVALID_PARAMETER);

#if defined(__OpenBSD__)
	// OpenBSD does not support dual stacking, fallback to IPv4 only.
	if (ip_type == IP::TYPE_ANY)
		ip_type = IP::TYPE_IPV4;
#endif

	int family = ip_type == IP::TYPE_IPV4 ? AF_INET : AF_INET6;
	int protocol = p_sock_type == TYPE_TCP ? IPPROTO_TCP : IPPROTO_UDP;
	int type = p_sock_type == TYPE_TCP ? SOCK_STREAM : SOCK_DGRAM;
	_sock = socket(family, type, protocol);

	if (_sock == SOCK_EMPTY && ip_type == IP::TYPE_ANY) {
		// Careful here, changing the referenced parameter so the caller knows that we are using an IPv4 socket
		// in place of a dual stack one, and further calls to _set_sock_addr will work as expected.
		ip_type = IP::TYPE_IPV4;
		family = AF_INET;
		_sock = socket(family, type, protocol);
	}

	ERR_FAIL_COND_V(_sock == SOCK_EMPTY, FAILED);
	_ip_type = ip_type;

	if (family == AF_INET6) {
		// Select IPv4 over IPv6 mapping
		set_ipv6_only_enabled(ip_type != IP::TYPE_ANY);
	}

	if (protocol == IPPROTO_UDP && ip_type != IP::TYPE_IPV6) {
		// Enable broadcasting for UDP sockets if it's not IPv6 only (IPv6 has no broadcast option).
		set_broadcasting_enabled(true);
	}

	return OK;
}

void NetSocketUnix::close() {

	if (_sock != SOCK_EMPTY)
		SOCK_CLOSE(_sock);

	_sock = SOCK_EMPTY;
	_ip_type = IP::TYPE_NONE;
}

Error NetSocketUnix::bind(IP_Address p_addr, uint16_t p_port) {

	ERR_FAIL_COND_V(_ip_type == IP::TYPE_NONE, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(!_can_use_ip(p_addr, true), ERR_INVALID_PARAMETER);

	sockaddr_storage addr;
	size_t addr_size = _set_addr_storage(&addr, p_addr, p_port, _ip_type);

	if (::bind(_sock, (struct sockaddr *)&addr, addr_size) == SOCK_EMPTY) {
		close();
		ERR_FAIL_V(ERR_UNAVAILABLE);
	}

	return OK;
}

Error NetSocketUnix::listen(int p_max_pending) {
	ERR_FAIL_COND_V(_ip_type == IP::TYPE_NONE, ERR_UNCONFIGURED);

	if (::listen(_sock, p_max_pending) == SOCK_EMPTY) {

		close();
		ERR_FAIL_V(FAILED);
	};

	return OK;
}

Error NetSocketUnix::connect_to_host(IP_Address p_host, uint16_t p_port) {

	ERR_FAIL_COND_V(_ip_type == IP::TYPE_NONE, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(!_can_use_ip(p_host, false), ERR_INVALID_PARAMETER);

	struct sockaddr_storage addr;
	size_t addr_size = _set_addr_storage(&addr, p_host, p_port, _ip_type);

	if (::connect(_sock, (struct sockaddr *)&addr, addr_size) == SOCK_EMPTY) {

		NetError err = _get_socket_error();

		switch (err) {
			// We are already connected
			case ERR_NET_IS_CONNECTED:
				return OK;
			// Still waiting to connect, try again in a while
			case ERR_NET_WOULD_BLOCK:
			case ERR_NET_IN_PROGRESS:
				return ERR_BUSY;
			default:
				ERR_PRINT("Connection to remote host failed!");
				close();
				return FAILED;
		}
	}

	return OK;
}

Error NetSocketUnix::poll(PollType p_type, int timeout) {

	ERR_FAIL_COND_V(_ip_type == IP::TYPE_NONE, ERR_UNCONFIGURED);

	struct pollfd pfd;
	pfd.fd = _sock;
	pfd.events = POLLIN;
	pfd.revents = 0;

	switch (p_type) {
		case POLL_TYPE_IN:
			pfd.events = POLLIN;
			break;
		case POLL_TYPE_OUT:
			pfd.events = POLLOUT;
			break;
		case POLL_TYPE_IN_OUT:
			pfd.events = POLLOUT || POLLIN;
	}

	int ret = SOCK_POLL(&pfd, timeout, 0);

	ERR_FAIL_COND_V(ret < 0, FAILED);

	if (ret == 0)
		return ERR_BUSY;

	return OK;
}

Error NetSocketUnix::recv(uint8_t *p_buffer, int p_len, int &r_read) {
	ERR_FAIL_COND_V(_ip_type == IP::TYPE_NONE, ERR_UNCONFIGURED);

	r_read = ::recv(_sock, SOCK_BUF(p_buffer), p_len, 0);

	if (r_read < 0) {
		NetError err = _get_socket_error();
		if (err == ERR_NET_WOULD_BLOCK)
			return ERR_BUSY;

		return FAILED;
	}

	return OK;
}

Error NetSocketUnix::recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IP_Address &r_ip, uint16_t &r_port) {
	ERR_FAIL_COND_V(_ip_type == IP::TYPE_NONE, ERR_UNCONFIGURED);

	struct sockaddr_storage from;
	socklen_t len = sizeof(struct sockaddr_storage);
	memset(&from, 0, len);

	r_read = ::recvfrom(_sock, SOCK_BUF(p_buffer), p_len, 0, (struct sockaddr *)&from, &len);

	if (r_read < 0) {
		NetError err = _get_socket_error();
		if (err == ERR_NET_WOULD_BLOCK)
			return ERR_BUSY;

		return FAILED;
	}

	if (from.ss_family == AF_INET) {
		struct sockaddr_in *sin_from = (struct sockaddr_in *)&from;
		r_ip.set_ipv4((uint8_t *)&sin_from->sin_addr);
		r_port = ntohs(sin_from->sin_port);
	} else if (from.ss_family == AF_INET6) {
		struct sockaddr_in6 *s6_from = (struct sockaddr_in6 *)&from;
		r_ip.set_ipv6((uint8_t *)&s6_from->sin6_addr);
		r_port = ntohs(s6_from->sin6_port);
	} else {
		// Unsupported socket family, should never happen.
		ERR_FAIL_V(FAILED);
	}

	return OK;
}

Error NetSocketUnix::send(const uint8_t *p_buffer, int p_len, int &r_sent) {
	ERR_FAIL_COND_V(_ip_type == IP::TYPE_NONE, ERR_UNCONFIGURED);

	r_sent = ::send(_sock, SOCK_CBUF(p_buffer), p_len, 0);

	if (r_sent < 0) {
		NetError err = _get_socket_error();
		if (err == ERR_NET_WOULD_BLOCK)
			return ERR_BUSY;

		return FAILED;
	}

	return OK;
}

Error NetSocketUnix::sendto(const uint8_t *p_buffer, int p_len, int &r_sent, IP_Address p_ip, uint16_t p_port) {
	ERR_FAIL_COND_V(_ip_type == IP::TYPE_NONE, ERR_UNCONFIGURED);

	struct sockaddr_storage addr;
	size_t addr_size = _set_addr_storage(&addr, p_ip, p_port, _ip_type);
	r_sent = ::sendto(_sock, SOCK_CBUF(p_buffer), p_len, 0, (struct sockaddr *)&addr, addr_size);

	if (r_sent < 0) {
		NetError err = _get_socket_error();
		if (err == ERR_NET_WOULD_BLOCK)
			return ERR_BUSY;

		return FAILED;
	}

	return OK;
}

void NetSocketUnix::set_broadcasting_enabled(bool p_enabled) {
	ERR_FAIL_COND(_sock == SOCK_EMPTY);
	// IPv6 has no broadcast support.
	ERR_FAIL_COND(_ip_type == IP::TYPE_IPV6);

	int par = p_enabled ? 1 : 0;
	if (setsockopt(_sock, SOL_SOCKET, SO_BROADCAST, SOCK_CBUF(&par), sizeof(int)) != 0) {
		WARN_PRINT("Unable to change broadcast setting");
	}
}

void NetSocketUnix::set_blocking_enabled(bool p_enabled) {
	ERR_FAIL_COND(_sock == SOCK_EMPTY);

	int ret = 0;
#if defined(WINDOWS_ENABLED)
	unsigned long par = p_enabled ? 0 : 1;
	ret = ioctlsocket(_sock, FIONBIO, &par);
#elif defined(NO_FCNTL)
	int par = p_enabled ? 0 : 1;
	ret = ioctl(_sock, FIONBIO, &par);
#else
	int opts = fcntl(_sock, F_GETFL);
	if (p_enabled)
		ret = fcntl(_sock, F_SETFL, opts & ~O_NONBLOCK);
	else
		ret = fcntl(_sock, F_SETFL, opts | O_NONBLOCK);
#endif

	if (ret != 0)
		WARN_PRINT("Unable to change non-block mode");
}

void NetSocketUnix::set_ipv6_only_enabled(bool p_enabled) {
	ERR_FAIL_COND(_sock == SOCK_EMPTY);
	// This option is only avaiable in IPv6 sockets.
	ERR_FAIL_COND(_ip_type == IP::TYPE_IPV4);

	int par = p_enabled ? 1 : 0;
	if (setsockopt(_sock, IPPROTO_IPV6, IPV6_V6ONLY, SOCK_CBUF(&par), sizeof(int)) != 0) {
		WARN_PRINT("Unable to change IPv4 address mapping over IPv6 option");
	}
}

void NetSocketUnix::set_tcp_no_delay_enabled(bool p_enabled) {
	ERR_FAIL_COND(_sock == SOCK_EMPTY);
	ERR_FAIL_COND(_ip_type != TYPE_TCP);

	int par = p_enabled ? 1 : 0;
	if (setsockopt(_sock, IPPROTO_TCP, TCP_NODELAY, SOCK_CBUF(&par), sizeof(int)) < 0) {
		ERR_PRINT("Unable to set TCP no delay option");
	}
}

void NetSocketUnix::set_reuse_address_enabled(bool p_enabled) {
	ERR_FAIL_COND(_sock == SOCK_EMPTY);

	int par = p_enabled ? 1 : 0;
	if (setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, SOCK_CBUF(&par), sizeof(int)) < 0) {
		WARN_PRINT("Unable to set socket REUSEADDR option!");
	}
}

void NetSocketUnix::set_reuse_port_enabled(bool p_enabled) {
// Windows does not have this option, as it is always ON when setting REUSEADDR.
#ifndef WINDOWS_ENABLED
	ERR_FAIL_COND(_sock == SOCK_EMPTY);

	int par = p_enabled ? 1 : 0;
	if (setsockopt(_sock, SOL_SOCKET, SO_REUSEPORT, SOCK_CBUF(&par), sizeof(int)) < 0) {
		WARN_PRINT("Unable to set socket REUSEPORT option!");
	}
#endif
}

bool NetSocketUnix::is_open() const {
	return _sock != SOCK_EMPTY;
}
