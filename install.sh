#!/bin/bash
# Iris-Lite installer for Debian / Raspberry Pi OS.
#
# Installs system + Python dependencies and builds the C++ compression
# engine FROM THE CURRENT SOURCE TREE (no bundled/prebuilt binaries), and
# installs a small wrapper command ("iris-lite") that always execs the
# launcher out of this repo checkout. Because nothing is copied/frozen,
# editing any Decision/*.py or Compression/* source and re-running this
# installer (or just relaunching, for Python) keeps everything working.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$REPO_ROOT/Compression/build"
LOG_FILE="$REPO_ROOT/log/install.log"
: >"$LOG_FILE"

# ---------------- progress bar ----------------
# Real, step-driven progress: the bar only advances after a step's command
# has actually exited 0. Nothing here is a fixed sleep/timer.
TOTAL_STEPS=10
CURRENT_STEP=0

draw_bar() {
    local percent=$(( CURRENT_STEP * 100 / TOTAL_STEPS ))
    local filled=$(( CURRENT_STEP * 40 / TOTAL_STEPS ))
    local empty=$(( 40 - filled ))
    printf "\r["
    printf "%${filled}s" | tr ' ' '#'
    printf "%${empty}s" | tr ' ' '-'
    printf "] %3d%% (%d/%d) %-40s" "$percent" "$CURRENT_STEP" "$TOTAL_STEPS" "$1"
}

fail() {
    echo ""
    echo "Install failed: $1"
    echo "See $LOG_FILE for details."
    exit 1
}

step() {
    local label="$1"; shift
    draw_bar "$label"
    if ! "$@" >>"$LOG_FILE" 2>&1; then
        fail "$label"
    fi
    CURRENT_STEP=$((CURRENT_STEP + 1))
    draw_bar "$label"
    echo ""
}

echo "Iris-Lite installer (Debian / Raspberry Pi OS)"
echo "Repo: $REPO_ROOT"
echo ""

# ---------------- 1. OS check ----------------
draw_bar "Checking OS compatibility"
if [[ ! -r /etc/os-release ]]; then
    fail "Cannot read /etc/os-release; this installer targets Debian / Raspberry Pi OS."
fi
# shellcheck disable=SC1091
. /etc/os-release
if [[ "${ID:-}" != "debian" && "${ID:-}" != "raspbian" && "${ID_LIKE:-}" != *debian* ]]; then
    fail "Unsupported OS ($PRETTY_NAME). This installer targets Debian / Raspberry Pi OS."
fi
CURRENT_STEP=$((CURRENT_STEP + 1))
draw_bar "OS OK: $PRETTY_NAME"
echo ""

# ---------------- 2. apt update ----------------
step "Updating package lists" sudo apt-get update

# ---------------- 3. apt packages ----------------
APT_PACKAGES=(
    build-essential cmake pkg-config
    ffmpeg libopencv-dev
    python3 python3-pip python3-dev
    portaudio19-dev libatlas-base-dev
    v4l-utils
    openssl libssl-dev
)
step "Installing system packages" sudo apt-get install -y "${APT_PACKAGES[@]}"

# ---------------- 4. python packages ----------------
PY_PACKAGES=(opencv-contrib-python numpy pyaudio ai-edge-litert RPi.GPIO cryptography)
install_python_deps() {
    if pip3 install --user "${PY_PACKAGES[@]}"; then
        return 0
    fi
    # Debian 12+/Bookworm marks the system Python as "externally managed"
    # (PEP 668); fall back to an explicit override rather than a venv, since
    # RPi.GPIO / V4L2 camera access need the system interpreter.
    pip3 install --user --break-system-packages "${PY_PACKAGES[@]}"
}
step "Installing Python dependencies" install_python_deps

# ---------------- 5. cmake configure ----------------
step "Configuring compression engine (cmake)" \
    cmake -S "$REPO_ROOT/Compression" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

# ---------------- 6. build ----------------
NPROC="$(nproc 2>/dev/null || echo 2)"
step "Building compression engine (this builds from Compression/src, so edits there are picked up on the next install)" \
    cmake --build "$BUILD_DIR" -j"$NPROC"

# ---------------- 7. directories ----------------
setup_dirs() {
    mkdir -p "$REPO_ROOT/log"
    # Logs record trigger types and timestamps (an occupancy pattern),
    # unlike clips they aren't encrypted, so at least keep other local
    # accounts out.
    chmod 700 "$REPO_ROOT/log"
    sudo mkdir -p /clipDrive/clips
    sudo chown "$(id -u):$(id -g)" /clipDrive/clips
    # Clips are AES-256-GCM encrypted at rest, but there's no reason for
    # other local accounts to even enumerate filenames/timestamps.
    chmod 700 /clipDrive/clips
}
step "Preparing storage & log directories" setup_dirs

# ---------------- 8. encryption keys ----------------
# Generated once per install, never overwritten: the clip key must stay
# stable or previously-encrypted footage becomes undecryptable, and the
# event-bus key must stay stable while any Iris-Lite process is running.
#
# The raw keys are never written to the SD card: tools/keywrap.py generates
# them in memory and immediately passphrase-wraps them (scrypt + AES-256-GCM)
# before anything touches disk, so losing/imaging the card alone isn't
# enough to recover usable keys. bin/iris-lite prompts for the same
# passphrase at each launch to unwrap them into a tmpfs-only runtime dir.
SECRETS_DIR="/etc/iris-lite"
setup_secrets() {
    sudo mkdir -p "$SECRETS_DIR"
    sudo chown "$(id -u):$(id -g)" "$SECRETS_DIR"
    chmod 700 "$SECRETS_DIR"

    python3 "$REPO_ROOT/tools/keywrap.py" wrap-all "$SECRETS_DIR"
    chmod 600 "$SECRETS_DIR"/clip.key.enc "$SECRETS_DIR"/eventbus.key.enc
}
step "Generating encryption keys" setup_secrets

# ---------------- 9. hardware encoder access ----------------
# PerceptualCompressor talks to the v4l2 h264_v4l2m2m encoder device
# directly. Rather than running the whole engine as root just for that -
# which also means `sudo` execs a binary rebuilt from a source tree this
# same user can write to, i.e. any compromise of that checkout is a
# guaranteed root exploit - grant the narrower device access instead.
grant_hw_access() {
    sudo usermod -aG video "$(id -un)"
}
step "Granting hardware encoder access (video group)" grant_hw_access

# ---------------- 10. install launcher command ----------------
install_launcher() {
    chmod +x "$REPO_ROOT/bin/iris-lite"
    # The installed command is a thin wrapper that always execs the script
    # inside this checkout by absolute path - it is never copied elsewhere,
    # so editing Decision/*.py or Compression/* and relaunching (rebuilding
    # automatically happens for C++ changes) just works, no reinstall needed.
    sudo tee /usr/local/bin/iris-lite >/dev/null <<EOF
#!/bin/bash
exec "$REPO_ROOT/bin/iris-lite" "\$@"
EOF
    sudo chmod +x /usr/local/bin/iris-lite
}
step "Installing 'iris-lite' command" install_launcher

echo ""
echo "Install complete."
echo "NOTE: you were just added to the 'video' group so the compression engine"
echo "can reach the hardware encoder without root. Log out and back in (or"
echo "reboot) before running 'iris-lite' for the first time, otherwise group"
echo "membership won't have taken effect yet and the encoder will fail to open."
