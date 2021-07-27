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

RequestContext::~RequestContext() {
    if (read_buffer) {
        memfree(read_buffer);
    }
    if (header_list) {
        memfree(header_list);
    }
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

size_t HTTPClientCurl::_header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    RequestContext *ctx = (RequestContext*)userdata;
    String s((const char*)buffer);
    Vector<String> parts = s.split(":");
    if (parts.size() != 2) {
        ERR_PRINT("Malformed header: " + s);
        return 0;
    }
    
    String header = parts[0].to_lower();
    String val = parts[1];
    // Trim any whitespace prefix
    while (val.begins_with(" ")) {
        val = val.trim_prefix(" ");
    }

    // Use the content length to determine body size.
    if (header == "content-length") {
        *ctx->body_size = val.to_int();
    }

    // If the Connection header is set to "close" then
    // keep-alive isn't enabled.
    if (header == "connection" && val == "close") {
        ctx->keep_alive = false;
    }

    return size*nitems;
}

size_t HTTPClientCurl::_read_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    Buffer *b = (Buffer*)userdata;
    return b->read_bytes(buffer, nitems);
}

size_t HTTPClientCurl::_write_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    RequestContext* ctx = (RequestContext*)userdata;
    PackedByteArray chunk;
    chunk.resize(nitems);
    memcpy(chunk.ptrw(), buffer, size*nitems);
    ctx->response_chunks->append(chunk);
    *(ctx->status) = STATUS_BODY;
    return size*nitems;
}

curl_slist *HTTPClientCurl::_ip_addr_to_slist(const IPAddress &p_addr) {
    return curl_slist_append(nullptr, String(p_addr).ascii().get_data());
}

String HTTPClientCurl::_hostname_from_url(const String &p_url) {
    String hostname = p_url.trim_prefix("http://");
    hostname = hostname.trim_prefix("https://");
    return hostname.split("/")[0];
}

IPAddress HTTPClientCurl::_resolve_dns(const String &p_hostname) {
    return IP::get_singleton()->resolve_hostname(p_hostname);
}

Error HTTPClientCurl::connect_to_host(const String &p_host, int p_port, bool p_ssl, bool p_verify_host) {
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
    WARN_PRINT("This Curl based HTTPClient is experimental!");
    // Only one request can be in flight at a time.
    if (in_flight) {
        return ERR_ALREADY_IN_USE;
    }

    CURL* eh = curl_easy_init();
    curl_easy_setopt(eh, CURLOPT_URL, p_url.ptr());
    curl_easy_setopt(eh, CURLOPT_CUSTOMREQUEST, methods[(int)p_method]);

    String hostname = _hostname_from_url(p_url);
    // TODO: Is there a way to make this not block?
    IPAddress addr = _resolve_dns(hostname);
    curl_slist *host = _ip_addr_to_slist(addr);
    curl_easy_setopt(eh, CURLOPT_RESOLVE, host);
    
    RequestContext* ctx = memnew(RequestContext);
    ctx->body_size = &body_size;
    ctx->status = &status;
    ctx->response_chunks = &response_chunks;
    
    if (p_body) {
        Buffer* b = memnew(Buffer);
        b->set_data((uint8_t*)p_body, p_body_size);

        // I'm not really sure what the difference is, but according to curl docs,
        // CURLOPT_POSTFIELDSIZE_LARGE should be used for date over 2GiB.
        // Either way, we need to set one of these feilds in order to be able to send binary data
        // otherwise libcurl will attempt to use strlen to determine size (which obv won't work for
        // binary data since it isn't a string).
        if (body_size <= 2.147e9) {
            curl_easy_setopt(eh, CURLOPT_POSTFIELDSIZE, (curl_off_t)p_body_size);
        } else {
            curl_easy_setopt(eh, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)p_body_size);
        }
        
        // Somewhat counter intuitively, the read function is actually used by libcurl to send data,
        // while the write function (see below) is used by libcurl to write response data to storage
        // (or in our case, memory).
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
    
    // Reset body size.
    body_size = 0;

    // Initialize callbacks.
    curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, _header_callback);
    curl_easy_setopt(eh, CURLOPT_HEADERDATA, ctx);
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, _write_callback);
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, ctx);
    
    // Headers
    curl_slist *hl = nullptr;
    for (int i = 0; i < p_headers.size(); i++) {
        hl = curl_slist_append(hl, p_headers[i].utf8().get_data());
    }
    curl_easy_setopt(eh, CURLOPT_HTTPHEADER, hl);
    ctx->header_list = hl;

    // Set the request context. CURLOPT_PRIVATE is just arbitrary data
    // that can be associated with request handlers.
    // @see https://curl.se/libcurl/c/CURLOPT_PRIVATE.html
    curl_easy_setopt(eh, CURLOPT_PRIVATE, ctx);

    curl_multi_add_handle(curl, eh);
    in_flight = true;
    status = STATUS_CONNECTED;

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
        curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);

        status = ctx->keep_alive ? STATUS_CONNECTED : STATUS_DISCONNECTED;

        memfree(ctx);

        curl_multi_remove_handle(curl, msg->easy_handle);
        curl_easy_cleanup(msg->easy_handle);
        in_flight = false;
    }
    
    return OK;
}

PackedByteArray HTTPClientCurl::read_response_body_chunk() {
    PackedByteArray chunk = response_chunks[0];
    response_chunks.remove(0);
    return chunk;
}

HTTPClient *(*HTTPClient::_create)() = HTTPClientCurl::_create_func;