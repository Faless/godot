/*************************************************************************/
/*  gcm_context.h                                                        */
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

#ifndef GCM_CONTEXT_H
#define GCM_CONTEXT_H

#include "core/reference.h"

class GCMContext : public Reference {
	GDCLASS(GCMContext, Reference);

protected:
	static GCMContext *(*_create)();
	static void _bind_methods();

public:
	static GCMContext *create();

	enum CypherType {
		GCM_CYPHER_AES,
		GCM_CYPHER_MAX
	};

	enum Operation {
		GCM_OPERATION_ENCRYPT,
		GCM_OPERATION_DECRYPT,
		GCM_OPERATION_MAX
	};

	virtual Error start(Operation p_operation, CypherType p_cypher, Vector<uint8_t> p_key, Vector<uint8_t> p_iv, Vector<uint8_t> p_aad) = 0;
	virtual Vector<uint8_t> update(Vector<uint8_t> p_chunk) = 0;
	virtual Vector<uint8_t> finish(int p_tag_length) = 0;

	GCMContext();
};

VARIANT_ENUM_CAST(GCMContext::CypherType);
VARIANT_ENUM_CAST(GCMContext::Operation);

#endif // GCM_CONTEXT_H
