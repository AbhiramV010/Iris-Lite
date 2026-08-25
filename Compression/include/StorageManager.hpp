#pragma once

#include <string>
#include <cstdint>

class StorageManager
{
public:
    void ensureReady();

    // Deliberately does NOT take a human-readable tag/trigger: filenames
    // are readable without the clip key, so anything that identifies what
    // kind of event happened belongs inside the encrypted payload
    // (see encryptFile's `metadata` param), never in the path.
    std::string buildPath(uint64_t start, uint64_t end);

    // AES-256-GCM encrypts the finished clip at `path` using the key from
    // /etc/iris-lite/clip.key (see install.sh), writing `path + ".enc"` and
    // removing the plaintext original. `metadata` (e.g. the human-readable
    // trigger label) is encrypted alongside the video rather than exposed
    // in the filename. Output format is
    // [1-byte version][12-byte GCM nonce][4-byte BE metadata length]
    // [ciphertext of metadata-bytes||video-bytes][16-byte GCM tag], with the
    // version byte and metadata length bound in as AEAD associated data so
    // a future format change - or a tampered length field - can't be
    // silently misinterpreted by the decryptor.
    // No-op-safe: on any failure the plaintext file is left in place and
    // false is returned.
    bool encryptFile(const std::string& path, const std::string& metadata = "");

private:
    bool loadKey(unsigned char* keyOut);
};