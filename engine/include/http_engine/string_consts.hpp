#pragma once

namespace http_engine::string_consts
{
    inline constexpr char http_version_1_0[] = "HTTP/1.0";
    inline constexpr char http_version_1_1[] = "HTTP/1.1";

    inline constexpr char content_type_header[] = "Content-Type";
    inline constexpr char content_type_header_lower[] = "content-type";
    inline constexpr char content_length_header[] = "Content-Length";
    inline constexpr char content_length_header_lower[] = "content-length";
    inline constexpr char host_header[] = "host";

    inline constexpr char media_type_text_plain[] = "text/plain";
    inline constexpr char media_type_application_json[] = "application/json";

    inline constexpr char reason_ok[] = "OK";
    inline constexpr char reason_bad_request[] = "Bad Request";
    inline constexpr char reason_not_found[] = "Not Found";
}
