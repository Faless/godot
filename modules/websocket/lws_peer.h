/*************************************************************************/
/*  lws_peer.h                                                           */
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
#ifndef LWSPEER_H
#define LWSPEER_H

#ifndef JAVASCRIPT_ENABLED

#include "core/error_list.h"
#include "core/io/packet_peer.h"
#include "core/ring_buffer.h"
#include "libwebsockets.h"
#include "lws_config.h"
#include "websocket_peer.h"
#include "packet_buffer.h"

class LWSPeer : public WebSocketPeer {

	GDCIIMPL(LWSPeer, WebSocketPeer);

private:
	enum {
		PACKET_BUFFER_SIZE = 65536 - 5 // 4 bytes for the size, 1 for the type
	};

	typedef struct _PacketInfo {
		uint32_t size;
		uint8_t is_string;
		uint8_t padding[3]; // Align to 8th byte
	} PacketInfo;

	PacketBuffer<PacketInfo> _in_buffer;
	PacketBuffer<PacketInfo> _out_buffer;
	PacketInfo _in_info;
	PacketInfo _current_info;
	PoolVector<uint8_t> _packet_buffer;

	struct lws *wsi;
	WriteMode write_mode;

	int close_code;
	String close_reason;

public:
	struct PeerData {
		uint32_t peer_id;
		bool force_close;
		bool clean_close;
	};

	virtual int get_available_packet_count() const;
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size);
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size);
	virtual int get_max_packet_size() const { return PACKET_BUFFER_SIZE; };

	virtual void close(int p_code = 1000, String p_reason = "");
	virtual bool is_connected_to_host() const;
	virtual IP_Address get_connected_host() const;
	virtual uint16_t get_connected_port() const;

	virtual WriteMode get_write_mode() const;
	virtual void set_write_mode(WriteMode p_mode);
	virtual bool was_string_packet() const;

	void set_wsi(struct lws *wsi, unsigned int _buffer_shift, unsigned int _max_packets_shift);
	Error read_wsi(void *in, size_t len);
	Error write_wsi();
	void send_close_status(struct lws *wsi);
	String get_close_reason(void *in, size_t len, int &r_code);

	LWSPeer();
	~LWSPeer();
};

#endif // JAVASCRIPT_ENABLED

#endif // LSWPEER_H
