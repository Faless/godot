/*************************************************************************/
/*  http_client.h                                                        */
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

#ifndef HTTP_CLIENT_CURL_H
#define HTTP_CLIENT_CURL_H

#include "core/io/http_client.h"
#include <stdio.h>
#include <curl/curl.h>

class Buffer {
    Vector<uint8_t> data;
    int roff = 0;
    int woff = 0;

public:
    Buffer() {}
    void set_data(uint8_t *p_data, int p_len);
    int read_bytes(char *p_buf, int p_len);
    int write_bytes(char *p_buf, int p_n);
    void resize(size_t size);
    void clear();
    int size();
    int remaining();
    ~Buffer() {}
};

class RequestContext {
public:

    RequestContext();
    ~RequestContext();

    Buffer* read_buffer;
    Buffer* write_buffer;
    curl_slist *header_list;
};

class HTTPClientCurl : public HTTPClient {
    
    static char* methods[10];
    static size_t _read_callback(char *buffer, size_t size, size_t nitems, void *userdata);
    static size_t _write_callback(char *buffer, size_t size, size_t nitems, void *userdata);

    CURLM* curl = nullptr;
    int still_running = 0;
    bool ssl = false;
    bool verify_host = false;
    bool blocking_mode = false;
    int read_chunk_size = 0;

    Status status = STATUS_DISCONNECTED;
    int response_code = 0;
    PackedByteArray response;

public:
	static HTTPClient *_create_func();

    virtual Error connect_to_host(const String &p_host, int p_port = -1, bool p_ssl = false, bool p_verify_host = true) override;
    virtual void close() override;
    virtual void set_connection(const Ref<StreamPeer> &p_connection) override {}
    virtual Ref<StreamPeer> get_connection() const override { return nullptr; }
    
    Status get_status() const override { return status; }
    virtual bool has_response() const override { return response_code != 0; }
    virtual bool is_response_chunked() const override { return false; }
    virtual int get_response_code() const override { return response_code; }
    virtual Error get_response_headers(List<String> *r_response) override { return OK; }
    virtual int get_response_body_length() const override { return response.size(); }
    virtual PackedByteArray read_response_body_chunk() override { return response; }
    virtual void set_blocking_mode(bool p_enabled) override { blocking_mode = p_enabled; }
    virtual bool is_blocking_mode_enabled() const override { return blocking_mode; }
    virtual void set_read_chunk_size(int p_size) override { read_chunk_size = p_size; }
    virtual int get_read_chunk_size() const override { return read_chunk_size; }

    virtual Error request(Method p_method, const String &p_url, const Vector<String> &p_headers, const uint8_t *p_body, int p_body_size) override;
    virtual Error poll() override;
};

#endif // #define HTTP_CLIENT_CURL_H