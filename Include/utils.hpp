#pragma once
#include <string>

// A couple of small helper functions.
// Just here to keep the main code cleaner.

namespace Utils {
    bool fileExists(const std::string& path);
    std::string replaceExtension(const std::string& path, const std::string& newExt);
}
