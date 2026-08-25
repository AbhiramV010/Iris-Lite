#include "StorageManager.hpp"
#include "logging.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

static const std::string BASE = "/mnt/clipDrive/clips/";
static const int AES_KEY_LEN = 32; // AES-256
static const int GCM_IV_LEN = 12;  // standard GCM nonce size
static const int GCM_TAG_LEN = 16;
static const unsigned char FORMAT_VERSION = 0x02;
static const int METADATA_LEN_FIELD_SIZE = 4; // big-endian uint32

void StorageManager::ensureReady()
{
    mkdir("/mnt/clipDrive", 0777);
    mkdir(BASE.c_str(), 0777);
}

std::string StorageManager::buildPath(uint64_t start, uint64_t end)
{
    std::ostringstream ss;

    ss << BASE
       << "iris_"
       << start << "_"
       << end
       << ".mp4";

    return ss.str();
}

bool StorageManager::loadKey(unsigned char* keyOut)
{
    // bin/iris-lite unwraps the passphrase-protected key (tools/keywrap.py)
    // into a tmpfs-only runtime directory and points this env var at it;
    // the /etc/iris-lite fallback only matters for a raw-key-on-disk deploy.
    const char* envPath = std::getenv("IRIS_LITE_CLIP_KEY_PATH");
    const std::string KEY_PATH = (envPath && *envPath) ? envPath : "/etc/iris-lite/clip.key";

    FILE* f = fopen(KEY_PATH.c_str(), "rb");
    if (!f)
    {
        logError("Clip encryption key missing at " + KEY_PATH +
                  " (run install.sh, then start via bin/iris-lite)");
        return false;
    }

    // Defense in depth: install.sh chmods this 600. Refuse to use it if
    // that has regressed (e.g. a careless `chmod -R`, or a restore that
    // didn't preserve permissions) rather than silently trusting a
    // group/world-readable key.
    struct stat st;
    if (fstat(fileno(f), &st) != 0 || (st.st_mode & (S_IRWXG | S_IRWXO)) != 0)
    {
        logError("Clip encryption key at " + KEY_PATH +
                  " is group/world accessible; refusing to use it. Run: chmod 600 " + KEY_PATH);
        fclose(f);
        return false;
    }

    size_t n = fread(keyOut, 1, AES_KEY_LEN, f);
    fclose(f);

    if (n != static_cast<size_t>(AES_KEY_LEN))
    {
        logError("Clip encryption key at " + KEY_PATH + " is not " +
                  std::to_string(AES_KEY_LEN) + " bytes");
        return false;
    }

    return true;
}

bool StorageManager::encryptFile(const std::string& path, const std::string& metadata)
{
    if (metadata.size() > 0xFFFFFFFFu)
    {
        logError("encryptFile: metadata too large for " + path);
        return false;
    }

    unsigned char key[AES_KEY_LEN];
    if (!loadKey(key))
        return false;

    FILE* in = fopen(path.c_str(), "rb");
    if (!in)
    {
        OPENSSL_cleanse(key, AES_KEY_LEN);
        logError("encryptFile: cannot open " + path);
        return false;
    }

    const std::string outPath = path + ".enc";
    FILE* out = fopen(outPath.c_str(), "wb");
    if (!out)
    {
        fclose(in);
        OPENSSL_cleanse(key, AES_KEY_LEN);
        logError("encryptFile: cannot create " + outPath);
        return false;
    }
    // Restrict the output before any ciphertext lands in it, rather than
    // relying on umask alone.
    fchmod(fileno(out), 0600);

    fwrite(&FORMAT_VERSION, 1, 1, out);

    unsigned char iv[GCM_IV_LEN];
    RAND_bytes(iv, GCM_IV_LEN);
    fwrite(iv, 1, GCM_IV_LEN, out);

    // Big-endian metadata length. It travels outside the ciphertext (the
    // decryptor needs it before it can split metadata from video bytes),
    // but it's bound in as AEAD associated data below so tampering with it
    // is still detected by the tag check, not silently accepted.
    const uint32_t metaLen = static_cast<uint32_t>(metadata.size());
    unsigned char metaLenBytes[METADATA_LEN_FIELD_SIZE] = {
        static_cast<unsigned char>((metaLen >> 24) & 0xFF),
        static_cast<unsigned char>((metaLen >> 16) & 0xFF),
        static_cast<unsigned char>((metaLen >> 8) & 0xFF),
        static_cast<unsigned char>(metaLen & 0xFF)
    };
    fwrite(metaLenBytes, 1, METADATA_LEN_FIELD_SIZE, out);

    unsigned char aad[1 + METADATA_LEN_FIELD_SIZE];
    aad[0] = FORMAT_VERSION;
    std::memcpy(aad + 1, metaLenBytes, METADATA_LEN_FIELD_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    int aadLen = 0;
    bool ok = ctx &&
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_LEN, nullptr) &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, iv) &&
        EVP_EncryptUpdate(ctx, nullptr, &aadLen, aad, sizeof(aad));

    std::vector<unsigned char> outBuf(metadata.size() + EVP_MAX_BLOCK_LENGTH);
    int outLen = 0;

    // Metadata (e.g. the human-readable trigger label) rides inside the
    // same ciphertext as the video, ahead of it, rather than ever touching
    // a filename or living outside the AEAD boundary.
    if (ok && !metadata.empty())
    {
        if (EVP_EncryptUpdate(ctx, outBuf.data(), &outLen,
                reinterpret_cast<const unsigned char*>(metadata.data()),
                static_cast<int>(metadata.size())))
        {
            fwrite(outBuf.data(), 1, outLen, out);
        }
        else
        {
            ok = false;
        }
    }

    std::vector<unsigned char> inBuf(1 << 16);
    outBuf.resize(inBuf.size() + EVP_MAX_BLOCK_LENGTH);
    size_t n;

    while (ok && (n = fread(inBuf.data(), 1, inBuf.size(), in)) > 0)
    {
        if (!EVP_EncryptUpdate(ctx, outBuf.data(), &outLen, inBuf.data(), static_cast<int>(n)))
        {
            ok = false;
            break;
        }
        fwrite(outBuf.data(), 1, outLen, out);
    }

    unsigned char tag[GCM_TAG_LEN];
    if (ok && EVP_EncryptFinal_ex(ctx, outBuf.data(), &outLen))
    {
        fwrite(outBuf.data(), 1, outLen, out);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_LEN, tag);
        fwrite(tag, 1, GCM_TAG_LEN, out);
    }
    else
    {
        ok = false;
    }

    if (ctx)
        EVP_CIPHER_CTX_free(ctx);
    fclose(in);
    fclose(out);
    OPENSSL_cleanse(key, AES_KEY_LEN);

    if (!ok)
    {
        logError("encryptFile: encryption failed on " + path);
        remove(outPath.c_str());
        return false;
    }

    remove(path.c_str());
    logInfo("Encrypted clip -> " + outPath);
    return true;
}