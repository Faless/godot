/*************************************************************************/
/*  lws_server.cpp                                                       */
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
#ifndef JAVASCRIPT_ENABLED

#include "lws_http_server.h"

Error LWSHTTPServer::listen(int p_port, IP_Address p_bind_ip) {

	ERR_FAIL_COND_V(context != NULL, FAILED);

	PoolVector<String> protocols;
	struct lws_context_creation_info info;

	memset(&info, 0, sizeof info);

	// Prepare lws protocol structs
	_lws_make_protocols(this, &LWSHTTPServer::_lws_gd_callback, protocols, &_lws_ref);

	info.port = p_port;
	info.user = _lws_ref;
	info.protocols = _lws_ref->lws_structs;
	info.gid = -1;
	info.uid = -1;
	//info.ws_ping_pong_interval = 5;

	context = lws_create_context(&info);

	if (context == NULL) {
		_lws_free_ref(_lws_ref);
		_lws_ref = NULL;
		ERR_EXPLAIN("Unable to create LWS context");
		ERR_FAIL_V(FAILED);
	}

	return OK;
}

bool LWSHTTPServer::is_listening() const {
	return context != NULL;
}

String LWSHTTPServer::get_mime_type(String p_ext) {

	if (mime_types.has(p_ext))
		return mime_types[p_ext];

	return "application/octet-stream";
}

int LWSHTTPServer::_handle_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {

	LWSPeer::PeerData *peer_data = (LWSPeer::PeerData *)user;

	switch (reason) {
		case LWS_CALLBACK_HTTP: {
			String req_path;
			String file_path;
			FileAccess *file_access;
			int ret;

			if (len < 1) {
				lws_return_http_status(wsi, HTTP_STATUS_BAD_REQUEST, NULL);
				goto http_end;
			}

			req_path = (char *)in;
			// No directories for now
			if (req_path.ends_with("/")) {
				lws_return_http_status(wsi, HTTP_STATUS_NOT_ACCEPTABLE, NULL);
				goto http_end;
			}
			WARN_PRINTS("HTTP Request: " + req_path);
			file_path = serve_path + req_path;
			if (serve_path == "")
				lws_return_http_status(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
			else if (dir_access->file_exists(file_path)) {
				//file_access = FileAccess::open();
				String mime_type = get_mime_type(req_path.get_extension());
				ret = lws_serve_http_file(
						wsi,
						file_path.utf8().get_data(),
						mime_type.utf8().get_data(),
						NULL,
						0);
				WARN_PRINTS("Sending file... " + itos(ret));
				if (ret != 0)
					return -1;
				return 0;
			} else {
				lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
				goto http_end;
			}

		http_end:
			if (lws_http_transaction_completed(wsi))
				return -1;
			return 0;
		}

		case LWS_CALLBACK_HTTP_FILE_COMPLETION:
			if (lws_http_transaction_completed(wsi))
				return -1;
			return 0;

		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
			// check header here?
			break;

		case LWS_CALLBACK_ESTABLISHED:
			break;

		case LWS_CALLBACK_CLOSED:
			return 0; // we can end here

		case LWS_CALLBACK_RECEIVE:
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			break;

		default:
			break;
	}

	return 0;
}

void LWSHTTPServer::stop() {
	if (context == NULL)
		return;

	destroy_context();
	context = NULL;
}

LWSHTTPServer::LWSHTTPServer() {
	context = NULL;
	_lws_ref = NULL;
	dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	mime_types["ico"] = "image/x-icon";
	mime_types["htm"] = "text/html";
	mime_types["html"] = "text/html";
	mime_types["css"] = "text/css";
	mime_types["js"] = "text/javascript";
}

LWSHTTPServer::~LWSHTTPServer() {
	invalidate_lws_ref(); // we do not want any more callbacks
	memdelete(dir_access);
	stop();
}

#endif // JAVASCRIPT_ENABLED
