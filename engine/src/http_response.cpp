#include "http_engine/http_response.hpp"
#include <stdexcept>

namespace http_engine
{
    std::string HttpResponse::version() const
    {
        return m_version;
    }
    std::string HttpResponse::reason() const
    {
        return m_reason;
    }
    std::string HttpResponse::body() const
    {
        return m_body;
    }
    int HttpResponse::statusCode() const
    {
        return m_status_code;
    }
    const std::unordered_map<std::string, std::string> &HttpResponse::headers() const
    {
        return m_headers;
    }

    HttpResponse &HttpResponse::send(const std::string &body)
    {
        if (m_header_sent)
        {
            throw std::runtime_error("Error [ERR_HTTP_HEADERS_SENT]: Cannot set headers after they are sent to the client");
        }

        this->m_headers[string_consts::content_type_header] = string_consts::media_type_text_plain;
        this->m_body = std::move(body);

        this->m_header_sent = true;
        return *this;
    }
    HttpResponse &HttpResponse::json(const nlohmann::json &body)
    {
        if (m_header_sent)
        {
            throw std::runtime_error("Error [ERR_HTTP_HEADERS_SENT]: Cannot set headers after they are sent to the client");
        }

        this->m_headers[string_consts::content_type_header] = string_consts::media_type_application_json;
        this->m_body = body.dump();
        
        this->m_header_sent = true;
        return *this;
    }
    HttpResponse &HttpResponse::status(int code)
    {
        this->m_status_code = code;
        switch (code)
        {
        case 200:
            this->m_reason = string_consts::reason_ok;
            break;
        case 201:
            this->m_reason = string_consts::reason_created;
            break;
        case 202:
            this->m_reason = string_consts::reason_accepted;
            break;
        case 400:
            this->m_reason = string_consts::reason_bad_request;
            break;
        case 401:
            this->m_reason = string_consts::reason_unauthorized;
            break;
        case 403:
            this->m_reason = string_consts::reason_forbidden;
            break;
        case 404:
            this->m_reason = string_consts::reason_not_found;
            break;
        case 429:
            this->m_reason = string_consts::reason_too_maany_requests;
            break;
        case 500:
            this->m_reason = string_consts::reason_server_error;
            break;
        case 502:
            this->m_reason = string_consts::reason_bad_gateway;
            break;
        case 503:
            this->m_reason = string_consts::reason_service_unavailable;
            break;
        case 504:
            this->m_reason = string_consts::reason_gateway_timeout;
            break;
        default:
            this->m_reason = string_consts::reason_unknown;
            break;
        }

        return *this;
    }
}
