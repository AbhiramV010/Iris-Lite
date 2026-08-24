#include "StorageManager.hpp"
#include "logging.hpp"

#include <sys/stat.h>
#include <sstream>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <openssl/evp.h>
#include <openssl/rand.h>

static const std::string BASE = "/mnt/clipDrive/clips/";
static const std::string KEY_PATH = "/etc/iris-lite/clip.key";
static const int AES_KEY_LEN = 32; // AES-256
static const int GCM_IV_LEN = 12;  // standard GCM nonce size
static const int GCM_TAG_LEN = 16;

void StorageManager::ensureReady()
{
    mkdir("/mnt/clipDrive", 0777);
    mkdir(BASE.c_str(), 0777);
}

std::string StorageManager::buildPath(uint64_t start,
                                      uint64_t end,
                                      const std::string& tag)
{
    std::ostringstream ss;

    ss << BASE
       << "iris_"
       << start << "_"
       << end;

    if (!tag.empty())
        ss << "_" << tag;

    ss << ".mp4";

    return ss.str();
}

bool StorageManager::loadKey(unsigned char* keyOut)
{
    FILE* f = fopen(KEY_PATH.c_str(), "rb");
    if (!f)
    {
        logError("Clip encryption key missing at " + KEY_PATH + " (run install.sh)");
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

bool StorageManager::encryptFile(const std::string& path)
{
    unsigned char key[AES_KEY_LEN];
    if (!loadKey(key))
        return false;

    FILE* in = fopen(path.c_str(), "rb");
    if (!in)
    {
        logError("encryptFile: cannot open " + path);
        return false;
    }

    const std::string outPath = path + ".enc";
    FILE* out = fopen(outPath.c_str(), "wb");
    if (!out)
    {
        fclose(in);
        logError("encryptFile: cannot create " + outPath);
        return false;
    }

    unsigned char iv[GCM_IV_LEN];
    RAND_bytes(iv, GCM_IV_LEN);
    fwrite(iv, 1, GCM_IV_LEN, out);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    bool ok = ctx &&
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_IV_LEN, nullptr) &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, iv);

    std::vector<unsigned char> inBuf(1 << 16);
    std::vector<unsigned char> outBuf(inBuf.size() + EVP_MAX_BLOCK_LENGTH);
    int outLen = 0;
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