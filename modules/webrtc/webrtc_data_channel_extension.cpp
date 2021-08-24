/*************************************************************************/
/*  webrtc_data_channel_gdnative.cpp                                     */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
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

#ifdef WEBRTC_EXTENSION_ENABLED

#include "webrtc_data_channel_extension.h"

#include "core/io/resource_loader.h"

void WebRTCDataChannelExtension::_bind_methods() {
	ADD_PROPERTY_DEFAULT("write_mode", WRITE_MODE_BINARY);
	GDVIRTUAL(_set_write_mode);
	GDVIRTUAL(_get_write_mode);
	GDVIRTUAL(_was_string_packet);

	GDVIRTUAL(_get_ready_state);
	GDVIRTUAL(_get_label);
	GDVIRTUAL(_is_ordered);
	GDVIRTUAL(_get_id);
	GDVIRTUAL(_get_max_packet_life_time);
	GDVIRTUAL(_get_max_retransmits);
	GDVIRTUAL(_get_protocol);
	GDVIRTUAL(_is_negotiated);
	GDVIRTUAL(_get_buffered_amount);

	GDVIRTUAL(_poll);
	GDVIRTUAL(_close);
}

Error WebRTCDataChannelExtension::poll() {
	Error err;
	if (GDVIRTUAL_CALL(_poll, err)) {
		return err;
	}
	return ERR_UNCONFIGURED;
}

void WebRTCDataChannelExtension::close() {
	GDVIRTUAL_CALL(_close);
}

void WebRTCDataChannelExtension::set_write_mode(WriteMode p_mode) {
	GDVIRTUAL_CALL(_set_write_mode);
}

WebRTCDataChannel::WriteMode WebRTCDataChannelExtension::get_write_mode() const {
	WebRTCDataChannel::WriteMode mode;
	if (GDVIRTUAL_CALL(_get_write_mode, mode)) {
		return mode;
	}
	return WRITE_MODE_BINARY;
}

bool WebRTCDataChannelExtension::was_string_packet() const {
	bool was_string;
	if (GDVIRTUAL_CALL(_was_string_packet, was_string)) {
		return was_string;
	}
	return false;
}

WebRTCDataChannel::ChannelState WebRTCDataChannelExtension::get_ready_state() const {
	WebRTCDataChannel::ChannelState state;
	if (GDVIRTUAL_CALL(_get_ready_state, state)) {
		return state;
	}
	return STATE_CLOSED;
}

String WebRTCDataChannelExtension::get_label() const {
	String label;
	if (GDVIRTUAL_CALL(_get_label, label)) {
		return label;
	}
	return "";
}

bool WebRTCDataChannelExtension::is_ordered() const {
	bool ordered;
	if (GDVIRTUAL_CALL(_is_ordered, ordered)) {
		return ordered;
	}
	return false;
}

int WebRTCDataChannelExtension::get_id() const {
	int id;
	if (GDVIRTUAL_CALL(_get_id, id)) {
		return id;
	}
	return -1;
}

int WebRTCDataChannelExtension::get_max_packet_life_time() const {
	int lifetime;
	if (GDVIRTUAL_CALL(_get_max_packet_life_time, lifetime)) {
		return lifetime;
	}
	return -1;
}

int WebRTCDataChannelExtension::get_max_retransmits() const {
	int retransmits;
	if (GDVIRTUAL_CALL(_get_max_retransmits, retransmits)) {
		return retransmits;
	}
	return -1;
}

String WebRTCDataChannelExtension::get_protocol() const {
	String proto;
	if (GDVIRTUAL_CALL(_get_protocol, proto)) {
		return proto;
	}
	return "";
}

bool WebRTCDataChannelExtension::is_negotiated() const {
	bool negotiated;
	if (GDVIRTUAL_CALL(_is_negotiated, negotiated)) {
		return negotiated;
	}
	return false;
}

int WebRTCDataChannelExtension::get_buffered_amount() const {
	int buffered;
	if (GDVIRTUAL_CALL(_get_buffered_amount, amount)) {
		return amount;
	}
	return 0;
}

Error WebRTCDataChannelExtension::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	// TODO
	return ERR_UNCONFIGURED;
}

Error WebRTCDataChannelExtension::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	// TODO
	return ERR_UNCONFIGURED;
}

int WebRTCDataChannelExtension::get_max_packet_size() const {
	int size;
	if (GDVIRTUAL_CALL(_get_max_packet_size, size)) {
		return size;
	}
	return 0;
}

int WebRTCDataChannelExtension::get_available_packet_count() const {
	int count;
	if (GDVIRTUAL_CALL(_get_available_packet_count, count)) {
		return count;
	}
	return 0;
}

//void WebRTCDataChannelExtension::set_native_webrtc_data_channel(const godot_net_webrtc_data_channel *p_impl) {
//	interface = p_impl;
//}

#endif // WEBRTC_EXTENSION_ENABLED
