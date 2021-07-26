#include "http_client_curl.h"

char* HTTPClientCurl::methods[10] = {
    "GET", 
    "HEAD", 
    "POST", 
    "PUT", 
    "DELETE", 
    "OPTIONS",
    "TRACE",
    "CONNECT",
    "PATCH",
    "MAX",
};

RequestContext::RequestContext() {
    write_buffer = memnew(Buffer);
}

RequestContext::~RequestContext() {
    if (read_buffer) {
        memfree(read_buffer);
    }
    if (header_list) {
        memfree(header_list);
    }
    memfree(write_buffer);
}

int Buffer::read_bytes(char *p_buf, int p_len) {
    if (data.size() < p_len) {
        p_len = data.size();
    }
    // TODO: Can we copy uint8_ts to chars like this?
    memcpy(p_buf, data.ptr(), p_len);
    roff += p_len;
    return p_len;
}

int Buffer::write_bytes(char *p_buf, int p_len) {
    const int remaining = data.size()-woff;
    if (p_len > remaining) {
        data.resize(woff + p_len);
    }

    memcpy(data.ptrw() + woff, p_buf, p_len);
    woff += p_len;
    return p_len;
}

void Buffer::set_data(uint8_t *p_data, int p_len) {
    clear();
    data.resize(p_len);
    memcpy(data.ptrw(), p_data, p_len);
}

void Buffer::clear() {
    data.clear();
    woff = 0;
    roff = 0;
}

int Buffer::remaining() {
    return data.size()-roff;
}

int  Buffer::size() {
    return data.size();
}

HTTPClient *HTTPClientCurl::_create_func() {
	return memnew(HTTPClientCurl);
}

size_t HTTPClientCurl::_read_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    Buffer *b = (Buffer*)userdata;
    return b->read_bytes(buffer, nitems);
}

size_t HTTPClientCurl::_write_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    Buffer *b = (Buffer*)userdata;
    return b->write_bytes(buffer, nitems);
}

Error HTTPClientCurl::connect_to_host(const String &p_host, int p_port = -1, bool p_ssl = false, bool p_verify_host = true) {
    curl = curl_multi_init();
    ssl = p_ssl;
    verify_host = p_verify_host;
    return OK;
}

void HTTPClientCurl::close() {
    if (curl) {
        curl_multi_cleanup(curl);
        curl = nullptr;
    }
}

Error HTTPClientCurl::request(Method p_method, const String &p_url, const Vector<String> &p_headers, const uint8_t *p_body, int p_body_size) {
    CURL* eh = curl_easy_init();
    curl_easy_setopt(eh, CURLOPT_URL, p_url.ptr());
    curl_easy_setopt(eh, CURLOPT_CUSTOMREQUEST, methods[(int)p_method]);
    RequestContext* ctx = memnew(RequestContext);
    if (p_body) {
        Buffer* b = memnew(Buffer);
        b->set_data((uint8_t*)p_body, p_body_size);
        curl_easy_setopt(eh, CURLOPT_POSTFIELDSIZE, (curl_off_t)p_body_size);
        curl_easy_setopt(eh, CURLOPT_READFUNCTION, _read_callback);
        curl_easy_setopt(eh, CURLOPT_READDATA, b);
        ctx->read_buffer = b;
    }
    if (ssl) {
        curl_easy_setopt(eh, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    }
    if (verify_host) {
        // When CURLOPT_SSL_VERIFYHOST is 2, that certificate must indicate 
        // that the server is the server to which you meant to connect, or 
        // the connection fails. Simply put, it means it has to have the same 
        // name in the certificate as is in the URL you operate against.
        // @see https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYHOST.html
        curl_easy_setopt(eh, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, _write_callback);
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, ctx->write_buffer);
    
    // Headers
    curl_slist *hl = nullptr;
    for (int i = 0; i < p_headers.size(); i++) {
        hl = curl_slist_append(hl, p_headers[i].utf8().get_data());
    }
    curl_easy_setopt(eh, CURLOPT_HTTPHEADER, hl);
    ctx->header_list = hl;

    curl_easy_setopt(eh, CURLOPT_PRIVATE, ctx);

    curl_multi_add_handle(curl, eh);
    return OK;
}

Error HTTPClientCurl::poll() {
    CURLMcode rc = curl_multi_perform(curl, &still_running);
    if (rc == OK) {
        rc = curl_multi_wait(curl, nullptr, 0, 1000, nullptr);
    }
    
    if (rc != OK) {
        return FAILED;
    }

    CURLMsg* msg = curl_multi_info_read(curl, nullptr);
    if (msg && msg->msg == CURLMSG_DONE) {
        RequestContext* ctx;
        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
        if (ctx->write_buffer) {
            response.clear();
            const int size = ctx->write_buffer->size();
            ctx->write_buffer->read_bytes((char *)response.ptrw(), size);
        }
        memfree(ctx);
        curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_multi_remove_handle(curl, msg->easy_handle);
        curl_easy_cleanup(msg->easy_handle);
    }
}