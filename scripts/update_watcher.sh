#!/bin/bash
REPO="RonnieHarrod-cell/TermuOS"

echo "[updater] signal received, downloading latest release..."
pkill -f "qemu.*termuos.iso" || true
sleep 1

# Download latest ISO from GitHub releases
curl -L "https://github.com/$REPO/releases/latest/download/termuos.iso" -o termuos.iso

echo "[updater] restarting..."
make run