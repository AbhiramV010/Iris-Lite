#!/usr/bin/env python3
# Decrypts a .mp4.enc clip produced by the C++ compression engine
# (Compression/src/StorageManager.cpp::encryptFile), which writes:
#   [12-byte GCM nonce][ciphertext][16-byte GCM tag]
# encrypted with AES-256-GCM using /etc/iris-lite/clip.key.
import argparse
import sys

from cryptography.hazmat.primitives.ciphers.aead import AESGCM

KEY_PATH = "/etc/iris-lite/clip.key"
NONCE_LEN = 12


def decrypt(in_path: str, key_path: str, out_path: str) -> None:
    with open(key_path, "rb") as f:
        key = f.read()

    with open(in_path, "rb") as f:
        blob = f.read()

    nonce, ciphertext_and_tag = blob[:NONCE_LEN], blob[NONCE_LEN:]
    plaintext = AESGCM(key).decrypt(nonce, ciphertext_and_tag, None)

    with open(out_path, "wb") as f:
        f.write(plaintext)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Decrypt an Iris-Lite .mp4.enc clip")
    parser.add_argument("clip", help="path to the .mp4.enc file")
    parser.add_argument("-o", "--out", help="output path (default: strip .enc suffix)")
    parser.add_argument("-k", "--key", default=KEY_PATH, help=f"clip key path (default: {KEY_PATH})")
    args = parser.parse_args()

    out_path = args.out or (args.clip[:-4] if args.clip.endswith(".enc") else args.clip + ".dec")

    try:
        decrypt(args.clip, args.key, out_path)
    except FileNotFoundError as e:
        sys.exit(f"error: {e.filename} not found")
    except Exception as e:
        sys.exit(f"error: decryption failed ({e})")

    print(f"decrypted -> {out_path}")
