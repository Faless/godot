/*************************************************************************/
/*  packet_peer_udp.cpp                                                  */
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

#include "packet_peer_udp.h"

#include "core/io/ip.h"

void PacketPeerUDP::set_blocking_mode(bool p_enable) {

	blocking = p_enable;
}

String PacketPeerUDP::_get_packet_ip() const {

	return get_packet_address();
}

Error PacketPeerUDP::_set_dest_address(const String &p_address, int p_port) {

	IP_Address ip;
	if (p_address.is_valid_ip_address()) {
		ip = p_address;
	} else {
		ip = IP::get_singleton()->resolve_hostname(p_address);
		if (!ip.is_valid())
			return ERR_CANT_RESOLVE;
	}

	set_dest_address(ip, p_port);
	return OK;
}

int PacketPeerUDP::get_available_packet_count() const {

	// TODO we should deprecate this, and expose poll instead!
	Error err = const_cast<PacketPeerUDP *>(this)->_poll();
	if (err != OK)
		return -1;

	return _in_buffer.packets_left();
}

Error PacketPeerUDP::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {

	Error err = _poll();
	if (err != OK)
		return err;
	if (_in_buffer.packets_left() == 0)
		return ERR_UNAVAILABLE;

	PacketInfo info = {};
	_in_buffer.read_packet_info(&info);
	PoolVector<uint8_t>::Write rw = _packet_buffer.write();
	_in_buffer.read_packet_payload(rw.ptr(), info.size);
	packet_ip.set_ipv6(info.ip);
	packet_port = info.port;
	*r_buffer = rw.ptr();
	r_buffer_size = info.size;
	return OK;
}

Error PacketPeerUDP::put_packet(const uint8_t *p_buffer, int p_buffer_size) {

	ERR_FAIL_COND_V(!_sock.is_valid(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(!peer_addr.is_valid(), ERR_UNCONFIGURED);

	Error err;
	int sent = -1;

	if (!_sock->is_open()) {
		IP::Type ip_type = peer_addr.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;
		err = _sock->open(NetSocket::TYPE_UDP, ip_type);
		ERR_FAIL_COND_V(err != OK, err);
		_sock->set_blocking_enabled(false);
		_set_buffers();
	}

	do {
		err = _sock->sendto(p_buffer, p_buffer_size, sent, peer_addr, peer_port);
		if (err != OK) {
			if (err != ERR_BUSY)
				return FAILED;
			else if (!blocking)
				return ERR_BUSY;
			// Keep trying to send full packet
			continue;
		}
		return OK;

	} while (sent != p_buffer_size);

	return OK;
}

int PacketPeerUDP::get_max_packet_size() const {

	return 512; // uhm maybe not
}

Error PacketPeerUDP::listen(int p_port, const IP_Address &p_bind_address, int p_recv_buffer_size) {

	ERR_FAIL_COND_V(!_sock.is_valid(), ERR_UNAVAILABLE);
	ERR_FAIL_COND_V(_sock->is_open(), ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(!p_bind_address.is_valid() && !p_bind_address.is_wildcard(), ERR_INVALID_PARAMETER);

	Error err;
	IP::Type ip_type = IP::TYPE_ANY;

	if (p_bind_address.is_valid())
		ip_type = p_bind_address.is_ipv4() ? IP::TYPE_IPV4 : IP::TYPE_IPV6;

	err = _sock->open(NetSocket::TYPE_UDP, ip_type);

	if (err != OK)
		return ERR_CANT_CREATE;

	_sock->set_blocking_enabled(false);
	_sock->set_reuse_address_enabled(true);
	err = _sock->bind(p_bind_address, p_port);

	if (err != OK) {
		_sock->close();
		return err;
	}
	// TODO Deprecate this in favor of buffer_size property
	if (p_recv_buffer_size > 0) {
		_buffer_shift = nearest_shift(p_recv_buffer_size);
	}
	_set_buffers();
	return OK;
}

void PacketPeerUDP::_set_buffers() {
	_in_buffer.set_payload_size(_buffer_shift);
	_in_buffer.set_max_packets(10);
	_recv_buffer.resize((1 << _buffer_shift) - 1);
	_packet_buffer.resize((1 << _buffer_shift) - 1);
}

void PacketPeerUDP::close() {

	if (_sock.is_valid())
		_sock->close();
	_in_buffer.clear();
	_recv_buffer.resize(0);
	_packet_buffer.resize(0);
}

Error PacketPeerUDP::wait() {

	ERR_FAIL_COND_V(!_sock.is_valid(), ERR_UNAVAILABLE);
	return _sock->poll(NetSocket::POLL_TYPE_IN, -1);
}

Error PacketPeerUDP::_poll() {

	ERR_FAIL_COND_V(!_sock.is_valid(), ERR_UNAVAILABLE);

	if (!_sock->is_open()) {
		return FAILED;
	}

	Error err;
	int read;
	IP_Address ip;
	uint16_t port;
	PacketInfo info = {};
	PoolVector<uint8_t>::Write rw = _recv_buffer.write();

	while (true) {
		err = _sock->recvfrom(rw.ptr(), _recv_buffer.size(), read, ip, port);

		if (err != OK) {
			if (err == ERR_BUSY)
				break;
			return FAILED;
		}

		if (_in_buffer.payload_space() < read) {
#ifdef TOOLS_ENABLED
			WARN_PRINT("Buffer payload full! Dropping data");
#endif
			continue;
		}

		info.size = read;
		info.port = port;
		copymem(info.ip, ip.get_ipv6(), 16);
		_in_buffer.write_packet_info(&info);
		_in_buffer.write_packet_payload(rw.ptr(), read);
	}

	return OK;
}
bool PacketPeerUDP::is_listening() const {

	return _sock.is_valid() && _sock->is_open();
}

IP_Address PacketPeerUDP::get_packet_address() const {

	return packet_ip;
}

int PacketPeerUDP::get_packet_port() const {

	return packet_port;
}

void PacketPeerUDP::set_dest_address(const IP_Address &p_address, int p_port) {

	peer_addr = p_address;
	peer_port = p_port;
}

void PacketPeerUDP::_bind_methods() {

	ClassDB::bind_method(D_METHOD("listen", "port", "bind_address", "recv_buf_size"), &PacketPeerUDP::listen, DEFVAL("*"), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("close"), &PacketPeerUDP::close);
	ClassDB::bind_method(D_METHOD("wait"), &PacketPeerUDP::wait);
	ClassDB::bind_method(D_METHOD("is_listening"), &PacketPeerUDP::is_listening);
	ClassDB::bind_method(D_METHOD("get_packet_ip"), &PacketPeerUDP::_get_packet_ip);
	ClassDB::bind_method(D_METHOD("get_packet_port"), &PacketPeerUDP::get_packet_port);
	ClassDB::bind_method(D_METHOD("set_dest_address", "host", "port"), &PacketPeerUDP::_set_dest_address);
}

PacketPeerUDP::PacketPeerUDP() {

	_sock = Ref<NetSocket>(NetSocket::create());
	_buffer_shift = 16;
	blocking = true;
	packet_port = 0;
	peer_port = 0;
}

PacketPeerUDP::~PacketPeerUDP() {

	close();
}
