#pragma once

#ifdef WITH_QT_GUI

#include <QtWidgets/QApplication>
#include <memory>

class MainWindow;
class Controller;

class GuiApplication : public QApplication
{
    Q_OBJECT

public:
    GuiApplication(int &argc, char **argv);
    ~GuiApplication();

    void setController(Controller* controller);
    void showMainWindow();

private:
    std::unique_ptr<MainWindow> m_mainWindow;
    Controller* m_controller;
};

#endif // WITH_QT_GUI