@echo off
title XMRDesk Professional v5.0.0 Final - NiceHash Style
color 0E
echo.
echo ========================================================
echo      XMRDesk Professional v5.0.0 Final
echo         NiceHash Style with Real XMRig
echo ========================================================
echo.
echo   🎯 PROFESSIONAL MINING SOLUTION 🎯
echo.
echo   ✅ NiceHash-inspired professional interface
echo   ✅ Real XMRig v6.24.0 integration
echo   ✅ Wallet address input and validation
echo   ✅ Pool selection with auto-port detection
echo   ✅ Real-time mining log and statistics
echo   ✅ Share found/rejected notifications
echo   ✅ Custom donation wallet integrated
echo.
echo   FEATURES:
echo   • Professional GUI inspired by NiceHash
echo   • Latest XMRig mining engine (unmodified)
echo   • 6 premium mining pools preconfigured
echo   • Real-time hashrate and share monitoring
echo   • Thread configuration and status display
echo   • Complete mining log with timestamps
echo.
echo   DONATION WALLET:
echo   48ckezCUYfnj3vDtQRtH1X4ExpvowjRBJj7U36P13KbDPeZ6M3Pjuev3xdCkXPuAuQNomuSDXTBZFTkQRfP33t3gMTjJCpL
echo.
echo   INCLUDED FILES:
echo   • xmrdesk-nicehash-style.exe - Professional GUI
echo   • xmrig.exe - Official XMRig v6.24.0
echo   • Complete source code and documentation
echo.
echo   ⚠️ REAL MINING - NOT SIMULATION ⚠️
echo   • 100%% CPU usage during mining
echo   • Actual pool connections and shares
echo   • Real RandomX algorithm for Monero
echo   • Genuine XMRig performance
echo.
echo   Starting professional miner...
echo.
timeout /t 3 /nobreak >nul

if exist "xmrdesk-nicehash-style.exe" (
    if exist "xmrig.exe" (
        start xmrdesk-nicehash-style.exe
        echo   ✅ Professional miner started successfully!
        echo.
        echo   USAGE:
        echo   1. Enter your Monero wallet address
        echo   2. Select mining pool (SupportXMR recommended)
        echo   3. Configure thread count
        echo   4. Click "Start Mining"
        echo   5. Monitor real-time statistics and log
        echo.
    ) else (
        echo   ❌ xmrig.exe not found!
        echo      Please ensure both files are in the same folder.
    )
) else (
    echo   ❌ xmrdesk-nicehash-style.exe not found!
    echo      Please ensure all files are extracted properly.
)

echo.
echo   For detailed instructions see README-PROFESSIONAL-v5.0.0-FINAL.txt
echo.
pause