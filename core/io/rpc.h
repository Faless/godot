/*************************************************************************/
/*  rpc.h                                                                */
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

#ifndef RPC_H
#define RPC_H

#include "core/string/string_name.h"

enum RPCTransferMode {
	RPC_TRANSFER_UNRELIABLE,
	RPC_TRANSFER_ORDERED,
	RPC_TRANSFER_RELIABLE
};

enum RPCMode {
	RPC_DISABLED, // No rpc for this method, calls to this will be blocked (default)
	RPC_REMOTE, // Using rpc() on it will call method in all remote peers
	RPC_MASTER, // Using rpc() on it will call method on wherever the master is, be it local or remote
	RPC_PUPPET, // Using rpc() on it will call method for all puppets
};

struct RPCConfig {
	StringName name;
	RPCMode rpc_mode = RPC_DISABLED;
	bool sync = false;
	RPCTransferMode transfer_mode = RPC_TRANSFER_RELIABLE;
	int channel = 0;

	bool operator==(RPCConfig const &p_other) const {
		return name == p_other.name;
	}

	struct Sort {
		StringName::AlphCompare compare;
		bool operator()(const RPCConfig &p_a, const RPCConfig &p_b) const {
			return compare(p_a.name, p_b.name);
		}
	};
};

#endif // RPC_H
