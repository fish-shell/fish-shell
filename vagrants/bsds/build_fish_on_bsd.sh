#!/bin/bash
# Build fish on BSD VMs
# Usage: ./build_fish.sh freebsd_14_0 openbsd_7_4 netbsd_9_3 dragonflybsd_6_4

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default to "4.x.x" version if not set
FISH_BUILD_VERSION="${FISH_BUILD_VERSION:-4.x.x}"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <bsd_dir1> [bsd_dir2] ..."
    echo "Example: $0 freebsd_14_0 openbsd_7_4"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FAILED_VMS=()
SUCCESS_VMS=()

for BSD_DIR in "$@"; do
    if [ ! -d "$SCRIPT_DIR/$BSD_DIR" ]; then
        echo -e "${RED}Error: Directory $BSD_DIR does not exist${NC}"
        continue
    fi

    echo ""
    echo "=========================================="
    echo -e "${YELLOW}Processing: $BSD_DIR${NC}"
    echo "=========================================="

    cd "$SCRIPT_DIR/$BSD_DIR"

    echo "Starting VM..."
    vagrant up

    echo "Syncing files..."
    vagrant rsync

    echo "Building fish..."
    if vagrant ssh -c "cd /home/vagrant/fish-shell && FISH_BUILD_VERSION=$FISH_BUILD_VERSION cargo build" 2>&1; then
        echo -e "${GREEN}✓ Build succeeded for $BSD_DIR${NC}"
        SUCCESS_VMS+=("$BSD_DIR")

        echo "Halting VM..."
        vagrant halt
    else
        echo ""
        echo -e "${RED}✗ Build FAILED for $BSD_DIR${NC}"
        echo -e "${YELLOW}VM is still running. To investigate:${NC}"
        echo -e "  cd $SCRIPT_DIR/$BSD_DIR"
        echo -e "  vagrant ssh"
        echo -e "  cd /home/vagrant/fish-shell"
        echo -e "  cargo build"
        echo ""
        echo -e "${YELLOW}To halt the VM when done:${NC}"
        echo -e "  cd $SCRIPT_DIR/$BSD_DIR && vagrant halt"
        echo ""
        FAILED_VMS+=("$BSD_DIR")
    fi
done

echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="

if [ ${#SUCCESS_VMS[@]} -gt 0 ]; then
    echo -e "${GREEN}Successful builds (${#SUCCESS_VMS[@]}):${NC}"
    for vm in "${SUCCESS_VMS[@]}"; do
        echo -e "  ${GREEN}✓${NC} $vm"
    done
fi

if [ ${#FAILED_VMS[@]} -gt 0 ]; then
    echo ""
    echo -e "${RED}Failed builds (${#FAILED_VMS[@]}):${NC}"
    for vm in "${FAILED_VMS[@]}"; do
        echo -e "  ${RED}✗${NC} $vm (VM still running)"
    done
    echo ""
    echo -e "${YELLOW}To halt all failed VMs:${NC}"
    for vm in "${FAILED_VMS[@]}"; do
        echo "  cd $SCRIPT_DIR/$vm && vagrant halt"
    done
fi

if [ ${#FAILED_VMS[@]} -eq 0 ]; then
    echo -e "\n${GREEN}All builds completed successfully!${NC}"
    exit 0
else
    echo ""
    exit 1
fi
