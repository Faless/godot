/*************************************************************************/
/*  lws_peer.h                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#ifndef WSLPEER_H
#define WSLPEER_H

#ifndef JAVASCRIPT_ENABLED

#include "core/error_list.h"
#include "core/io/packet_peer.h"
#include "core/ring_buffer.h"
#include "packet_buffer.h"
#include "websocket_peer.h"
#include "wslay/wslay.h"

#ifndef LWS_PRE
#define LWS_PRE 0
#endif

class WSLPeer : public WebSocketPeer {

	GDCIIMPL(WSLPeer, WebSocketPeer);

public:
	struct PeerData {
		bool polling;
		bool destroy;
		bool valid;
		void *obj;
		void *peer;
		StreamPeer *conn;
		wslay_event_context_ptr ctx;

		PeerData() {
			polling = false;
			destroy = false;
			valid = false;
			ctx = NULL;
			obj = NULL;
			peer = NULL;
			conn = NULL;
		}
	};

private:
	static bool _wsl_poll(struct PeerData *p_data);
	static void _wsl_destroy(struct PeerData **p_data);

	Ref<StreamPeer> _connection;
	struct PeerData *_data;
	uint8_t _is_string;
	// Our packet info is just a boolean (is_string), using uint8_t for it.
	PacketBuffer<uint8_t> _in_buffer;

	PoolVector<uint8_t> _packet_buffer;

	WriteMode write_mode;

public:
	int close_code;
	String close_reason;
	void poll(); // Used by client and server.

	virtual int get_available_packet_count() const;
	virtual Error get_packet(const uint8_t **r_buffer, int &r_buffer_size);
	virtual Error put_packet(const uint8_t *p_buffer, int p_buffer_size);
	virtual int get_max_packet_size() const { return _packet_buffer.size(); };

	virtual void close(int p_code = 1000, String p_reason = "");
	virtual bool is_connected_to_host() const;
	virtual IP_Address get_connected_host() const;
	virtual uint16_t get_connected_port() const;

	virtual WriteMode get_write_mode() const;
	virtual void set_write_mode(WriteMode p_mode);
	virtual bool was_string_packet() const;

	void make_context(void *p_obj, Ref<StreamPeer> connection, unsigned int p_in_buf_size, unsigned int p_in_pkt_size, unsigned int p_out_buf_size, unsigned int p_out_pkt_size);
	Error parse_message(const wslay_event_on_msg_recv_arg *arg);
	void invalidate();

	WSLPeer();
	~WSLPeer();
};

#endif // JAVASCRIPT_ENABLED

#endif // LSWPEER_H
