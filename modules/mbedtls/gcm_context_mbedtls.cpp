/*************************************************************************/
/*  gcm_context_mbedtls.cpp                                              */
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

#include "gcm_context_mbedtls.h"

#include "mbedtls/gcm.h"

GCMContext *GCMContextMbedTLS::create() {
	return memnew(GCMContextMbedTLS);
}

Error GCMContextMbedTLS::start(Operation p_operation, CypherType p_cypher, Vector<uint8_t> p_key, Vector<uint8_t> p_iv, Vector<uint8_t> p_aad) {
	ERR_FAIL_COND_V_MSG(ctx != nullptr, ERR_ALREADY_IN_USE, "Context already started. Call 'finish' before starting a new one.");
	int op = p_operation == GCM_OPERATION_ENCRYPT ? MBEDTLS_GCM_ENCRYPT : MBEDTLS_GCM_DECRYPT;
	mbedtls_cipher_id_t cypher;
	switch (p_cypher) {
		case GCM_CYPHER_AES:
			cypher = MBEDTLS_CIPHER_ID_AES;
			break;
		default:
			ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Invalid cypher selected.");
	}
	ctx = (mbedtls_gcm_context *)memalloc(sizeof(mbedtls_gcm_context));
	mbedtls_gcm_init(ctx);
	int ret = mbedtls_gcm_setkey(ctx, cypher, p_key.ptr(), p_key.size() * 8);
	if (ret) {
		memdelete(ctx);
		ctx = nullptr;
		ERR_FAIL_V_MSG(FAILED, "Setkey failed with error: " + itos(ret));
	}
	int aad_size = p_aad.size();
	ret = mbedtls_gcm_starts(ctx, op, p_iv.ptr(), p_iv.size(), aad_size ? p_aad.ptr() : nullptr, aad_size);
	if (ret) {
		memdelete(ctx);
		ctx = nullptr;
		ERR_FAIL_V_MSG(FAILED, "Start failed with error: " + itos(ret));
	}
	return OK;
}

Vector<uint8_t> GCMContextMbedTLS::update(Vector<uint8_t> p_chunk) {
	ERR_FAIL_COND_V(ctx == nullptr, Vector<uint8_t>());
	size_t len = p_chunk.size();
	ERR_FAIL_COND_V(len == 0, Vector<uint8_t>());
	Vector<uint8_t> out;
	out.resize(p_chunk.size());
	int ret = mbedtls_gcm_update(ctx, p_chunk.size(), p_chunk.ptr(), out.ptrw());
	ERR_FAIL_COND_V(ret, Vector<uint8_t>());
	return out;
}

Vector<uint8_t> GCMContextMbedTLS::finish(int p_tag_length) {
	ERR_FAIL_COND_V(ctx == nullptr, Vector<uint8_t>());
	Vector<uint8_t> out;
	out.resize(p_tag_length);
	int ret = mbedtls_gcm_finish(ctx, out.ptrw(), p_tag_length);
	ERR_FAIL_COND_V(ret, Vector<uint8_t>());
	mbedtls_gcm_free(ctx);
	memdelete(ctx);
	ctx = nullptr;
	return out;
}

void GCMContextMbedTLS::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "operation", "cypher", "key", "iv", "additional_authenticated_data"), &GCMContextMbedTLS::start);
	ClassDB::bind_method(D_METHOD("update", "chunk"), &GCMContextMbedTLS::update);
	ClassDB::bind_method(D_METHOD("finish", "tag_length"), &GCMContextMbedTLS::finish);
	BIND_ENUM_CONSTANT(GCM_CYPHER_AES);
	BIND_ENUM_CONSTANT(GCM_CYPHER_MAX);
	BIND_ENUM_CONSTANT(GCM_OPERATION_ENCRYPT);
	BIND_ENUM_CONSTANT(GCM_OPERATION_DECRYPT);
	BIND_ENUM_CONSTANT(GCM_OPERATION_MAX);
}

GCMContextMbedTLS::GCMContextMbedTLS() {
}

GCMContextMbedTLS::~GCMContextMbedTLS() {
	if (ctx != nullptr) {
		mbedtls_gcm_free(ctx);
		memdelete(ctx);
		ctx = nullptr;
	}
}
