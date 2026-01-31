#!/bin/bash
CONTAINER_VERSION="${VERSION:-unknown}"
. /etc/os-release
echo "======================="
echo "One ROM Build Container"
echo "======================="
echo ""
echo "Built:    ${BUILD_DATE:-unknown}"
echo "Version:  ${CONTAINER_VERSION}"
echo "Git Hash: ${VCS_REF:-unknown}"
echo ""
echo "Dist:     ${PRETTY_NAME}"
echo "ARM GCC:  $(/opt/arm-toolchain/bin/arm-none-eabi-gcc --version 2>/dev/null | head -n1 || echo 'not found')"
echo "Rust:     $(rustc --version 2>/dev/null || echo 'not found')"
echo "Cargo:    $(cargo --version 2>/dev/null || echo 'not found')"
echo "probe-rs: $(probe-rs --version 2>/dev/null || echo 'not found')"
echo "picotool: $(picotool version 2>/dev/null || echo 'not found')"
echo ""

cat << 'EOF'
To get started:
  ./clone.sh && cd one-rom

Build firmware:
  scripts/onerom.sh <board> <config-file>

Examples:
  scripts/onerom.sh fire-24-d onerom-config/vic20-pal.json
  scripts/onerom.sh ice-24-j onerom-config/c64.json

Copy firmware to output directory:
  ./copy-fw.sh
EOF

echo ""
echo "======================="