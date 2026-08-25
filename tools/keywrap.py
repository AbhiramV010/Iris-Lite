#!/usr/bin/env python3
# Wraps/unwraps Iris-Lite's on-disk secret keys with a passphrase, so the SD
# card never holds a raw, directly-usable AES key - only a passphrase-locked
# blob. Losing the SD card (or a copy of it) no longer means losing the
# footage/event-bus keys outright; the attacker also needs the passphrase.
#
# install.sh calls `wrap-all` once, at first install, to generate fresh raw
# clip/event-bus keys and immediately wrap them - the raw bytes are never
# written to persistent disk. bin/iris-lite calls `unwrap-all` once per
# launch, prompting for that same passphrase, and writes the recovered raw
# keys into a tmpfs-only runtime directory that's wiped on shutdown.
import argparse
import getpass
import os
import sys

from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.scrypt import Scrypt

FORMAT_VERSION = b"\x01"
SALT_LEN = 16
NONCE_LEN = 12
RAW_KEY_LEN = 32
# "Interactive" scrypt parameters (Percival/NIST-style): strong enough to
# meaningfully slow down offline passphrase guessing, cheap enough for a
# once-per-boot unlock prompt on a Pi 4.
SCRYPT_N, SCRYPT_R, SCRYPT_P = 2 ** 14, 8, 1

KEY_NAMES = ("clip.key", "eventbus.key")


def _derive_wrapping_key(passphrase: str, salt: bytes) -> bytes:
    return Scrypt(salt=salt, length=32, n=SCRYPT_N, r=SCRYPT_R, p=SCRYPT_P).derive(
        passphrase.encode("utf-8")
    )


def wrap(raw_key: bytes, passphrase: str) -> bytes:
    salt = os.urandom(SALT_LEN)
    nonce = os.urandom(NONCE_LEN)
    wrapping_key = _derive_wrapping_key(passphrase, salt)
    ciphertext = AESGCM(wrapping_key).encrypt(nonce, raw_key, FORMAT_VERSION)
    return FORMAT_VERSION + salt + nonce + ciphertext


def unwrap(blob: bytes, passphrase: str) -> bytes:
    version, rest = blob[:1], blob[1:]
    if version != FORMAT_VERSION:
        raise ValueError(f"unsupported wrapped-key format version {version!r}")
    salt, rest = rest[:SALT_LEN], rest[SALT_LEN:]
    nonce, ciphertext = rest[:NONCE_LEN], rest[NONCE_LEN:]
    wrapping_key = _derive_wrapping_key(passphrase, salt)
    return AESGCM(wrapping_key).decrypt(nonce, ciphertext, version)


# On Windows, os.open() without O_BINARY defaults to text-mode translation
# (CRLF rewriting, 0x1A treated as EOF), which silently corrupts arbitrary
# binary ciphertext. O_BINARY doesn't exist on POSIX, where there's no such
# distinction, so default to 0 there.
_O_BINARY = getattr(os, "O_BINARY", 0)


def _write_private(path: str, data: bytes) -> None:
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | os.O_EXCL | _O_BINARY, 0o600)
    try:
        os.write(fd, data)
    finally:
        os.close(fd)


def _read_passphrase_with_confirmation() -> str:
    while True:
        passphrase = getpass.getpass("Set a passphrase to protect Iris-Lite's encryption keys: ")
        if len(passphrase) < 8:
            print("error: passphrase must be at least 8 characters", file=sys.stderr)
            continue
        confirm = getpass.getpass("Confirm passphrase: ")
        if passphrase != confirm:
            print("error: passphrases did not match, try again", file=sys.stderr)
            continue
        return passphrase


def cmd_wrap_all(secrets_dir: str) -> int:
    for name in KEY_NAMES:
        if os.path.exists(os.path.join(secrets_dir, name + ".enc")):
            print(f"{name}.enc already exists in {secrets_dir} - leaving it untouched")

    missing = [n for n in KEY_NAMES if not os.path.exists(os.path.join(secrets_dir, n + ".enc"))]
    if not missing:
        return 0

    passphrase = _read_passphrase_with_confirmation()
    for name in missing:
        raw_key = os.urandom(RAW_KEY_LEN)
        out_path = os.path.join(secrets_dir, name + ".enc")
        _write_private(out_path, wrap(raw_key, passphrase))
        print(f"wrapped key written -> {out_path}")
    return 0


def cmd_unwrap_all(secrets_dir: str, runtime_dir: str) -> int:
    blobs = {}
    for name in KEY_NAMES:
        wrapped_path = os.path.join(secrets_dir, name + ".enc")
        try:
            with open(wrapped_path, "rb") as f:
                blobs[name] = f.read()
        except FileNotFoundError:
            print(f"error: {wrapped_path} missing - run install.sh first", file=sys.stderr)
            return 1

    passphrase = getpass.getpass("Iris-Lite passphrase: ")

    raw_keys = {}
    for name, blob in blobs.items():
        try:
            raw_keys[name] = unwrap(blob, passphrase)
        except Exception:
            print("error: wrong passphrase or corrupted key file", file=sys.stderr)
            return 1

    os.makedirs(runtime_dir, exist_ok=True)
    for name, raw_key in raw_keys.items():
        out_path = os.path.join(runtime_dir, name)
        if os.path.exists(out_path):
            os.remove(out_path)
        _write_private(out_path, raw_key)

    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)

    p_wrap = sub.add_parser("wrap-all", help="generate clip.key/eventbus.key and write passphrase-wrapped blobs")
    p_wrap.add_argument("secrets_dir", help="directory to write clip.key.enc/eventbus.key.enc into")

    p_unwrap = sub.add_parser("unwrap-all", help="decrypt wrapped blobs into raw runtime key files")
    p_unwrap.add_argument("secrets_dir", help="directory containing clip.key.enc/eventbus.key.enc")
    p_unwrap.add_argument("runtime_dir", help="directory to write raw clip.key/eventbus.key into (should be tmpfs)")

    args = parser.parse_args()

    if args.mode == "wrap-all":
        sys.exit(cmd_wrap_all(args.secrets_dir))
    elif args.mode == "unwrap-all":
        sys.exit(cmd_unwrap_all(args.secrets_dir, args.runtime_dir))
