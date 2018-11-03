/*************************************************************************/
/*  packet_buffer.h                                                      */
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

#ifndef PACKET_BUFFER_H
#define PACKET_BUFFER_H

#include "core/ring_buffer.h"

template <class T>
class PacketBuffer {

private:
	RingBuffer<T> _info;
	RingBuffer<uint8_t> _payload;

public:
	Error write_packet_info(const T *p_info) {
		ERR_FAIL_COND_V(_info.space_left() < 1, ERR_OUT_OF_MEMORY);
		_info.write(p_info, 1);
		return OK;
	}

	Error write_packet_payload(const void *p_payload, int p_bytes) {
		ERR_FAIL_COND_V(_payload.space_left() < p_bytes, ERR_OUT_OF_MEMORY);
		_payload.write((const uint8_t *)p_payload, p_bytes);
		return OK;
	}

	Error read_packet_info(T *r_info) {
		ERR_FAIL_COND_V(_info.data_left() < 1, ERR_UNAVAILABLE);
		_info.read(r_info, 1);
		return OK;
	}

	Error read_packet_payload(uint8_t *r_payload, int p_bytes) {
		ERR_FAIL_COND_V(_payload.data_left() < p_bytes, ERR_UNAVAILABLE);
		_payload.read(r_payload, p_bytes);
		return OK;
	}

	void set_payload_size(int p_shift) {
		_payload.resize(p_shift);
	}

	void set_max_packets(int p_shift) {
		_info.resize(p_shift);
	}

	int packets_left() const {
		return _info.data_left();
	}

	int packets_space() const {
		return _info.space_left();
	}

	int payload_left() const {
		return _payload.data_left();
	}

	int payload_space() const {
		return _payload.space_left();
	}

	void clear() {
		_info.resize(0);
		_payload.resize(0);
	}

	PacketBuffer() {
		clear();
	}

	~PacketBuffer() {
		clear();
	}
};

#endif // PACKET_BUFFER_H
