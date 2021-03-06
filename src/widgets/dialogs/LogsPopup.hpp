#pragma once

#include "widgets/BaseWindow.hpp"

namespace chatterino {

class Channel;
class ChannelView;

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;

struct Message;
using MessagePtr = std::shared_ptr<const Message>;

class LogsPopup : public BaseWindow
{
public:
    LogsPopup();

    void setInfo(std::shared_ptr<Channel> channel, QString userName);

private:
    ChannelView *channelView_ = nullptr;
    ChannelPtr channel_;

    QString userName_;
    QString channelName_;

    void initLayout();
    void setMessages(std::vector<MessagePtr> &messages);
    void getOverrustleLogs();
    void getLogviewerLogs(const QString &roomID);
};

}  // namespace chatterino
