#pragma once

#include <cstdint>
#include <string>

namespace akira::platform
{

struct UpdateInfo {
    bool available = false;
    std::string version;
    std::string tag;
    std::string channel;
    std::string nroUrl;
    std::string sha256Url;
    std::string sha256;
    std::string notesUrl;
    std::string notes;
    int64_t size = 0;
    std::string error;
};

}
