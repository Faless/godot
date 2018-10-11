/*************************************************************************/
/*  packet_peer_udp.h                                                    */
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

#ifndef PACKET_PEER_UDP_H
#define PACKET_PEER_UDP_H

#include "core/io/ip.h"
#include "core/io/net_socket.h"
#include "core/io/packet_buffer.h"
#include "core/io/packet_peer.h"

class PacketPeerUDP : public PacketPeer {
	GDCLASS(PacketPeerUDP, PacketPeer);

private:
	typedef struct _PacketInfo {
		uint32_t size;
		uint8_t ip[16];
		uint16_t port;
		uint8_t padding[2];
	} PacketInfo;

	void _set_buffers();

protected:

	uint32_t _buffer_shift;
	PacketBuffer<PacketInfo> _in_buffer;
	PoolVector<uint8_t> _packet_buffer;
	PoolVector<uint8_t> _recv_buffer;
	IP_Address packet_ip;
	int packet_port;

	IP_Address peer_addr;
	int peer_port;
	bool blocking;
	Ref<NetSocket> _sock;

	static void _bind_methods();

	String _get_packet_ip() const;

	Error _set_dest_address(const String &p_address, int p_port);
	Error _poll();

public:
	void set_blocking_mode(bool p_enable);

	Error listen(int p_port, const IP_Address &p_bind_address = IP_Address("*"), int p_recv_buffer_size = 0);
	void close();
	Error wait();
	bool is_listening() const;
	IP_Address get_packet_address() const;
	int get_packet_port() const;
	void set_dest_address(const IP_Address &p_address, int p_port);

	Error put_packet(const uint8_t *p_buffer, int p_buffer_size);
	Error get_packet(const uint8_t **r_buffer, int &r_buffer_size);
	int get_available_packet_count() const;
	int get_max_packet_size() const;

	PacketPeerUDP();
	~PacketPeerUDP();
};

#endif // PACKET_PEER_UDP_H
