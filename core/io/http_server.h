/*************************************************************************/
/*  stream_peer_tcp.h                                                    */
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

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "core/io/ip.h"
#include "core/io/ip_address.h"
#include "core/reference.h"

class HTTPServer : public Reference {

	GDCLASS(HTTPServer, Reference);
	OBJ_CATEGORY("Networking");

protected:
	String serve_path;

	static HTTPServer *(*_create)();
	static void _bind_methods();

public:
	virtual Error listen(int p_port, IP_Address p_bind_ip = IP_Address("*")) = 0;
	virtual bool is_listening() const = 0;
	virtual void stop() = 0;
	virtual void poll() = 0;
	virtual void set_serve_path(String p_path);
	virtual String get_serve_path();

	static Ref<HTTPServer> create_ref();
	static HTTPServer *create();

	HTTPServer();
	~HTTPServer();
};

#endif
