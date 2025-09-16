#!/bin/bash

echo "🚀 Building Final XMRDesk Windows GUI v2..."

# Clean previous builds
rm -rf build-gui-final xmrdesk-gui-final

# Create build directory
mkdir -p build-gui-final

# Compile improved GUI version with better error handling
x86_64-w64-mingw32-g++ \
    -o build-gui-final/xmrdesk.exe \
    xmrdesk-gui-v2.cpp \
    -static \
    -static-libgcc \
    -static-libstdc++ \
    -lcomctl32 \
    -lshell32 \
    -luser32 \
    -lgdi32 \
    -lkernel32 \
    -mwindows \
    -O2 \
    -DUNICODE \
    -D_UNICODE \
    -fno-exceptions

if [ $? -eq 0 ]; then
    echo "✅ GUI Build successful!"
    echo "📁 Output: build-gui-final/xmrdesk.exe"
    ls -la build-gui-final/xmrdesk.exe
else
    echo "❌ Build failed!"
    exit 1
fi

# Create final release package
echo "📦 Creating final release package..."
mkdir -p xmrdesk-gui-final
cp build-gui-final/xmrdesk.exe xmrdesk-gui-final/

# Create improved README
cat > xmrdesk-gui-final/README.txt << 'EOF'
XMRDesk v1.0.0 - AMD Ryzen Optimized Mining
===========================================

QUICK START:
1. Double-click xmrdesk.exe
2. Select your mining pool
3. Enter your wallet address
4. Click "Start Mining"

FEATURES:
✅ Professional Windows GUI
✅ AMD Ryzen Zen 1-5 Optimizations
✅ CPU Detection & Auto-Configuration
✅ Popular Mining Pools Pre-configured
✅ Real-time Hashrate & Temperature
✅ One-Click Mining Control
✅ Crash-resistant Design

SUPPORTED POOLS:
• SupportXMR.com (Recommended)
• Nanopool.org
• MineXMR.com
• MoneroOcean.stream
• F2Pool

SYSTEM REQUIREMENTS:
• Windows 10/11 (64-bit)
• 4GB+ RAM
• Internet Connection
• AMD Ryzen CPU (Recommended)

DONATION ADDRESS:
48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL

PROJECT:
GitHub: https://github.com/speteai/xmrdesk
Version: 1.0.0
Build: Final Windows GUI
EOF

# Copy license
cp LICENSE xmrdesk-gui-final/ 2>/dev/null || echo "LICENSE not found"

# Create ZIP package
zip -r xmrdesk-final-v1.0.0.zip xmrdesk-gui-final/

echo ""
echo "🎉 Final Release Package Created!"
echo "📦 File: xmrdesk-final-v1.0.0.zip"
echo "💾 Size: $(du -h xmrdesk-final-v1.0.0.zip | cut -f1)"
echo ""
echo "✅ READY FOR DISTRIBUTION!"

ls -la xmrdesk-final-v1.0.0.zip