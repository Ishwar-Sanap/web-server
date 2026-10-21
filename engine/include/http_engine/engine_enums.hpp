
#pragma once

namespace http_engine
{
    enum class EngineReturnValue
    {
        SUCCESS,
        BAD_REQUEST,
        IO_ERROR,
        CLIENT_CONNECTION_CLOSED,
    };
}