#pragma once

#include <string>
#include <cstdint>

class StorageManager
{
public:
    void ensureReady();

    std::string buildPath(uint64_t start,
                          uint64_t end,
                          const std::string& tag = "");

    // AES-256-GCM encrypts the finished clip at `path` using the key from
    // /etc/iris-lite/clip.key (see install.sh), writing `path + ".enc"` and
    // removing the plaintext original. No-op-safe: on any failure the
    // plaintext file is left in place and false is returned.
    bool encryptFile(const std::string& path);

private:
    bool loadKey(unsigned char* keyOut);
};