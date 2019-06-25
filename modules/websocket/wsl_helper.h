/*************************************************************************/
/*  wsl_helper.h                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#ifndef WSL_HELPER_H
#define WSL_HELPER_H

#define WSL_BUF_SIZE 65536
#define WSL_PACKET_SIZE WSL_BUF_SIZE

#include "core/io/stream_peer.h"
#include "core/os/os.h"
#include "core/reference.h"
#include "core/ring_buffer.h"
#include "wsl_peer.h"

struct _WSLRef {
	bool free_context;
	bool is_polling;
	bool is_valid;
	bool is_destroying;
	void *obj;
};

_WSLRef *_wsl_create_ref(void *obj);
void _wsl_free_ref(_WSLRef *ref);
bool _wsl_destroy(struct wsl_context *context, _WSLRef *ref);
bool _wsl_poll(struct wsl_context *context, _WSLRef *ref);

/* clang-format off */
#define WSL_HELPER() \
protected:															\
	struct _WSLRef *_wsl_ref;												\
	struct wslay_event_context_ptr context;											\
																\
	void invalidate_wsl_ref() {												\
		if (_wsl_ref != NULL)												\
			_wsl_ref->is_valid = false;										\
	}															\
																\
	void destroy_context() {												\
		if (_wsl_destroy(context, _wsl_ref)) {										\
			context = NULL;												\
			_wsl_ref = NULL;											\
		}														\
	}															\
																\
public:																\
																\
	void _wsl_poll() {													\
		ERR_FAIL_COND(context == NULL);											\
		do {														\
			_keep_servicing = false;										\
			if (::_wsl_poll(context, _wsl_ref)) {									\
				context = NULL;											\
				_wsl_ref = NULL;										\
				break;												\
			}													\
		} while (_keep_servicing);											\
	}															\
																\
protected:

/* clang-format on */

#endif // WSL_HELPER_H
