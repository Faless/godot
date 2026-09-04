/**************************************************************************/
/*  library_godot_debugger.js                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

const GodotDebugger = {
	$GodotDebugger__deps: ['$GodotRuntime', '$IDHandler', '$GodotConfig'],
	$GodotDebugger: {
		debug_id: -1,
		bind: function (id, ref, cb) {
			const port = IDHandler.get(id);
			if (!port) {
				return -1;
			}
			port.onmessage = function (event) {
				// TODO Possibly use a string for negotiation/ping/state/kill?
				if (!(event.data instanceof Uint8Array)) {
					GodotRuntime.error('Only Uint8Array messages are supported');
					return;
				}
				const buffer = event.data;
				if (buffer.length == 0) {
					return;
				}
				const len = buffer.length * buffer.BYTES_PER_ELEMENT;
				const out = GodotRuntime.malloc(len);
				HEAPU8.set(buffer, out);
				cb(ref, out, len);
				GodotRuntime.free(out);
			};
			return id;
		},
		post: function (id, msg) {
			const port = IDHandler.get(id);
			if (!port) {
				return;
			}
			port.postMessage(msg);
		},
		unbind: function (id) {
			const port = IDHandler.get(id);
			if (!port) {
				return;
			}
			port.close();
			port.onmessage = null;
		},
	},

	godot_js_debugger_post__proxy: 'sync',
	godot_js_debugger_post__sig: 'vpi',
	godot_js_debugger_post: function (p_id, p_ptr, p_size) {
		if (!p_size) {
			return;
		}
		const msg = GodotRuntime.heapSlice(HEAPU8, p_ptr, p_size);
		GodotDebugger.post(p_id, msg);
	},

	godot_js_debugger_bind__proxy: 'sync',
	godot_js_debugger_bind__sig: 'iipp',
	godot_js_debugger_bind: function (p_id, p_cb, p_ref) {
		let id = p_id;
		if (id == -1) {
			if (!GodotConfig.debug_port) {
				return -1;
			}
			if (GodotDebugger.debug_id !== -1) {
				GodotRuntime.error('Already bound');
				return -1;
			}
			id = IDHandler.add(GodotConfig.debug_port);
		}
		const cb = GodotRuntime.get_func(p_cb);
		return GodotDebugger.bind(id, p_ref, cb);
	},

	godot_js_debugger_unbind__proxy: 'sync',
	godot_js_debugger_unbind__sig: 'vi',
	godot_js_debugger_unbind: function (p_id) {
		GodotDebugger.unbind(p_id);
		GodotDebugger.debug_id = -1;
	},
};

autoAddDeps(GodotDebugger, '$GodotDebugger');
mergeInto(LibraryManager.library, GodotDebugger);
