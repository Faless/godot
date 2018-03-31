/*************************************************************************/
/*  lws_server.h                                                         */
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
#ifndef LWS_HTTP_SERVER_H
#define LWS_HTTP_SERVER_H

#ifndef JAVASCRIPT_ENABLED

#include "core/io/http_server.h"
#include "core/reference.h"
#include "lws_helper.h"
#include "os/dir_access.h"

class LWSHTTPServer : public HTTPServer {

	GDCIIMPL(LWSHTTPServer, HTTPServer);

	LWS_HELPER(LWSHTTPServer);

protected:
	Map<String, String> mime_types;
	DirAccess *dir_access;

public:
	Error listen(int p_port, IP_Address = IP_Address("*"));
	void stop();
	bool is_listening() const;
	virtual void poll() { _lws_poll(); }

	String get_mime_type(String p_ext);

	LWSHTTPServer();
	~LWSHTTPServer();
};

#endif // JAVASCRIPT_ENABLED

#endif // LWS_HTTP_SERVER_H
