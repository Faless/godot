/*************************************************************************/
/*  http_server.cpp                                                      */
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

#include "http_server.h"

HTTPServer *(*HTTPServer::_create)() = NULL;

void HTTPServer::set_serve_path(String p_path) {

	serve_path = p_path;
}

String HTTPServer::get_serve_path() {

	return serve_path;
}

void HTTPServer::_bind_methods() {

	ClassDB::bind_method(D_METHOD("listen", "port", "bind_ip"), &HTTPServer::listen, DEFVAL("*"));
	ClassDB::bind_method(D_METHOD("is_listening"), &HTTPServer::is_listening);
	ClassDB::bind_method(D_METHOD("stop"), &HTTPServer::stop);
	ClassDB::bind_method(D_METHOD("poll"), &HTTPServer::poll);
	ClassDB::bind_method(D_METHOD("set_serve_path", "path"), &HTTPServer::set_serve_path);
	ClassDB::bind_method(D_METHOD("get_serve_path"), &HTTPServer::get_serve_path);
}

Ref<HTTPServer> HTTPServer::create_ref() {

	if (!_create)
		return Ref<HTTPServer>();
	return Ref<HTTPServer>(_create());
}

HTTPServer *HTTPServer::create() {

	if (!_create)
		return NULL;
	return _create();
}

HTTPServer::HTTPServer() {
}

HTTPServer::~HTTPServer(){

};
