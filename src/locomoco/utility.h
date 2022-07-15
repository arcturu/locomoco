#pragma once
#include <strsafe.h>

// Print formatted string on debug console.
#define DEBUG_PRINT(format, ...) {\
    const size_t BUFFER_SIZE = 1024;\
    wchar_t buffer[BUFFER_SIZE];\
    StringCbPrintf(buffer, BUFFER_SIZE, format, __VA_ARGS__);\
    OutputDebugString(buffer);\
}

#define SUCCESS_OR_RETURN(hr) {\
    if (!lm::Utility::SuccessOrLog(hr)) {\
        return;\
    }\
}

#define SUCCESS_OR_RETURN_NULL(hr) {\
    if (!lm::Utility::SuccessOrLog(hr)) {\
        return nullptr;\
    }\
}

#define SUCCESS_OR_RETURN_FALSE(hr) {\
    if (!lm::Utility::SuccessOrLog(hr)) {\
        return false;\
    }\
}

namespace lm {

class Utility {
public:
    static bool SuccessOrLog(HRESULT hr) {
        if (FAILED(hr))
        {
            OutputDebugHresult(hr);
            return false;
        }
        return true;
    }

    static void OutputDebugHresult(HRESULT hr) {
        const size_t BUFFER_SIZE = 1024;
        wchar_t hrStr[BUFFER_SIZE];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, hrStr, BUFFER_SIZE, nullptr);
        OutputDebugString(hrStr);
    }
};

}
