/*
 * HttpMessage.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
 * https://github.com/emmef/simpledsp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstring>
#include <iostream>
#include <regex>
#include <speakerman/HttpMessage.hpp>

namespace speakerman {
    static const char *UNKNOWN_STATUS = "Unknown status";
    static const char *EMPTY = "";

    class StatusCode : public std::runtime_error
    {
        int code_;
        const char *additional_message_;
    public:
        StatusCode(int code, const char *message) :
                std::runtime_error(http_status::status_name(code)),
                code_(code), additional_message_(message)
        {
            if (!http_status::is_ok(code)) {
                std::cerr << "HTTP " << code << std::endl;
            }
        }

        StatusCode(int code) : StatusCode(code, nullptr)
        {}

        int code() const
        { return code_; }

        bool has_additional_message() const
        { return additional_message_ != nullptr; }

        const char *additional_message() const
        { return additional_message_; }

        bool is_ok() const
        { return http_status::is_ok(code_); }
    };

    class FlagGuard
    {
        std::atomic_flag &flag_;
        bool enter_;
    public:
        FlagGuard(std::atomic_flag &f) : flag_(f), enter_(!flag_.test_and_set())
        {}

        bool enter() const
        { return enter_; }

        bool busy() const
        { return !enter_; }

        ~FlagGuard()
        { if (enter_) { flag_.clear(); }}
    };

    template<typename T>
    class ResetGuard
    {
        T *&to_reset_;
    public:
        ResetGuard(T *&to_reset) : to_reset_(to_reset)
        {}

        ~ResetGuard()
        {
            to_reset_ = nullptr;
        }
    };

    static inline int getHexValue(char c)
    {
        switch (c) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return c - '0';
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                return c + 10 - 'A';
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                return c + 10 - 'f';
            default:
                return -128;
        }
    }

    static constexpr bool isWhiteSpace(char c)
    {
        return c == ' ' || c == '\t';
    }

    static constexpr bool isNum(char c)
    {
        return c >= '0' && c <= '9';
    }

    static constexpr bool isAlpha(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    static constexpr bool isAlphaNum(char c)
    {
        return isAlpha(c) || isNum(c);
    }

    static constexpr bool isKeyStartChar(char c)
    {
        return isAlpha(c);
    }

    static constexpr bool isKeyChar(char c)
    {
        return isKeyStartChar(c) || isNum(c) || c == '-';
    }


    const char *http_status::status_name(unsigned status)
    {
        switch (status) {
            case OK:
                // 200
                return "OK";
            case PARTIAL_CONTENT:
                // 206
                return "Partial content";
            case BAD_REQUEST:
                // 400
                return "Bad Request";
            case NOT_FOUND:
                // 404
                return "Not Found";
            case METHOD_NOT_ALLOWED:
                // 405
                return "Method Not Allowed";
            case REQUEST_URI_TOO_LONG:
                // 414
                return "Request URI Too Long";
            case INTERNAL_SERVER_ERROR:
                //500
                return "Internal Server Error";
            case SERVICE_UNAVAILABLE:
                //503
                return "Service Unavailable";
            case HTTP_VERSION_NOT_SUPPORTED:
                // 505
                return "HTTP Version Not Supported";
            default:
                return UNKNOWN_STATUS;
        }
    }

    bool http_status::is_ok(unsigned status)
    {
        return status == OK;
    }

    int http_status::format_message(char *buffer, size_t max_length, unsigned status)
    {
        return snprintf(buffer, max_length, "HTTP/1.1 %u %s\r\n", status, status_name(status));
    }

    int http_status::format_message_extra(char *buffer, size_t max_length, unsigned status, const char *extraMessage)
    {
        return snprintf(buffer, max_length, "HTTP/1.1 %u %s\r\n\r\n%s\r\n", status, status_name(status), extraMessage);
    }

    int http_status::format_message(output_stream &stream, unsigned status)
    {
        int r = stream.write_string("HTTP/1.1 ", 20);
        if (r < 0) {
            return r;
        }
        char status_str[7];
        snprintf(status_str, 7, "%u ", status & 0xffff);
        r = stream.write_string(status_str, 6);
        if (r < 0) {
            return r;
        }
        r = stream.write_string(status_name(status), 1024);
        if (r < 0) {
            return r;
        }
        return stream.write_string("\r\n", 3);
    }

    int http_status::format_message_extra(output_stream &stream, unsigned status, const char *extraMessage)
    {
        int r = format_message(stream, status);

        if (extraMessage) {
            size_t len = strnlen(extraMessage, 1024);
            char buff[6];
            snprintf(buff, 6, "%zu", len);
            r = stream.write_string("\r\nContent-Type: text/plain\r\nContent-length: ", 60);
            if (r < 0) {
                return r;
            }
            r = stream.write_string(buff, 6);
            if (r < 0) {
                return r;
            }

            r = stream.write_string("\r\n\r\n", 5);
            if (r < 0) {
                return r;
            }
            return stream.write_string(extraMessage, len);
        }
        return r;
    }

    void http_message::write_status(socket_stream &stream, unsigned status, const char *additional_message)
    {
        if (additional_message) {
            http_status::format_message_extra(read_buffer(), read_buffer_size(), status,
                                              additional_message);
            stream.write_string(read_buffer(), read_buffer_size());
            stream.flush();
        }
    }


    size_t http_message::valid_buffer_size(size_t size)
    {
        if (size >= 128 && size <= 1048960) {
            return size;
        }
        throw std::invalid_argument("Invalid buffer size");
    }

    const char *http_message::on_method(const char *method)
    {
        if (strncmp("GET", method, 10) == 0) {
//			std::cout << "D: Method = " << method << std::endl;
            return nullptr;
        }
        return "GET";
    }

    const char *http_message::on_url(const char *url)
    {
        if (*url == '/') {
//			std::cout << "D: URI = " << url << std::endl;
            return nullptr;
        }
        return "Only absolute paths allowed";
    }

    const char *http_message::on_version(const char *version)
    {
//		std::cout << "D: VERSION = " << version << std::endl;
        return nullptr;
    }

    void http_message::on_header(const char *header, const char *value)
    {
//		std::cout << "D: HEADER = " << header << " : " << value << std::endl;
    }

    static int write_header_raw(const char *header, const char *value, output_stream &stream)
    {
        int w = stream.write_string(header, 100);
        if (w < 0) {
            return w;
        }
        w = stream.write_string(": ", 3);
        if (w < 0) {
            return w;
        }
        w = stream.write_string(value, 1024);
        if (w < 0) {
            return w;
        }
        return stream.write_string("\r\n", 3);
    }

    int http_message::write_header(const char *header, const char *value)
    {
        return write_header_raw(header, value, *stream_);
    }

    int http_message::set_header(const char *header, const char *value)
    {
        return write_header_raw(header, value, headers_);
    }

    int http_message::write_content_length(size_t value)
    {
        char length[24];
        snprintf(length, 24, "%zu", value);
        return http_message::write_header("Content-Length", length);
    }

    int http_message::write_content_type(const char *value)
    {
        return http_message::write_header("Content-Type", value);
    }

    long signed http_message::handle_content_prefix(size_t length)
    {
        int w = write_content_length(length);
        if (w < 0) {
            return w;
        }
        if (content_type_) {
            w = write_content_type(content_type_);
            if (w < 0) {
                return w;
            }
        }
        int r;
        while ((r = headers_.read()) >= 0) {
            w = stream_->write(r);
            if (w < 0) {
                return w;
            }
        }
        w = stream_->write_string("\r\n", 2);
        if (w < 0) {
            return w;
        }
        stream_->flush();
        return length;
    }

    unsigned http_message::handle_ok()
    {
        output_stream &stream = *stream_;
        http_status::format_message(stream, http_status::OK);
        stream.flush();

        if (content_stream_length_ > 0) {
            size_t writes = content_stream_length_;
            handle_content_prefix(writes);
            for (size_t i = 0; i < content_stream_length_; i++) {
                int c = content_stream_->read();
                if (c < 0) {
                    stream.flush();
                    cleanup_content_stream();
                    return http_status::PARTIAL_CONTENT;
                }
                if (stream.write(c) < 0) {
                    stream.flush();
                    cleanup_content_stream();
                    return http_status::PARTIAL_CONTENT;
                }
            }
            stream.flush();
            cleanup_content_stream();
            return http_status::OK;
        }
        size_t length = response_.readable_size();
        if (length > 0) {
            int w = handle_content_prefix(length);
            if (w < 0) {
                return http_status::PARTIAL_CONTENT;
            }
            int r, writes = 0;
            while ((r = response_.read()) >= 0 && (w = stream.write(r)) >= 0) { writes++; }
            stream.flush();
            return
                    w >= 0 && (r >= 0 || r == stream_result::END_OF_STREAM) ? http_status::OK
                                                                            : http_status::PARTIAL_CONTENT;
        }
        return http_status::INTERNAL_SERVER_ERROR;
    }

    void http_message::cleanup_content_stream()
    {
        if (content_stream_) {
            input_stream *ptr = content_stream_;
            content_stream_ = nullptr;
            if (content_stream_delete()) {
                delete ptr;
            }
            else {
                ptr->close();
            }
        }
    }

    unsigned http_message::handle(socket_stream &stream)
    {
        FlagGuard busyGuard(busy_);
        if (busyGuard.busy()) {
            return http_status::SERVICE_UNAVAILABLE;
        }

        stream_ = &stream;
        ResetGuard<socket_stream> streamGuard(stream_);

        content_stream_length_ = 0;
        content_stream_ = nullptr;

        try {
            read_method();
            read_url();
            read_version();
            read_headers();
            headers_.flush();
            handle_request();
            return handle_ok();
        }
        catch (const StatusCode &status) {
            if (status.is_ok()) {
                return handle_ok();
            }
            std::cerr
                    << "While handling request: status="
                    << status.code()
                    << " "
                    << status.what();
            if (status.has_additional_message()) {
                if (status.code() == http_status::METHOD_NOT_ALLOWED) {
                    http_status::format_message(stream, status.code());
                    write_header("Accept", status.additional_message());
                }
                else {
                    http_status::format_message_extra(stream, status.code(), status.additional_message());
                }
                std::cerr << ": " << status.additional_message() << std::endl;
            }
            else {
                http_status::format_message(stream, status.code());
                std::cerr << std::endl;
            }
            stream.flush();
            return status.code();
        }
        catch (const std::exception &e) {
            http_status::format_message_extra(stream,
                                              http_status::INTERNAL_SERVER_ERROR,
                                              e.what());
            return http_status::INTERNAL_SERVER_ERROR;
        }
    }

    void http_message::set_error(unsigned status, const char *additional_message)
    {
        throw StatusCode(status, additional_message);
    }

    void http_message::set_success()
    {
        throw StatusCode(http_status::OK);
    }

    void http_message::set_content_type(const char *type)
    {
        content_type_ = type;
    }

    void http_message::handle_content(size_t content_length, input_stream *content_stream)
    {
        content_stream_length_ = 0;
        content_stream_ = nullptr;
        if (content_length > 0) {
            if (content_stream) {
                content_stream_length_ = content_length;
                content_stream_ = content_stream;
                throw StatusCode(http_status::OK);
            }
            throw StatusCode(http_status::INTERNAL_SERVER_ERROR, "Invalid content stream handle");
        }
    }

    void http_message::read_method()
    {
        int c;
        size_t length = 0;
        for (length = 0; length < (read_buffer_size() - 1) && ((c = stream_->read()) >= 0); length++) {
            if (c < 'A' || c > 'Z') {
                break;
            }
            read_buffer(length) = c;
        }
        if (c < 0) {
            throw StatusCode(http_status::BAD_REQUEST, "Unexpected end of stream");
        }
        else if (c != ' ') {
            throw StatusCode(http_status::BAD_REQUEST, "Invalid method");
        }
        read_buffer(length) = 0;
        const char *onMethod = on_method(read_buffer());
        if (onMethod) {
            throw StatusCode(http_status::METHOD_NOT_ALLOWED, onMethod);
        }
    }


    inline void http_message::read_url()
    {
        static constexpr int URI_DONE = 0;
        static constexpr int URI_SPACE = 1;
        static constexpr int URI_COPY = 2;
        static constexpr int URI_HEX1 = 3;
        static constexpr int URI_HEX2 = 4;
        static constexpr int URI_INVCHAR = -2;

        int dig, num = 0;
        int c;
        int state = URI_SPACE;
        size_t length;

        for (length = 0; length < (read_buffer_size() - 1) && ((c = stream_->read()) >= 0);) {
            if (state == URI_SPACE) {
                if (c == ' ') {
                    continue;
                }
                state = URI_COPY;
            }
            if (state == URI_COPY) {
                if (c == '+') {
                    read_buffer(length++) = ' ';
                }
                else if (c == '%') {
                    state = URI_HEX1;
                }
                else if (c == ' ') {
                    state = URI_DONE;
                    break;
                }
                else if (c >= ' ' && c <= 126) {
                    read_buffer(length++) = c;
                }
                else {
                    state = URI_INVCHAR;
                    break;
                }
            }
            else if (state == URI_HEX1) {
                dig = getHexValue(c);
                if (dig < 0) {
                    state = URI_INVCHAR;
                    break;
                }
                else {
                    num = 16 * dig;
                    state = URI_HEX2;
                }
            }
            else if (state == URI_HEX2) {
                dig = getHexValue(c);
                if (dig < 0) {
                    state = URI_INVCHAR;
                    break;
                }
                else {
                    read_buffer(length++) = num + dig;
                    state = URI_COPY;
                }
            }
        }
        if (state == URI_DONE) {
            read_buffer(length) = 0;
            const char *msg = on_url(read_buffer());
            if (msg) {
                throw StatusCode(http_status::BAD_REQUEST, msg);
            }
        }
        else {
            throw StatusCode(http_status::BAD_REQUEST, "URI parse error");
        }
    }

    void http_message::read_version()
    {
        static constexpr int VERSION_COPY = 2;
        static constexpr int VERSION_DONE = 0;
        static constexpr int VERSION_INVCHAR = -1;
        static constexpr const char *TEMPLATE = "HTTP/#\n";
        socket_stream &stream = *stream_;
        int tPos = 0;
        size_t length;
        int c;
        int state = VERSION_COPY;
        for (length = 0; length < (read_buffer_size() - 1) && ((c = stream.read()) >= 0);) {
            char expected = TEMPLATE[tPos];
            if (expected == '#') {
                if ((c >= '0' && c <= '9') || c == '.') {
                    read_buffer(length++) = c;
                    continue;
                }
                expected = TEMPLATE[++tPos];
            }
            if (expected == 0) {
                state = VERSION_DONE;
                break;
            }
            if (expected == '\n') {
                if (c == '\r' || c == '\n') {
                    tPos++;
                }
                else {
                    state = VERSION_INVCHAR;
                }
            }
            if (expected == c) {
                read_buffer(length++) = expected;
                tPos++;
            }
            else {
                state = VERSION_INVCHAR;
            }
        }
        if (state != VERSION_DONE) {
            throw StatusCode(http_status::BAD_REQUEST, "Malformed HTTP version");
        }
        read_buffer(length) = 0;
        const char *msg = on_version(read_buffer());
        if (msg) {
            throw StatusCode(http_status::HTTP_VERSION_NOT_SUPPORTED, msg);
        }
    }

    void http_message::read_headers()
    {
        static constexpr int INVALID = -1;
        static constexpr int NONE = 1;
        static constexpr int END_OF_HEADERS = 2;
        static constexpr int LF = 3;
        static constexpr int CR = 4;
        static constexpr int KEYNAME = 5;
        static constexpr int ASSIGN = 6;
        static constexpr int VALUE_WHITESPACE = 7;
        static constexpr int VALUE = 8;

        size_t length;
        int c;
        int state = NONE;
        const char *value = nullptr;
        for (length = 0; length < read_buffer_length() && ((c = stream_->read()) >= 0);) {
            if (state == NONE) {
                if (c == '\n') {
                    state = LF;
                }
                else if (c == '\r') {
                    state = CR;
                }
                else if (isWhiteSpace(c)) {
                    state = NONE;
                }
                else if (isKeyStartChar(c)) {
                    state = KEYNAME;
                    read_buffer(length++) = c;
                }
                else {
                    state = INVALID;
                    break;
                }
            }
            else if (state == CR) {
                if (c == '\r') {
                    state = END_OF_HEADERS;
                    break;
                }
                else if (c == '\n') {
                    continue;
                }
                else if (isWhiteSpace(c)) {
                    state = NONE;
                }
                else if (isKeyStartChar(c)) {
                    state = KEYNAME;
                    read_buffer(length++) = c;
                }
            }
            else if (state == LF) {
                if (c == '\n') {
                    state = END_OF_HEADERS;
                    break;
                }
                else if (c == '\r') {
                    continue;
                }
                else if (isWhiteSpace(c)) {
                    state = NONE;
                }
                else if (isKeyStartChar(c)) {
                    state = KEYNAME;
                    read_buffer(length++) = c;
                }
            }
            else if (state == KEYNAME) {
                if (isKeyChar(c)) {
                    read_buffer(length++) = c;
                }
                else if (isWhiteSpace(c)) {
                    read_buffer(length++) = 0;
                    state = ASSIGN;
                }
                else if (c == ':') {
                    read_buffer(length++) = 0;
                    state = VALUE_WHITESPACE;
                }
                else {
                    state = INVALID;
                    break;
                }
            }
            else if (state == ASSIGN) {
                if (isWhiteSpace(c)) {
                    continue;
                }
                if (c == ':') {
                    state = VALUE_WHITESPACE;
                }
                else {
                    state = INVALID;
                    break;
                }
            }
            else if (state == VALUE_WHITESPACE) {
                if (isWhiteSpace(c)) {
                    continue;
                }
                else if (c == '\r') {
                    state = CR;
                    on_header(read_buffer(), EMPTY);
                    length = 0;
                }
                else if (c == '\n') {
                    state = LF;
                    on_header(read_buffer(), EMPTY);
                    length = 0;
                }
                else {
                    value = read_buffer() + length;
                    read_buffer(length++) = c;
                    state = VALUE;
                }
            }
            else if (state == VALUE) {
                if (c == '\r') {
                    state = CR;
                    read_buffer(length++) = 0;
                    on_header(read_buffer(), value);
                    length = 0;
                }
                else if (c == '\n') {
                    state = LF;
                    read_buffer(length++) = 0;
                    on_header(read_buffer(), value);
                    length = 0;
                }
                else {
                    read_buffer(length++) = c;
                }
            }
        }
        if (state != END_OF_HEADERS) {
            throw StatusCode(http_status::BAD_REQUEST, "Error reading headers");
        }
    }


} /* End of namespace speakerman */

