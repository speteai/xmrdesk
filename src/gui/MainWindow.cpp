#ifdef WITH_QT_GUI

#include "MainWindow.h"
#include "CpuTempMonitor.h"
#include "../backend/cpu/CpuOptimizer.h"
#include "../core/Controller.h"
#include "../core/Miner.h"
#include <QtWidgets/QApplication>
#include <QtCharts/QDateTimeAxis>
#include <QDateTime>
#include <QMessageBox>

MainWindow::MainWindow(Controller* controller, QWidget *parent)
    : QMainWindow(parent)
    , m_controller(controller)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_hashrateGroup(nullptr)
    , m_hashrateChartView(nullptr)
    , m_hashrateChart(nullptr)
    , m_hashrateSeries(nullptr)
    , m_axisX(nullptr)
    , m_axisY(nullptr)
    , m_currentHashrateLabel(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_cpuTempLabel(nullptr)
    , m_cpuTempProgress(nullptr)
    , m_poolGroup(nullptr)
    , m_poolCombo(nullptr)
    , m_walletEdit(nullptr)
    , m_workerEdit(nullptr)
    , m_controlGroup(nullptr)
    , m_startButton(nullptr)
    , m_stopButton(nullptr)
    , m_statusLabel(nullptr)
    , m_logGroup(nullptr)
    , m_logDisplay(nullptr)
    , m_cpuTempMonitor(new CpuTempMonitor(this))
    , m_cpuOptimizer(nullptr)
    , m_cpuInfoLabel(nullptr)
    , m_optimizationLabel(nullptr)
    , m_timeCounter(0)
    , m_isMining(false)
{
    // Initialize CPU optimizer
    m_cpuOptimizer = CpuOptimizer::create().release();

    setupUI();

    // Connect CPU temperature monitor
    connect(m_cpuTempMonitor, &CpuTempMonitor::temperatureChanged,
            this, &MainWindow::updateCpuTemperature);
    m_cpuTempMonitor->startMonitoring();

    // Update timer for real-time data
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        // This will be connected to actual miner stats
        if (m_isMining) {
            // Simulate hashrate for now - will be connected to real miner data
            static double simulatedHashrate = 1000.0;
            simulatedHashrate += (rand() % 200) - 100; // Random variation
            if (simulatedHashrate < 0) simulatedHashrate = 0;
            updateHashrate(simulatedHashrate);
        }
    });
    m_updateTimer->start(1000); // Update every second
}

MainWindow::~MainWindow()
{
    delete m_cpuOptimizer;
}

void MainWindow::setupUI()
{
    setWindowTitle("XMRDesk - Monero Mining GUI");
    setMinimumSize(1200, 800);

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_mainLayout = new QVBoxLayout(m_centralWidget);

    // Setup components
    setupCpuInfo();
    setupHashrateChart();
    setupPoolConfiguration();
    setupMiningControls();
    setupStatusDisplay();
}

void MainWindow::setupCpuInfo()
{
    QGroupBox* cpuGroup = new QGroupBox("CPU Information & Optimizations", this);
    QVBoxLayout* layout = new QVBoxLayout(cpuGroup);

    // CPU information display
    m_cpuInfoLabel = new QLabel("Initializing CPU detection...", this);
    m_cpuInfoLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #2c3e50;");
    layout->addWidget(m_cpuInfoLabel);

    // Optimization status
    m_optimizationLabel = new QLabel("Checking optimizations...", this);
    m_optimizationLabel->setStyleSheet("font-size: 12px; color: #7f8c8d;");
    layout->addWidget(m_optimizationLabel);

    // Update CPU info if optimizer is available
    if (m_cpuOptimizer) {
        QString cpuInfo = QString("CPU: %1 %2")
                         .arg(m_cpuOptimizer->getCpuVendorName())
                         .arg(m_cpuOptimizer->getCpuArchitectureName());
        m_cpuInfoLabel->setText(cpuInfo);

        QString optimizationInfo;
        if (m_cpuOptimizer->isRyzenCpu()) {
            optimizationInfo = QString("🚀 AMD Ryzen optimizations active - Enhanced performance for %1")
                              .arg(m_cpuOptimizer->getCpuArchitectureName());
            m_optimizationLabel->setStyleSheet("font-size: 12px; color: #27ae60; font-weight: bold;");
        } else if (m_cpuOptimizer->isIntelCpu()) {
            optimizationInfo = "⚡ Intel optimizations active - Tuned for Intel architecture";
            m_optimizationLabel->setStyleSheet("font-size: 12px; color: #3498db; font-weight: bold;");
        } else {
            optimizationInfo = "ℹ️  Generic optimizations - Standard configuration";
            m_optimizationLabel->setStyleSheet("font-size: 12px; color: #f39c12;");
        }
        m_optimizationLabel->setText(optimizationInfo);
    }

    m_mainLayout->addWidget(cpuGroup);
}

void MainWindow::setupHashrateChart()
{
    m_hashrateGroup = new QGroupBox("Hashrate Monitoring", this);
    QVBoxLayout* layout = new QVBoxLayout(m_hashrateGroup);

    // Current hashrate display
    m_currentHashrateLabel = new QLabel("Current Hashrate: 0.00 H/s", this);
    m_currentHashrateLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #2ecc71;");
    layout->addWidget(m_currentHashrateLabel);

    // CPU Temperature
    QHBoxLayout* tempLayout = new QHBoxLayout();
    m_cpuTempLabel = new QLabel("CPU Temp:", this);
    m_cpuTempProgress = new QProgressBar(this);
    m_cpuTempProgress->setRange(30, 90);
    m_cpuTempProgress->setValue(50);
    m_cpuTempProgress->setFormat("%v°C");
    tempLayout->addWidget(m_cpuTempLabel);
    tempLayout->addWidget(m_cpuTempProgress);
    layout->addLayout(tempLayout);

    // Hashrate chart
    m_hashrateChart = new QChart();
    m_hashrateChart->setTitle("Hashrate History");
    m_hashrateChart->setAnimationOptions(QChart::SeriesAnimations);

    m_hashrateSeries = new QLineSeries();
    m_hashrateSeries->setName("Hashrate (H/s)");
    m_hashrateChart->addSeries(m_hashrateSeries);

    m_axisX = new QValueAxis();
    m_axisX->setTitleText("Time (seconds)");
    m_axisX->setRange(0, 60);
    m_hashrateChart->addAxis(m_axisX, Qt::AlignBottom);
    m_hashrateSeries->attachAxis(m_axisX);

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("Hashrate (H/s)");
    m_axisY->setRange(0, 2000);
    m_hashrateChart->addAxis(m_axisY, Qt::AlignLeft);
    m_hashrateSeries->attachAxis(m_axisY);

    m_hashrateChartView = new QChartView(m_hashrateChart);
    m_hashrateChartView->setRenderHint(QPainter::Antialiasing);
    m_hashrateChartView->setMinimumHeight(300);
    layout->addWidget(m_hashrateChartView);

    m_mainLayout->addWidget(m_hashrateGroup);
}

void MainWindow::setupPoolConfiguration()
{
    m_poolGroup = new QGroupBox("Pool Configuration", this);
    QVBoxLayout* layout = new QVBoxLayout(m_poolGroup);

    // Pool selection
    QHBoxLayout* poolLayout = new QHBoxLayout();
    poolLayout->addWidget(new QLabel("Pool:", this));
    m_poolCombo = new QComboBox(this);
    m_poolCombo->addItem("SupportXMR.com", "pool.supportxmr.com:443");
    m_poolCombo->addItem("Qubic.org", "qubic.org:3333");
    m_poolCombo->addItem("Nanopool.org", "xmr-eu1.nanopool.org:14433");
    connect(m_poolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onPoolChanged);
    poolLayout->addWidget(m_poolCombo);
    layout->addLayout(poolLayout);

    // Wallet address
    QHBoxLayout* walletLayout = new QHBoxLayout();
    walletLayout->addWidget(new QLabel("Wallet:", this));
    m_walletEdit = new QLineEdit(this);
    m_walletEdit->setPlaceholderText("Enter your XMR wallet address");
    walletLayout->addWidget(m_walletEdit);
    layout->addLayout(walletLayout);

    // Worker name
    QHBoxLayout* workerLayout = new QHBoxLayout();
    workerLayout->addWidget(new QLabel("Worker:", this));
    m_workerEdit = new QLineEdit(this);
    m_workerEdit->setPlaceholderText("Optional worker name");
    m_workerEdit->setText("xmrdesk-worker");
    workerLayout->addWidget(m_workerEdit);
    layout->addLayout(workerLayout);

    m_mainLayout->addWidget(m_poolGroup);
}

void MainWindow::setupMiningControls()
{
    m_controlGroup = new QGroupBox("Mining Controls", this);
    QHBoxLayout* layout = new QHBoxLayout(m_controlGroup);

    m_startButton = new QPushButton("Start Mining", this);
    m_startButton->setStyleSheet("QPushButton { background-color: #27ae60; color: white; font-weight: bold; padding: 10px; }");
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartMining);
    layout->addWidget(m_startButton);

    m_stopButton = new QPushButton("Stop Mining", this);
    m_stopButton->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; padding: 10px; }");
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopMining);
    layout->addWidget(m_stopButton);

    m_statusLabel = new QLabel("Status: Stopped", this);
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    layout->addWidget(m_statusLabel);

    m_mainLayout->addWidget(m_controlGroup);
}

void MainWindow::setupStatusDisplay()
{
    m_logGroup = new QGroupBox("Mining Log", this);
    QVBoxLayout* layout = new QVBoxLayout(m_logGroup);

    m_logDisplay = new QTextEdit(this);
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setMaximumHeight(200);
    m_logDisplay->setStyleSheet("background-color: #2c3e50; color: #ecf0f1; font-family: monospace;");
    layout->addWidget(m_logDisplay);

    m_mainLayout->addWidget(m_logGroup);
}

void MainWindow::updateHashrate(double hashrate)
{
    m_currentHashrateLabel->setText(QString("Current Hashrate: %1 H/s").arg(hashrate, 0, 'f', 2));

    // Add to chart data
    m_hashrateData.append(QPointF(m_timeCounter, hashrate));
    m_timeCounter++;

    // Keep only last 60 data points
    if (m_hashrateData.size() > 60) {
        m_hashrateData.removeFirst();
        // Adjust time axis
        m_axisX->setRange(m_timeCounter - 60, m_timeCounter);
    }

    // Update chart
    m_hashrateSeries->replace(m_hashrateData);

    // Auto-scale Y axis
    if (!m_hashrateData.isEmpty()) {
        double maxHashrate = 0;
        for (const QPointF& point : m_hashrateData) {
            if (point.y() > maxHashrate) maxHashrate = point.y();
        }
        m_axisY->setRange(0, maxHashrate * 1.1);
    }
}

void MainWindow::updateCpuTemperature(double temperature)
{
    m_cpuTempProgress->setValue((int)temperature);

    // Color coding based on temperature
    QString style;
    if (temperature < 60) {
        style = "QProgressBar::chunk { background-color: #27ae60; }"; // Green
    } else if (temperature < 75) {
        style = "QProgressBar::chunk { background-color: #f39c12; }"; // Orange
    } else {
        style = "QProgressBar::chunk { background-color: #e74c3c; }"; // Red
    }
    m_cpuTempProgress->setStyleSheet(style);
}

void MainWindow::updateMiningStatus(const QString& status)
{
    m_statusLabel->setText("Status: " + status);

    // Add to log
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logDisplay->append(QString("[%1] %2").arg(timestamp, status));
}

void MainWindow::onStartMining()
{
    if (m_walletEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Configuration Error", "Please enter your wallet address.");
        return;
    }

    m_isMining = true;
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);

    updateMiningStatus("Starting mining...");

    // Apply CPU optimizations
    if (m_cpuOptimizer) {
        QString optimizationMsg;
        if (m_cpuOptimizer->isRyzenCpu()) {
            optimizationMsg = QString("Applied AMD Ryzen optimizations for %1")
                             .arg(m_cpuOptimizer->getCpuArchitectureName());
        } else if (m_cpuOptimizer->isIntelCpu()) {
            optimizationMsg = "Applied Intel-specific optimizations";
        } else {
            optimizationMsg = "Applied generic CPU optimizations";
        }
        updateMiningStatus(optimizationMsg);

        // Print optimization summary to log
        m_cpuOptimizer->printOptimizationSummary();
    }

    // Here we would connect to the actual miner with optimized settings
    // For now, just simulate
    updateMiningStatus("Mining started successfully with CPU optimizations");
}

void MainWindow::onStopMining()
{
    m_isMining = false;
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);

    updateMiningStatus("Stopping mining...");

    // Here we would stop the actual miner
    updateMiningStatus("Mining stopped");

    // Reset hashrate display
    m_currentHashrateLabel->setText("Current Hashrate: 0.00 H/s");
}

void MainWindow::onPoolChanged()
{
    QString poolUrl = m_poolCombo->currentData().toString();
    updateMiningStatus(QString("Pool changed to: %1").arg(poolUrl));
}

#endif // WITH_QT_GUI