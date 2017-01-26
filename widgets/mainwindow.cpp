#include "widgets/mainwindow.h"
#include "colorscheme.h"
#include "widgets/chatwidget.h"
#include "widgets/notebook.h"

#include <QPalette>

namespace chatterino {
namespace widgets {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , notebook(this)
{
    setCentralWidget(&this->notebook);

    QPalette palette;
    palette.setColor(QPalette::Background,
                     ColorScheme::getInstance().TabPanelBackground);
    setPalette(palette);

    resize(1280, 800);
}

MainWindow::~MainWindow()
{
}

void
MainWindow::layoutVisibleChatWidgets(Channel *channel)
{
    auto *page = notebook.getSelectedPage();

    if (page == NULL) {
        return;
    }

    const std::vector<ChatWidget *> &widgets = page->getChatWidgets();

    for (auto it = widgets.begin(); it != widgets.end(); ++it) {
        ChatWidget *widget = *it;

        if (channel == NULL || channel == widget->getChannel()) {
            if (widget->getView().layoutMessages()) {
                widget->update();
            }
        }
    }
}

void
MainWindow::repaintVisibleChatWidgets(Channel *channel)
{
    auto *page = notebook.getSelectedPage();

    if (page == NULL) {
        return;
    }

    const std::vector<ChatWidget *> &widgets = page->getChatWidgets();

    for (auto it = widgets.begin(); it != widgets.end(); ++it) {
        ChatWidget *widget = *it;

        if (channel == NULL || channel == widget->getChannel()) {
            widget->getView().layoutMessages();
            widget->update();
        }
    }
}
}
}