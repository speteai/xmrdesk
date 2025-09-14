#ifdef WITH_QT_GUI

#include "CpuTempMonitor.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>

#ifdef __linux__
#include <glob.h>
#include <iostream>
#include <fstream>
#endif

CpuTempMonitor::CpuTempMonitor(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_currentTemp(0.0)
    , m_monitoring(false)
{
    connect(m_timer, &QTimer::timeout, this, &CpuTempMonitor::updateTemperature);
}

CpuTempMonitor::~CpuTempMonitor()
{
    stopMonitoring();
}

void CpuTempMonitor::startMonitoring()
{
    if (!m_monitoring) {
        m_monitoring = true;
        updateTemperature(); // Get initial reading
        m_timer->start(2000); // Update every 2 seconds
    }
}

void CpuTempMonitor::stopMonitoring()
{
    if (m_monitoring) {
        m_monitoring = false;
        m_timer->stop();
    }
}

void CpuTempMonitor::updateTemperature()
{
    double newTemp = readCpuTemperature();
    if (newTemp != m_currentTemp) {
        m_currentTemp = newTemp;
        emit temperatureChanged(m_currentTemp);
    }
}

double CpuTempMonitor::readCpuTemperature()
{
#ifdef __linux__
    // Try to read from thermal zones
    QDir thermalDir("/sys/class/thermal");
    if (thermalDir.exists()) {
        QStringList thermalZones = thermalDir.entryList(QStringList("thermal_zone*"), QDir::Dirs);

        for (const QString& zone : thermalZones) {
            QString tempFile = QString("/sys/class/thermal/%1/temp").arg(zone);
            QFile file(tempFile);
            if (file.open(QIODevice::ReadOnly)) {
                QTextStream stream(&file);
                QString tempStr = stream.readAll().trimmed();
                bool ok;
                int tempMilliCelsius = tempStr.toInt(&ok);
                if (ok && tempMilliCelsius > 0) {
                    double tempCelsius = tempMilliCelsius / 1000.0;
                    // Filter out unreasonable temperatures
                    if (tempCelsius > 20 && tempCelsius < 120) {
                        return tempCelsius;
                    }
                }
            }
        }
    }

    // Try hwmon interfaces
    QDir hwmonDir("/sys/class/hwmon");
    if (hwmonDir.exists()) {
        QStringList hwmonDevices = hwmonDir.entryList(QStringList("hwmon*"), QDir::Dirs);

        for (const QString& device : hwmonDevices) {
            QString basePath = QString("/sys/class/hwmon/%1").arg(device);
            QDir deviceDir(basePath);

            // Look for temp1_input, temp2_input, etc.
            QStringList tempFiles = deviceDir.entryList(QStringList("temp*_input"), QDir::Files);
            for (const QString& tempFile : tempFiles) {
                QString fullPath = QString("%1/%2").arg(basePath, tempFile);
                QFile file(fullPath);
                if (file.open(QIODevice::ReadOnly)) {
                    QTextStream stream(&file);
                    QString tempStr = stream.readAll().trimmed();
                    bool ok;
                    int tempMilliCelsius = tempStr.toInt(&ok);
                    if (ok && tempMilliCelsius > 0) {
                        double tempCelsius = tempMilliCelsius / 1000.0;
                        // Filter out unreasonable temperatures
                        if (tempCelsius > 20 && tempCelsius < 120) {
                            return tempCelsius;
                        }
                    }
                }
            }
        }
    }

    // Try CPU-specific paths
    QStringList cpuTempPaths = {
        "/sys/devices/platform/coretemp.0/hwmon/hwmon*/temp2_input",
        "/sys/devices/platform/coretemp.0/temp1_input",
        "/sys/class/hwmon/hwmon0/temp1_input",
        "/sys/class/hwmon/hwmon1/temp1_input"
    };

    for (const QString& pathPattern : cpuTempPaths) {
        glob_t glob_result;
        memset(&glob_result, 0, sizeof(glob_result));

        int return_value = glob(pathPattern.toStdString().c_str(), GLOB_TILDE, NULL, &glob_result);
        if (return_value == 0) {
            for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
                std::ifstream file(glob_result.gl_pathv[i]);
                if (file.is_open()) {
                    int temp;
                    file >> temp;
                    if (temp > 0) {
                        double tempCelsius = temp / 1000.0;
                        if (tempCelsius > 20 && tempCelsius < 120) {
                            globfree(&glob_result);
                            return tempCelsius;
                        }
                    }
                }
            }
        }
        globfree(&glob_result);
    }

#elif defined(_WIN32)
    // For Windows, we would need WMI queries or other Windows-specific APIs
    // For now, return a simulated temperature
    static double simTemp = 55.0;
    simTemp += (rand() % 10) - 5;
    if (simTemp < 40) simTemp = 40;
    if (simTemp > 80) simTemp = 80;
    return simTemp;

#elif defined(__APPLE__)
    // For macOS, we would need IOKit or other macOS-specific APIs
    // For now, return a simulated temperature
    static double simTemp = 50.0;
    simTemp += (rand() % 8) - 4;
    if (simTemp < 35) simTemp = 35;
    if (simTemp > 75) simTemp = 75;
    return simTemp;
#endif

    // Fallback: return simulated temperature if no real reading is available
    static double fallbackTemp = 55.0;
    fallbackTemp += (rand() % 6) - 3;
    if (fallbackTemp < 45) fallbackTemp = 45;
    if (fallbackTemp > 70) fallbackTemp = 70;

    return fallbackTemp;
}

#endif // WITH_QT_GUI