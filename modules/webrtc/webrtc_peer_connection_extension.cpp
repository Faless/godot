/*************************************************************************/
/*  webrtc_peer_connection_gdnative.cpp                                  */
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

#include "webrtc_peer_connection_extension.h"

#include "core/io/resource_loader.h"
#include "webrtc_data_channel_extension.h"

//const godot_net_webrtc_library *WebRTCPeerConnectionExtension::default_library = nullptr;

//Error WebRTCPeerConnectionExtension::set_default_library(const godot_net_webrtc_library *p_lib) {
//	if (default_library) {
//		const godot_net_webrtc_library *old = default_library;
//		default_library = nullptr;
//		old->unregistered();
//	}
//	default_library = p_lib;
//	return OK; // Maybe add version check and fail accordingly
//}

WebRTCPeerConnection *WebRTCPeerConnectionExtension::_create() {
	return memnew(WebRTCPeerConnectionExtension);
}

void WebRTCPeerConnectionExtension::_bind_methods() {
	GDVIRTUAL_BIND(_get_connection_state);
	GDVIRTUAL_BIND(_initialize);
	GDVIRTUAL_BIND(_create_data_channel);
	GDVIRTUAL_BIND(_create_offer);
	GDVIRTUAL_BIND(_set_remote_description);
	GDVIRTUAL_BIND(_set_local_description);
	GDVIRTUAL_BIND(_add_ice_candidate);
	GDVIRTUAL_BIND(_poll);
	GDVIRTUAL_BIND(_close);
}

Error WebRTCPeerConnectionExtension::initialize(Dictionary p_config) {
	Error err;
	if (GDVIRTUAL_CALL(_initialize, p_config, err)) {
		return err;
	}
	return ERR_UNCONFIGURED;
}

Ref<WebRTCDataChannel> WebRTCPeerConnectionExtension::create_data_channel(String p_label, Dictionary p_options) {
	Ref<WebRTCDataChannel> channel;
	if (GDVIRTUAL_CALL(_create_data_channel, p_label, p_options, channel)) {
		return channel;
	}
	return nullptr;
}

Error WebRTCPeerConnectionExtension::create_offer() {
	Error err;
	if (GDVIRTUAL_CALL(_create_offer, err)) {
		return err;
	}
	return ERR_UNCONFIGURED;
}

Error WebRTCPeerConnectionExtension::set_local_description(String p_type, String p_sdp) {
	Error err;
	if (GDVIRTUAL_CALL(_set_local_description, p_type, p_sdp, err)) {
		return err;
	}
	return ERR_UNCONFIGURED;
}

Error WebRTCPeerConnectionExtension::set_remote_description(String p_type, String p_sdp) {
	Error err;
	if (GDVIRTUAL_CALL(_set_remote_description, p_type, p_sdp, err)) {
		return err;
	}
	return ERR_UNCONFIGURED;
}

Error WebRTCPeerConnectionExtension::add_ice_candidate(String sdpMidName, int sdpMlineIndexName, String sdpName) {
	Error err;
	if (GDVIRTUAL_CALL(_add_ice_candidate, sdpMidName, sdpMlineIndexName, sdpName, err)) {
		return err;
	}
	return ERR_UNCONFIGURED;
}

Error WebRTCPeerConnectionExtension::poll() {
	Error err;
	if (GDVIRTUAL_CALL(_poll, err)) {
		return err;
	}
	return err;
}

void WebRTCPeerConnectionExtension::close() {
	GDVIRTUAL_CALL(_close);
}

WebRTCPeerConnection::ConnectionState WebRTCPeerConnectionExtension::get_connection_state() const {
	ConnectionState state;
	if (GDVIRTUAL_CALL(_get_connection_state, state)) {
		return state;
	}
	return STATE_DISCONNECTED;
}

//void WebRTCPeerConnectionExtension::set_native_webrtc_peer_connection(const godot_net_webrtc_peer_connection *p_impl) {
//	interface = p_impl;
//}

#endif // WEBRTC_EXTENSION_ENABLED
