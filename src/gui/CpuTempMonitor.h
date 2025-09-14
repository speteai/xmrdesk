#pragma once

#ifdef WITH_QT_GUI

#include <QObject>
#include <QTimer>

class CpuTempMonitor : public QObject
{
    Q_OBJECT

public:
    explicit CpuTempMonitor(QObject *parent = nullptr);
    ~CpuTempMonitor();

    void startMonitoring();
    void stopMonitoring();
    double getCurrentTemperature() const { return m_currentTemp; }

signals:
    void temperatureChanged(double temperature);

private slots:
    void updateTemperature();

private:
    double readCpuTemperature();

    QTimer* m_timer;
    double m_currentTemp;
    bool m_monitoring;
};

#endif // WITH_QT_GUI