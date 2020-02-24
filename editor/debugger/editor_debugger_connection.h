/*************************************************************************/
/*  editor_debugger_connection.h                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#ifndef EDITOR_DEBUGGER_CONNECTION_H
#define EDITOR_DEBUGGER_CONNECTION_H

#include "core/io/packet_peer.h"
#include "core/io/tcp_server.h"

class EditorDebuggerPeer : public Reference {

public:
	virtual bool has_message() = 0;
	virtual Array get_message() = 0;
	virtual Error put_message(const Array p_arr) = 0;

	virtual int get_max_message_size() const = 0;
	virtual bool is_peer_connected() = 0;
	virtual void close() = 0;
};

class EditorDebuggerServer : public Reference {

public:
	virtual Error start() = 0;
	virtual void stop() = 0;
	virtual bool is_active() const = 0;
	virtual bool is_connection_available() const = 0;
	virtual Ref<EditorDebuggerPeer> take_connection() = 0;
};

class Thread;
class Mutex;
class EditorDebuggerServerTCP : public EditorDebuggerServer {

private:
	Ref<TCP_Server> server;
	List<Ref<EditorDebuggerPeer> > peers;
	Thread *thread = NULL;
	Mutex *mutex = NULL;
	bool running = false;

	static void _poll_func(void *p_ud);

public:
	virtual Error start();
	virtual void stop();
	virtual bool is_active() const;
	virtual bool is_connection_available() const;
	virtual Ref<EditorDebuggerPeer> take_connection();

	EditorDebuggerServerTCP();
	~EditorDebuggerServerTCP();
};
#endif // EDITOR_DEBUGGER_CONNECTION_H
