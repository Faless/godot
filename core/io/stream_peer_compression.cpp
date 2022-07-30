/*************************************************************************/
/*  stream_peer_compression.cpp                                          */
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

#include "core/io/stream_peer_compression.h"

#include "core/io/zip_io.h"

#include "thirdparty/misc/fastlz.h"

#include <zlib.h>
#include <zstd.h>

#define MAKE_ROOM(buf, buf_size) \
	if (buf.size() < buf_size) { \
		buf.resize(buf_size);    \
	}

class StreamGZIPContext : public StreamCompressionContext {
protected:
	z_stream strm;
	bool compressing;
	bool closing = false;

public:
	StreamGZIPContext(bool p_is_deflate, bool p_compress) {
		compressing = p_compress;
		strm.next_in = Z_NULL;
		strm.avail_in = 0;
		strm.zalloc = zipio_alloc;
		strm.zfree = zipio_free;
		strm.opaque = Z_NULL;
		int window_bits = p_is_deflate ? 15 : (15 + 16);
		int err = Z_OK;
		int level = Z_DEFAULT_COMPRESSION;
		if (compressing) {
			err = deflateInit2(&strm, level, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY);
		} else {
			err = inflateInit2(&strm, window_bits);
		}
		ERR_FAIL_COND(!err == Z_OK);
	}

	~StreamGZIPContext() {
		if (compressing) {
			deflateEnd(&strm);
		} else {
			inflateEnd(&strm);
		}
	}

	bool is_compressing() const override {
		return compressing;
	}

	Error update(uint8_t *p_dst, int p_dst_size, const uint8_t *p_src, int p_src_size, int &r_consumed, int &r_out) override {
		strm.avail_in = p_src_size;
		strm.avail_out = p_dst_size;
		strm.next_in = (Bytef *)p_src;
		strm.next_out = (Bytef *)p_dst;
		int flush = closing ? Z_FINISH : Z_NO_FLUSH;
		if (compressing) {
			int err = deflate(&strm, flush);
			ERR_FAIL_COND_V(err != (closing ? Z_STREAM_END : Z_OK), FAILED);
		} else {
			int err = inflate(&strm, flush);
			ERR_FAIL_COND_V(err != Z_OK && err != Z_STREAM_END, FAILED);
		}
		r_out = p_dst_size - strm.avail_out;
		r_consumed = p_src_size - strm.avail_in;
		return OK;
	}

	virtual Error finish() override {
		ERR_FAIL_COND_V(closing, FAILED);
		closing = true;
		return OK;
	}
};

void StreamPeerCompressor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "compress", "compression", "buffer_size"), &StreamPeerCompressor::start, DEFVAL(core_bind::File::COMPRESSION_ZSTD), DEFVAL(65535));
	ClassDB::bind_method(D_METHOD("finish"), &StreamPeerCompressor::finish);
}

Error StreamPeerCompressor::_make_ctx(Ref<StreamCompressionContext> &ctx, CompressionMode p_mode, bool p_compress, int buffer_size) {
	Compression::Mode mode = (Compression::Mode)p_mode;
	switch (mode) {
		case Compression::MODE_FASTLZ: {
			// TODO
			ERR_FAIL_V(ERR_UNAVAILABLE);
		} break;
		case Compression::MODE_DEFLATE:
		case Compression::MODE_GZIP: {
			ctx = Ref<StreamCompressionContext>(memnew(StreamGZIPContext(mode == Compression::MODE_DEFLATE, p_compress)));
		} break;
		case Compression::MODE_ZSTD: {
			// TODO
			ERR_FAIL_V(ERR_UNAVAILABLE);
		} break;
		default: {
			ERR_FAIL_V(ERR_INVALID_PARAMETER);
		} break;
	}
	return OK;
}

Error StreamPeerCompressor::start(bool p_compress, CompressionMode p_mode, int buffer_size) {
	ERR_FAIL_COND_V(ctx.is_valid(), ERR_ALREADY_IN_USE);
	rb.clear();
	rb.resize(nearest_shift(buffer_size - 1));
	return _make_ctx(ctx, p_mode, p_compress, buffer_size);
}

Error StreamPeerCompressor::put_data(const uint8_t *p_data, int p_bytes) {
	int wrote = 0;
	Error err = put_partial_data(p_data, p_bytes, wrote);
	if (err != OK) {
		return err;
	}
	ERR_FAIL_COND_V(p_bytes != wrote, ERR_OUT_OF_MEMORY);
	return OK;
}

Error StreamPeerCompressor::put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) {
	ERR_FAIL_COND_V(ctx.is_null(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_bytes < 0, ERR_INVALID_PARAMETER);

	MAKE_ROOM(buffer, p_bytes);
	if (ctx->is_compressing()) {
		int to_write = 0;
		Error err = ctx->update(buffer.ptrw(), p_bytes, p_data, p_bytes, r_sent, to_write);
		if (err != OK) {
			return err;
		}
		// Copy to ring buffer.
		int wrote = rb.write(buffer.ptr(), to_write);
		ERR_FAIL_COND_V(wrote != to_write, ERR_OUT_OF_MEMORY);
		return OK;
	} else {
		r_sent = 0;
		while (rb.space_left() && r_sent < p_bytes) {
			int sent = 0;
			int to_write = 0;
			Error err = ctx->update(buffer.ptrw(), p_bytes, p_data + r_sent, p_bytes - r_sent, sent, to_write);
			if (err != OK) {
				return err;
			}
			if (to_write > rb.space_left()) {
				return OK; // We can't write more than this buffer is full.
			}
			int wrote = rb.write(buffer.ptr(), to_write);
			ERR_FAIL_COND_V(wrote != to_write, ERR_BUG);
			r_sent += sent;
		}
		return OK;
	}
}

Error StreamPeerCompressor::get_data(uint8_t *p_buffer, int p_bytes) {
	int received = 0;
	Error err = get_partial_data(p_buffer, p_bytes, received);
	if (err != OK) {
		return err;
	}
	ERR_FAIL_COND_V(p_bytes != received, ERR_OUT_OF_MEMORY);
	return OK;
}

Error StreamPeerCompressor::get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) {
	ERR_FAIL_COND_V(ctx.is_null(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_bytes < 0, ERR_INVALID_PARAMETER);

	r_received = MIN(p_bytes, rb.data_left());
	if (r_received == 0) {
		return OK;
	}
	int received = rb.read(p_buffer, r_received);
	ERR_FAIL_COND_V(received != r_received, ERR_BUG);
	return OK;
}

int StreamPeerCompressor::get_available_bytes() const {
	return rb.data_left();
}

Error StreamPeerCompressor::finish() {
	ERR_FAIL_COND_V(ctx.is_null() || !ctx->is_compressing(), ERR_UNAVAILABLE);
	MAKE_ROOM(buffer, 1024); // should be more than enough.
	ctx->finish(); // mark as finished.
	int consumed = 0;
	int to_write = 0;
	Error err = ctx->update(buffer.ptrw(), 1024, nullptr, 0, consumed, to_write); // compress
	if (err != OK) {
		return err;
	}
	int wrote = rb.write(buffer.ptr(), to_write);
	ERR_FAIL_COND_V(wrote != to_write, ERR_OUT_OF_MEMORY);
	return OK;
}
