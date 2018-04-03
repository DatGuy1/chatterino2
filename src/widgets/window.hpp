#pragma once

#include "util/helpers.hpp"
#include "widgets/basewindow.hpp"
#include "widgets/notebook.hpp"

//#ifdef USEWINSDK
//#include <platform/borderless/qwinwidget.h>
//#endif

#include <pajlada/settings/setting.hpp>
#include <pajlada/signals/signal.hpp>

namespace chatterino {
namespace singletons {
class ThemeManager;
}  // namespace singletons

namespace widgets {

struct WindowGeometry {
    WindowGeometry(const std::string &settingPrefix)
        : x(fS("{}/geometry/x", settingPrefix))
        , y(fS("{}/geometry/y", settingPrefix))
        , width(fS("{}/geometry/width", settingPrefix))
        , height(fS("{}/geometry/height", settingPrefix))
    {
    }

    pajlada::Settings::Setting<int> x;
    pajlada::Settings::Setting<int> y;
    pajlada::Settings::Setting<int> width;
    pajlada::Settings::Setting<int> height;
};

class Window : public BaseWindow
{
    Q_OBJECT

    std::string settingRoot;

    WindowGeometry windowGeometry;

public:
    explicit Window(const QString &windowName, singletons::ThemeManager &_themeManager,
                    bool isMainWindow);

    void repaintVisibleChatWidgets(Channel *channel = nullptr);

    Notebook &getNotebook();

    void refreshWindowTitle(const QString &username);

    pajlada::Signals::NoArgSignal closed;

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

private:
    float dpi;

    void loadGeometry();

    Notebook notebook;
    // TitleBar titleBar;

    friend class Notebook;

public:
    void save();
};

}  // namespace widgets
}  // namespace chatterino
