#!/usr/bin/env python3
# Decrypts a .mp4.enc clip produced by the C++ compression engine
# (Compression/src/StorageManager.cpp::encryptFile), which writes:
#   [1-byte format version][12-byte GCM nonce][4-byte BE metadata length]
#   [ciphertext of metadata-bytes||video-bytes][16-byte GCM tag]
# encrypted with AES-256-GCM using /etc/iris-lite/clip.key, with the version
# byte and metadata length bound in as AEAD associated data. The metadata
# (the human-readable trigger label, e.g. "Screaming (81.2%)") travels
# inside the encrypted payload rather than the filename, so it's only ever
# visible to someone who holds the clip key.
import argparse
import os
import stat
import struct
import sys

from cryptography.hazmat.primitives.ciphers.aead import AESGCM

# clip.key is stored passphrase-wrapped at /etc/iris-lite/clip.key.enc (see
# tools/keywrap.py); this needs a raw key file, which only exists while
# iris-lite is running (IRIS_LITE_CLIP_KEY_PATH, tmpfs) or after manually
# running `keywrap.py unwrap-all /etc/iris-lite <some-dir>`.
KEY_PATH = os.environ.get("IRIS_LITE_CLIP_KEY_PATH", "/etc/iris-lite/clip.key")
NONCE_LEN = 12
META_LEN_SIZE = 4
FORMAT_VERSION = b"\x02"


def _reject_if_group_or_world_accessible(path: str, fd: int) -> None:
    mode = stat.S_IMODE(os.fstat(fd).st_mode)
    if mode & (stat.S_IRWXG | stat.S_IRWXO):
        raise RuntimeError(
            f"{path} is group/world accessible (mode {oct(mode)}). "
            f"Refusing to use it - run: chmod 600 {path}"
        )


def decrypt(in_path: str, key_path: str, out_path: str) -> str:
    with open(key_path, "rb") as f:
        _reject_if_group_or_world_accessible(key_path, f.fileno())
        key = f.read()

    with open(in_path, "rb") as f:
        blob = f.read()

    version, rest = blob[:1], blob[1:]
    if version != FORMAT_VERSION:
        raise ValueError(f"unsupported .enc format version {version!r}")

    nonce, rest = rest[:NONCE_LEN], rest[NONCE_LEN:]
    meta_len_bytes, ciphertext_and_tag = rest[:META_LEN_SIZE], rest[META_LEN_SIZE:]
    meta_len = struct.unpack(">I", meta_len_bytes)[0]

    aad = version + meta_len_bytes
    plaintext = AESGCM(key).decrypt(nonce, ciphertext_and_tag, aad)

    metadata, video = plaintext[:meta_len], plaintext[meta_len:]

    with open(out_path, "wb") as f:
        f.write(video)

    return metadata.decode("utf-8", "replace")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Decrypt an Iris-Lite .mp4.enc clip")
    parser.add_argument("clip", help="path to the .mp4.enc file")
    parser.add_argument("-o", "--out", help="output path (default: strip .enc suffix)")
    parser.add_argument("-k", "--key", default=KEY_PATH, help=f"clip key path (default: {KEY_PATH})")
    args = parser.parse_args()

    out_path = args.out or (args.clip[:-4] if args.clip.endswith(".enc") else args.clip + ".dec")

    try:
        trigger = decrypt(args.clip, args.key, out_path)
    except FileNotFoundError as e:
        sys.exit(f"error: {e.filename} not found")
    except Exception as e:
        sys.exit(f"error: decryption failed ({e})")

    print(f"decrypted -> {out_path}")
    if trigger:
        print(f"trigger    -> {trigger}")
