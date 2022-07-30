/*************************************************************************/
/*  stream_peer_compression.h                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
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

#ifndef STREAM_PEER_COMPRESSION_H
#define STREAM_PEER_COMPRESSION_H

#include "core/io/stream_peer.h"

#include "core/core_bind.h"
#include "core/io/compression.h"
#include "core/templates/ring_buffer.h"

class StreamCompressionContext : public RefCounted {
	GDCLASS(StreamCompressionContext, RefCounted);

public:
	virtual bool is_compressing() const = 0;
	virtual Error update(uint8_t *p_dst, int p_dst_size, const uint8_t *p_src, int p_src_size, int &r_consumed, int &r_out) = 0;
	virtual Error finish() = 0;

	StreamCompressionContext() {}
	virtual ~StreamCompressionContext() {}
};

class StreamPeerCompressor : public StreamPeer {
	GDCLASS(StreamPeerCompressor, StreamPeer);

private:
	using CompressionMode = core_bind::File::CompressionMode;

	RingBuffer<uint8_t> rb;
	Vector<uint8_t> buffer;
	Ref<StreamCompressionContext> ctx;

	Error _make_ctx(Ref<StreamCompressionContext> &ctx, CompressionMode p_mode, bool p_compress, int buffer_size);

protected:
	static void _bind_methods();

public:
	Error start(bool p_compress, core_bind::File::CompressionMode p_mode, int buffer_size);

	virtual Error put_data(const uint8_t *p_data, int p_bytes) override;
	virtual Error put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) override;

	virtual Error get_data(uint8_t *p_buffer, int p_bytes) override;
	virtual Error get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) override;

	virtual int get_available_bytes() const override;

	Error finish();

	StreamPeerCompressor() {}
};

#endif // STREAM_PEER_DECOMPRESSOR_H
