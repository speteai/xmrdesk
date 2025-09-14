@echo off
title XMRDesk - Console Mining

echo.
echo  ╔═══════════════════════════════════════════════════════════════╗
echo  ║                        XMRDesk v1.0.0                        ║
echo  ║                    Console Mining Mode                        ║
echo  ╚═══════════════════════════════════════════════════════════════╝
echo.
echo ⚡ High-performance Monero mining for advanced users
echo 📊 This version runs in console mode (no GUI)
echo 💻 Perfect for: Remote servers, automated setups, minimal overhead
echo.
echo ═══════════════════════════════════════════════════════════════════
echo.
echo 🔧 Configuration:
echo    Edit config.json for advanced settings
echo    Or use command-line parameters
echo.
echo 🏊 Supported pools:
echo    • SupportXMR.com (Recommended)
echo    • Qubic.org
echo    • Nanopool.org
echo.
echo 💡 For GUI version, use: start.bat (recommended for beginners)
echo.
echo Starting XMRDesk Console...
echo.

xmrdesk.exe --no-gui %*

echo.
echo Mining session ended. Press any key to exit...
pause >nul