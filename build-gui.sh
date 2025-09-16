#!/bin/bash

echo "🚀 Building XMRDesk Windows GUI..."

# Create build directory
mkdir -p build-gui

# Compile GUI version with proper Windows libraries
x86_64-w64-mingw32-g++ \
    -o build-gui/xmrdesk-gui.exe \
    xmrdesk-gui.cpp \
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
    -D_UNICODE

if [ $? -eq 0 ]; then
    echo "✅ GUI Build erfolgreich!"
    echo "📁 Output: build-gui/xmrdesk-gui.exe"
    ls -la build-gui/xmrdesk-gui.exe
else
    echo "❌ Build fehlgeschlagen!"
    exit 1
fi

# Create GUI release package
echo "📦 Creating GUI release package..."
mkdir -p xmrdesk-gui-release
cp build-gui/xmrdesk-gui.exe xmrdesk-gui-release/

# Create README for GUI
cat > xmrdesk-gui-release/README-GUI.txt << 'EOF'
XMRDesk GUI v1.0.0 - AMD Ryzen Optimized Mining
================================================

INSTALLATION:
1. Starte xmrdesk-gui.exe direkt (keine .bat Dateien nötig)
2. Das GUI öffnet sich automatisch

FEATURES:
✅ Native Windows GUI Interface
✅ AMD Ryzen Zen 1-5 Optimierungen
✅ CPU-Erkennung und automatische Konfiguration
✅ Pool-Auswahl (SupportXMR, Nanopool, etc.)
✅ Echtzeit Hashrate und Temperatur Anzeige
✅ Ein-Klick Mining Start/Stop

VERWENDUNG:
1. Pool auswählen (Standard: SupportXMR.com)
2. Wallet-Adresse eingeben (oder Standard-Donation verwenden)
3. CPU Threads anpassen (automatisch erkannt)
4. "Mining Starten" klicken

DONATION ADDRESS:
48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL

GitHub: github.com/speteai/xmrdesk
EOF

# Copy license
cp LICENSE xmrdesk-gui-release/ 2>/dev/null || echo "LICENSE nicht gefunden"

# Create ZIP package
cd xmrdesk-gui-release
zip -r ../xmrdesk-gui-v1.0.0.zip .
cd ..

echo "🎉 GUI Release package erstellt!"
echo "📦 xmrdesk-gui-v1.0.0.zip"
ls -la xmrdesk-gui-v1.0.0.zip