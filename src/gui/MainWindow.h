#pragma once

#ifdef WITH_QT_GUI

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLineEdit>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QTimer>

class Miner;
class Controller;
class CpuTempMonitor;
class CpuOptimizer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Controller* controller, QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void updateHashrate(double hashrate);
    void updateCpuTemperature(double temperature);
    void updateMiningStatus(const QString& status);
    void onStartMining();
    void onStopMining();
    void onPoolChanged();

private:
    void setupUI();
    void setupHashrateChart();
    void setupPoolConfiguration();
    void setupMiningControls();
    void setupStatusDisplay();
    void setupCpuInfo();

private:
    Controller* m_controller;

    // Main layout
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;

    // Hashrate monitoring
    QGroupBox* m_hashrateGroup;
    QChartView* m_hashrateChartView;
    QChart* m_hashrateChart;
    QLineSeries* m_hashrateSeries;
    QValueAxis* m_axisX;
    QValueAxis* m_axisY;
    QLabel* m_currentHashrateLabel;
    QTimer* m_updateTimer;

    // CPU temperature
    QLabel* m_cpuTempLabel;
    QProgressBar* m_cpuTempProgress;

    // Pool configuration
    QGroupBox* m_poolGroup;
    QComboBox* m_poolCombo;
    QLineEdit* m_walletEdit;
    QLineEdit* m_workerEdit;

    // Mining controls
    QGroupBox* m_controlGroup;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QLabel* m_statusLabel;

    // Log display
    QGroupBox* m_logGroup;
    QTextEdit* m_logDisplay;

    // CPU temperature monitor
    CpuTempMonitor* m_cpuTempMonitor;

    // CPU optimizer
    CpuOptimizer* m_cpuOptimizer;
    QLabel* m_cpuInfoLabel;
    QLabel* m_optimizationLabel;

    // Data tracking
    QList<QPointF> m_hashrateData;
    int m_timeCounter;
    bool m_isMining;
};

#endif // WITH_QT_GUI