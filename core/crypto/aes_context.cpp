/*************************************************************************/
/*  aes_context.cpp                                                      */
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

#include "core/crypto/aes_context.h"

Error AESContext::set_encode_key(PoolByteArray p_key) {
	size_t bits = p_key.size() << 3;
	ERR_FAIL_COND_V(bits < 128 || bits > 256, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(next_power_of_2(bits) != bits, ERR_INVALID_PARAMETER);
	return ctx.set_encode_key(p_key.read().ptr(), bits);
}

Error AESContext::set_decode_key(PoolByteArray p_key) {
	size_t bits = p_key.size() << 3;
	ERR_FAIL_COND_V(bits < 128 || bits > 256, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(next_power_of_2(bits) != bits, ERR_INVALID_PARAMETER);
	return ctx.set_decode_key(p_key.read().ptr(), bits);
}

PoolByteArray AESContext::encrypt_ecb(PoolByteArray p_src) {
	ERR_FAIL_COND_V(p_src.size() != 16, PoolByteArray());
	PoolByteArray out;
	out.resize(16);
	Error err = ctx.encrypt_ecb(p_src.read().ptr(), out.write().ptr());
	ERR_FAIL_COND_V(err != OK, PoolByteArray());
	return out;
}

PoolByteArray AESContext::decrypt_ecb(PoolByteArray p_src) {
	ERR_FAIL_COND_V(p_src.size() != 16, PoolByteArray());
	PoolByteArray out;
	out.resize(16);
	Error err = ctx.decrypt_ecb(p_src.read().ptr(), out.write().ptr());
	ERR_FAIL_COND_V(err != OK, PoolByteArray());
	return out;
}

Array AESContext::encrypt_cbc(PoolByteArray p_src, PoolByteArray p_iv) {
	ERR_FAIL_COND_V_MSG(p_src.size() % 16, Array(), "Encryption source must be in blocks of 16 bytes. Try to add some padding.");
	ERR_FAIL_COND_V_MSG(p_iv.size() != 16, Array(), "The initialization vector (IV) must be exactly 16 bytes.");
	PoolByteArray dest;
	dest.resize(p_src.size());
	PoolByteArray iv;
	iv.append_array(p_iv);
	Error err = ctx.encrypt_cbc(p_src.size(), iv.write().ptr(), p_src.read().ptr(), dest.write().ptr());
	ERR_FAIL_COND_V(err != OK, Array());
	Array out;
	out.push_back(dest);
	out.push_back(iv);
	return out;
}

Array AESContext::decrypt_cbc(PoolByteArray p_src, PoolByteArray p_iv) {
	ERR_FAIL_COND_V_MSG(p_src.size() % 16, Array(), "Malformed decryption source. Must be in blocks of 16 bytes");
	ERR_FAIL_COND_V_MSG(p_iv.size() != 16, Array(), "The initialization vector (IV) must be exactly 16 bytes.");
	PoolByteArray dest;
	dest.resize(p_src.size());
	PoolByteArray iv;
	iv.append_array(p_iv);
	Error err = ctx.decrypt_cbc(p_src.size(), iv.write().ptr(), p_src.read().ptr(), dest.write().ptr());
	ERR_FAIL_COND_V(err != OK, Array());
	Array out;
	out.push_back(dest);
	out.push_back(iv);
	return out;
}

void AESContext::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_encode_key", "key"), &AESContext::set_encode_key);
	ClassDB::bind_method(D_METHOD("set_decode_key", "key"), &AESContext::set_decode_key);
	ClassDB::bind_method(D_METHOD("encrypt_ecb", "src"), &AESContext::encrypt_ecb);
	ClassDB::bind_method(D_METHOD("decrypt_ecb", "src"), &AESContext::decrypt_ecb);
	ClassDB::bind_method(D_METHOD("encrypt_cbc", "src", "iv"), &AESContext::encrypt_cbc);
	ClassDB::bind_method(D_METHOD("decrypt_cbc", "src", "iv"), &AESContext::decrypt_cbc);
}

AESContext::AESContext() {
}
