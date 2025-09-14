/* XMRDesk
 * Copyright (c) 2018-2021 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2021 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 * Copyright (c) 2024 speteai          <https://github.com/speteai>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "App.h"
#include "base/kernel/Entry.h"
#include "base/kernel/Process.h"

#ifdef WITH_QT_GUI
#include "gui/GuiApplication.h"
#include <memory>
#endif

int main(int argc, char **argv)
{
    using namespace xmrig;

    Process process(argc, argv);
    const Entry::Id entry = Entry::get(process);
    if (entry) {
        return Entry::exec(process, entry);
    }

#ifdef WITH_QT_GUI
    // Check if GUI mode should be used (default to GUI unless --no-gui is specified)
    bool useGui = true;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-gui") == 0 || strcmp(argv[i], "--console") == 0) {
            useGui = false;
            break;
        }
    }

    if (useGui) {
        GuiApplication guiApp(argc, argv);

        // Create app and get controller
        App app(&process);
        guiApp.setController(app.getController().get());
        guiApp.showMainWindow();

        return guiApp.exec();
    }
#endif

    // Fallback to console mode
    App app(&process);
    return app.exec();
}
