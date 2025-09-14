#ifdef WITH_QT_GUI

#include "GuiApplication.h"
#include "MainWindow.h"
#include "../core/Controller.h"

GuiApplication::GuiApplication(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_controller(nullptr)
{
    setApplicationName("XMRDesk");
    setApplicationVersion("1.0.0");
    setApplicationDisplayName("XMRDesk - Monero Mining GUI");
    setOrganizationName("XMRDesk");
}

GuiApplication::~GuiApplication()
{
}

void GuiApplication::setController(Controller* controller)
{
    m_controller = controller;
}

void GuiApplication::showMainWindow()
{
    if (!m_controller) {
        return;
    }

    m_mainWindow = std::make_unique<MainWindow>(m_controller);
    m_mainWindow->show();
}

#endif // WITH_QT_GUI