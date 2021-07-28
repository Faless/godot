#include "http_client_curl.h"
#include "core/string/print_string.h"

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
    if (header_list) {
        memfree(header_list);
        header_list = nullptr;
    }
    if (read_buffer) {
        memfree(read_buffer);
        read_buffer = nullptr;
    }
}

HTTPClient *HTTPClientCurl::_create_func() {
	return memnew(HTTPClientCurl);
}

size_t HTTPClientCurl::_header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    RequestContext *ctx = (RequestContext*)userdata;
    String s((const char*)buffer);
    Vector<String> parts = s.split(":");
    if (parts.size() != 2) {
        return size*nitems;
    }

    ctx->response_headers->push_back(s);

    String header = parts[0].to_lower();
    String val = parts[1];
    // Trim any whitespace prefix
    while (val.begins_with(" ")) {
        val = val.trim_prefix(" ");
    }

    // Use the content length to determine body size.
    if (header == "content-length") {
        *(ctx->body_size) = val.to_int();
    }

    // If the Connection header is set to "close" then
    // keep-alive isn't enabled.
    if (header == "connection" && val == "close") {
        *(ctx->keep_alive) = false;
    }

    if (header == "transfer-encoding" && val == "chunked") {
        *(ctx->body_size) = -1;
        *(ctx->chunked) = true;
    }

    *(ctx->has_response) = true;

    return size*nitems;
}

size_t HTTPClientCurl::_read_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    RingBuffer<uint8_t> *b = (RingBuffer<uint8_t>*)userdata;
    int n = b->copy((uint8_t*)buffer, 0, size*nitems);
    return n;
}

size_t HTTPClientCurl::_write_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    
    RequestContext* ctx = (RequestContext*)userdata;
    PackedByteArray chunk;
    chunk.resize(size*nitems);
    memcpy(chunk.ptrw(), buffer, size*nitems);
    ctx->response_chunks->append(chunk);
    *(ctx->status) = STATUS_BODY;

    return size*nitems;
}

curl_slist *HTTPClientCurl::_ip_addr_to_slist(const IPAddress &p_addr) {
    String addr = String(p_addr);
    addr = addr.substr(0, addr.rfind(":"));
    String h = host + ":" + String::num_int64(port) + ":" + addr;
    print_line("resolve host: " + h);
    return curl_slist_append(nullptr, h.ascii().get_data());
}

String HTTPClientCurl::_hostname_from_url(const String &p_url) {
    String hostname = p_url.trim_prefix("http://");
    hostname = hostname.trim_prefix("https://");
    return hostname.split("/")[0];
}

IPAddress HTTPClientCurl::_resolve_dns(const String &p_hostname) {
    print_line("hostname: " + p_hostname);
    return IP::get_singleton()->resolve_hostname(p_hostname, IP::Type::TYPE_IPV4);
}

Error HTTPClientCurl::_poll_curl() {
    CURLMcode rc = curl_multi_perform(curl, &still_running);
    if (still_running) {
        rc = curl_multi_wait(curl, nullptr, 0, 1000, nullptr);
    }
    
    if (rc != CURLM_OK) {
        ERR_PRINT("MULTI ERROR: " + String::num_int64(rc));
        return FAILED;
    }

    if (still_running == 0) {
        int n = 0;
        CURLMsg* msg = curl_multi_info_read(curl, &n);
        if (msg && msg->msg == CURLMSG_DONE) {
            if (msg->data.result != CURLE_OK) {
                ERR_PRINT("Curl result failed: " + String::num_int64(msg->data.result));
                status = STATUS_DISCONNECTED;
                return FAILED;
            }

            RequestContext* ctx;
            CURLcode rc = curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
            if (rc != CURLE_OK) {
                ERR_PRINT("couldnt get status code ! " + String::num_int64(rc));
                return FAILED;
            }
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);

            memfree(ctx);

            curl_multi_remove_handle(curl, msg->easy_handle);
            curl_easy_cleanup(msg->easy_handle);
            in_flight = false;
        }
    }
    return OK;
}

Error HTTPClientCurl::get_response_headers(List<String> *r_response) {
    *r_response = response_headers;
    return OK;
}

Error HTTPClientCurl::connect_to_host(const String &p_host, int p_port, bool p_ssl, bool p_verify_host) {
    if (curl == nullptr) {
        curl = curl_multi_init();
    }
    
    response_code = 0;
    body_size = -1;
    // response_headers.clear();
    response_available = false;
    response_code = 0;
    response_chunks.clear();
    keep_alive = false;
    status = STATUS_CONNECTED;
    
    host = p_host.trim_prefix("http://").trim_prefix("https://");
    port = p_port;
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
    curl_easy_setopt(eh, CURLOPT_URL, (host+p_url).ascii().get_data());
    curl_easy_setopt(eh, CURLOPT_CUSTOMREQUEST, methods[(int)p_method]);
    // TODO: Is there a way to make this not block?
    // TODO: Support resolving multiple addresses.
    IPAddress addr = _resolve_dns(host);
    curl_slist *h = _ip_addr_to_slist(addr);
    CURLcode rc = curl_easy_setopt(eh, CURLOPT_RESOLVE, h);
    if (rc != CURLE_OK) {
        ERR_PRINT("failed to initialize dns resolver: " + String::num_int64(rc));
        return FAILED;
    }
    
    RequestContext* ctx = memnew(RequestContext);
    ctx->response_headers = &response_headers;
    ctx->body_size = &body_size;
    ctx->status = &status;
    ctx->response_chunks = &response_chunks;
    ctx->has_response = &response_available;
    ctx->chunked = &chunked;
    ctx->keep_alive = &keep_alive;
    
    if (p_body_size > 0) {
        RingBuffer<uint8_t>* b = memnew(RingBuffer<uint8_t>);
        b->resize(p_body_size);
        b->write(p_body, p_body_size);

        // Special cases for POST and PUT to configure uploads.
        switch (p_method)
        {
        case METHOD_POST:
            curl_easy_setopt(eh, CURLOPT_POST, 1L);
            // curl_easy_setopt(eh, CURLOPT_POSTFIELDS, p_body);
            curl_easy_setopt(eh, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)p_body_size);
            break;
        case METHOD_PUT:
            curl_easy_setopt(eh, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(eh, CURLOPT_INFILESIZE_LARGE, (curl_off_t)p_body_size);
            break;
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
    for (int i = 0; i < p_headers.size(); i++) {
        ctx->header_list = curl_slist_append(ctx->header_list, p_headers[i].ascii().get_data());
    }
    if (ctx->header_list) {
        rc = curl_easy_setopt(eh, CURLOPT_HTTPHEADER, ctx->header_list);
        if (rc != CURLE_OK) {
            ERR_PRINT("failed to set request headers: " + String::num_uint64(rc));
            return FAILED;
        }
    }

    // Set the request context. CURLOPT_PRIVATE is just arbitrary data
    // that can be associated with request handlers.
    // @see https://curl.se/libcurl/c/CURLOPT_PRIVATE.html
    curl_easy_setopt(eh, CURLOPT_PRIVATE, ctx);

    CURLMcode mrc = curl_multi_add_handle(curl, eh); 
    if (mrc != CURLM_OK) {
        ERR_PRINT("failed to add easy handle: " + String::num_int64(rc));
        return FAILED;
    }

    in_flight = true;
    status = STATUS_REQUESTING;
    still_running = 0;
    return OK;
}

Error HTTPClientCurl::poll() {
    if (status != STATUS_BODY) {
        return _poll_curl();
    }

    return OK;
}

PackedByteArray HTTPClientCurl::read_response_body_chunk() {
    if (status == STATUS_BODY) {
        Error err = _poll_curl();
        if (err != OK) {
            return PackedByteArray();
        }
    }
    if (response_chunks.is_empty()) {
        status = keep_alive ? STATUS_CONNECTED : STATUS_DISCONNECTED;
        return PackedByteArray();
    }
    PackedByteArray chunk = response_chunks[0];
    response_chunks.remove(0);
    
    return chunk;
}

HTTPClient *(*HTTPClient::_create)() = HTTPClientCurl::_create_func;