#include "UserInfoPopup.hpp"

#include "Application.hpp"
#include "common/UrlFetch.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Resources.hpp"
#include "util/LayoutCreator.hpp"
#include "util/PostToThread.hpp"
#include "widgets/Label.hpp"
#include "widgets/helper/Line.hpp"
#include "widgets/helper/RippleEffectLabel.hpp"

#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QLabel>
#include <QMessageBox>

#define TEXT_FOLLOWERS "Followers: "
#define TEXT_VIEWS "Views: "
#define TEXT_CREATED "Created: "

namespace chatterino {

UserInfoPopup::UserInfoPopup()
    : BaseWindow(nullptr, BaseWindow::Flags(BaseWindow::Frameless | BaseWindow::FramelessDraggable))
    , hack_(new bool)
{
    this->setStayInScreenRect(true);

#ifdef Q_OS_LINUX
    this->setWindowFlag(Qt::Popup);
#endif

    auto app = getApp();

    auto layout = LayoutCreator<UserInfoPopup>(this).setLayoutType<QVBoxLayout>();

    // first line
    auto head = layout.emplace<QHBoxLayout>().withoutMargin();
    {
        // avatar
        auto avatar = head.emplace<RippleEffectButton>(nullptr).assign(&this->ui_.avatarButton);
        avatar->setScaleIndependantSize(100, 100);
        QObject::connect(avatar.getElement(), &RippleEffectButton::clicked, [this] {
            QDesktopServices::openUrl(QUrl("https://twitch.tv/" + this->userName_));
        });

        // items on the right
        auto vbox = head.emplace<QVBoxLayout>();
        {
            auto name = vbox.emplace<Label>().assign(&this->ui_.nameLabel);

            auto font = name->font();
            font.setBold(true);
            name->setFont(font);
            vbox.emplace<Label>(TEXT_VIEWS).assign(&this->ui_.viewCountLabel);
            vbox.emplace<Label>(TEXT_FOLLOWERS).assign(&this->ui_.followerCountLabel);
            vbox.emplace<Label>(TEXT_CREATED).assign(&this->ui_.createdDateLabel);
        }
    }

    layout.emplace<Line>(false);

    // second line
    auto user = layout.emplace<QHBoxLayout>().withoutMargin();
    {
        user->addStretch(1);

        user.emplace<QCheckBox>("Follow").assign(&this->ui_.follow);
        user.emplace<QCheckBox>("Ignore").assign(&this->ui_.ignore);
        user.emplace<QCheckBox>("Ignore highlights").assign(&this->ui_.ignoreHighlights);
        auto viewLogs = user.emplace<RippleEffectLabel>(this).assign(&this->ui_.viewLogs);
        this->ui_.viewLogs->getLabel().setText("View logs");

        auto mod = user.emplace<RippleEffectButton>(this);
        mod->setPixmap(app->resources->buttons.mod);
        mod->setScaleIndependantSize(30, 30);
        auto unmod = user.emplace<RippleEffectButton>(this);
        unmod->setPixmap(app->resources->buttons.unmod);
        unmod->setScaleIndependantSize(30, 30);

        user->addStretch(1);

        QObject::connect(viewLogs.getElement(), &RippleEffectLabel::clicked,
                         [this] { this->getLogs(); });

        QObject::connect(mod.getElement(), &RippleEffectButton::clicked,
                         [this] { this->channel_->sendMessage("/mod " + this->userName_); });
        QObject::connect(unmod.getElement(), &RippleEffectButton::clicked,
                         [this] { this->channel_->sendMessage("/unmod " + this->userName_); });

        // userstate
        this->userStateChanged.connect([this, mod, unmod]() mutable {
            TwitchChannel *twitchChannel = dynamic_cast<TwitchChannel *>(this->channel_.get());

            if (twitchChannel) {
                qDebug() << this->userName_;

                bool isMyself =
                    QString::compare(getApp()->accounts->twitch.getCurrent()->getUserName(),
                                     this->userName_, Qt::CaseInsensitive) == 0;

                mod->setVisible(twitchChannel->isBroadcaster() && !isMyself);
                unmod->setVisible((twitchChannel->isBroadcaster() && !isMyself) ||
                                  (twitchChannel->isMod() && isMyself));
            }
        });
    }

    auto lineMod = layout.emplace<Line>(false);

    // third line
    auto moderation = layout.emplace<QHBoxLayout>().withoutMargin();
    {
        auto timeout = moderation.emplace<TimeoutWidget>();

        this->userStateChanged.connect([this, lineMod, timeout]() mutable {
            TwitchChannel *twitchChannel = dynamic_cast<TwitchChannel *>(this->channel_.get());

            if (twitchChannel) {
                lineMod->setVisible(twitchChannel->hasModRights());
                timeout->setVisible(twitchChannel->hasModRights());
            }
        });

        timeout->buttonClicked.connect([this](auto item) {
            TimeoutWidget::Action action;
            int arg;
            std::tie(action, arg) = item;

            switch (action) {
                case TimeoutWidget::Ban: {
                    if (this->channel_) {
                        this->channel_->sendMessage("/ban " + this->userName_);
                    }
                } break;
                case TimeoutWidget::Unban: {
                    if (this->channel_) {
                        this->channel_->sendMessage("/unban " + this->userName_);
                    }
                } break;
                case TimeoutWidget::Timeout: {
                    if (this->channel_) {
                        this->channel_->sendMessage("/timeout " + this->userName_ + " " +
                                                    QString::number(arg));
                    }
                } break;
            }
        });
    }

    this->setStyleSheet("font-size: 11pt;");

    this->installEvents();
}

void UserInfoPopup::themeRefreshEvent()
{
    BaseWindow::themeRefreshEvent();

    this->setStyleSheet("background: #333");
}

void UserInfoPopup::installEvents()
{
    std::weak_ptr<bool> hack = this->hack_;

    // follow
    QObject::connect(this->ui_.follow, &QCheckBox::stateChanged, [this](int) mutable {
        auto currentUser = getApp()->accounts->twitch.getCurrent();

        QUrl requestUrl("https://api.twitch.tv/kraken/users/" + currentUser->getUserId() +
                        "/follows/channels/" + this->userId_);

        this->ui_.follow->setEnabled(false);
        if (this->ui_.follow->isChecked()) {
            twitchApiPut(requestUrl, [this](QJsonObject) { this->ui_.follow->setEnabled(true); });
        } else {
            twitchApiDelete(requestUrl, [this] { this->ui_.follow->setEnabled(true); });
        }
    });

    std::shared_ptr<bool> ignoreNext = std::make_shared<bool>(false);

    // ignore
    QObject::connect(
        this->ui_.ignore, &QCheckBox::stateChanged, [this, ignoreNext, hack](int) mutable {
            if (*ignoreNext) {
                *ignoreNext = false;
                return;
            }

            this->ui_.ignore->setEnabled(false);

            auto currentUser = getApp()->accounts->twitch.getCurrent();
            if (this->ui_.ignore->isChecked()) {
                currentUser->ignoreByID(this->userId_, this->userName_,
                                        [=](auto result, const auto &message) mutable {
                                            if (hack.lock()) {
                                                if (result == IgnoreResult_Failed) {
                                                    *ignoreNext = true;
                                                    this->ui_.ignore->setChecked(false);
                                                }
                                                this->ui_.ignore->setEnabled(true);
                                            }
                                        });
            } else {
                currentUser->unignoreByID(this->userId_, this->userName_,
                                          [=](auto result, const auto &message) mutable {
                                              if (hack.lock()) {
                                                  if (result == UnignoreResult_Failed) {
                                                      *ignoreNext = true;
                                                      this->ui_.ignore->setChecked(true);
                                                  }
                                                  this->ui_.ignore->setEnabled(true);
                                              }
                                          });
            }
        });
}

void UserInfoPopup::setData(const QString &name, const ChannelPtr &channel)
{
    this->userName_ = name;
    this->channel_ = channel;

    this->ui_.nameLabel->setText(name);

    this->updateUserData();

    this->userStateChanged.invoke();
}

void UserInfoPopup::updateUserData()
{
    std::weak_ptr<bool> hack = this->hack_;

    // get user info
    twitchApiGetUserID(this->userName_, this, [this, hack](QString id) {
        auto currentUser = getApp()->accounts->twitch.getCurrent();

        this->userId_ = id;

        // get channel info
        twitchApiGet(
            "https://api.twitch.tv/kraken/channels/" + id, this, [this](const QJsonObject &obj) {
                this->ui_.followerCountLabel->setText(
                    TEXT_FOLLOWERS + QString::number(obj.value("followers").toInt()));
                this->ui_.viewCountLabel->setText(TEXT_VIEWS +
                                                  QString::number(obj.value("views").toInt()));
                this->ui_.createdDateLabel->setText(
                    TEXT_CREATED + obj.value("created_at").toString().section("T", 0, 0));

                this->loadAvatar(QUrl(obj.value("logo").toString()));
            });

        // get follow state
        currentUser->checkFollow(id, [this, hack](auto result) {
            if (hack.lock()) {
                if (result != FollowResult_Failed) {
                    this->ui_.follow->setEnabled(true);
                    this->ui_.follow->setChecked(result == FollowResult_Following);
                }
            }
        });

        // get ignore state
        bool isIgnoring = false;
        for (const auto &ignoredUser : currentUser->getIgnores()) {
            if (id == ignoredUser.id) {
                isIgnoring = true;
                break;
            }
        }

        this->ui_.ignore->setEnabled(true);
        this->ui_.ignore->setChecked(isIgnoring);
    });

    this->ui_.follow->setEnabled(false);
    this->ui_.ignore->setEnabled(false);
    this->ui_.ignoreHighlights->setEnabled(false);
}

void UserInfoPopup::getLogs()
{
    TwitchChannel *twitchChannel = dynamic_cast<TwitchChannel *>(this->channel_.get());
    QUrl url(QString("https://cbenni.com/api/logs/%1/?nick=%2")
                 .arg(twitchChannel->name, this->userName_));

    QNetworkRequest req(url);
    static auto manager = new QNetworkAccessManager();
    auto *reply = manager->get(req);

    QObject::connect(reply, &QNetworkReply::finished, this, [=] {
        QMessageBox *messageBox = new QMessageBox;
        QString answer = "";
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray rawdata = reply->readAll();
            QJsonObject data = QJsonDocument::fromJson(rawdata).object();
            QJsonValue before = data.value("before");
            for (int i = before.toArray().size() - 1; i >= 0; --i) {
                int index = before.toArray().size() - 1 - i;
                QString message = before[index]["text"].toString();
                QRegExp rx(QString("(.*PRIVMSG #%1 :)").arg(twitchChannel->name));
                message = message.replace(rx, "");
                qint64 rawtime = before[index]["time"].toInt();
                QDateTime timestamp = QDateTime::fromSecsSinceEpoch(rawtime);
                answer += QString("<br><b>[%1 %2]</b> %3: %4")
                              .arg(timestamp.date().toString(), timestamp.time().toString(),
                                   this->userName_, message);
            };
        }
        else
        {
			QString username = this->userName_;
            QString channelName = twitchChannel->name;
            QUrl urlRustle (QString("https://overrustlelogs.net/api/v1/stalk/" + channelName + "/" + username + ".json?limit=10"));
            QNetworkRequest reqRustle(urlRustle);
            static auto managerRustle = new QNetworkAccessManager();
            auto *replyRustle = managerRustle->get(req);

            QObject::connect(replyRustle, &QNetworkReply::finished,this, [=]{
                                if (replyRustle->error() == QNetworkReply::NoError)
                                {
                                    QByteArray rawdata = replyRustle->readAll();
                                    QJsonObject json  = QJsonDocument::fromJson(rawdata).object();
                                    if (json.contains("lines"))
                                    {
                                            QJsonArray messages = json.value("lines").toArray();
                                            for ( auto i: messages)
                                            {

                                                QJsonObject singleMessage = i.toObject();
                                                const QDateTime test = QDateTime::fromTime_t( singleMessage.value("timestamp").toInt());
                                                answer += "<b>[" + test.toString(Qt::TextDate) + "]</b> " + username + ": " + singleMessage.value("text").toString() + "<br>";

                                            }
                                    }
                                }
                                else
                                {
                                	messageBox->setIcon(QMessageBox::Critical);
                                    answer  =  "Cbenni error: " + reply->errorString() +"\nOverrustle error: " + replyRustle->errorString() ;
                                }

           				});
        }
        messageBox->setText(answer);
        messageBox->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
        messageBox->show();
        messageBox->raise();
    });
}

void UserInfoPopup::loadAvatar(const QUrl &url)
{
    QNetworkRequest req(url);
    static auto manager = new QNetworkAccessManager();
    auto *reply = manager->get(req);

    QObject::connect(reply, &QNetworkReply::finished, this, [=] {
        if (reply->error() == QNetworkReply::NoError) {
            const auto data = reply->readAll();

            // might want to cache the avatar image
            QPixmap avatar;
            avatar.loadFromData(data);
            this->ui_.avatarButton->setPixmap(avatar);
        } else {
            this->ui_.avatarButton->setPixmap(QPixmap());
        }
    });
}

//
// TimeoutWidget
//
UserInfoPopup::TimeoutWidget::TimeoutWidget()
    : BaseWidget(nullptr)
{
    auto layout = LayoutCreator<TimeoutWidget>(this).setLayoutType<QHBoxLayout>().withoutMargin();

    QColor color1(255, 255, 255, 80);
    QColor color2(255, 255, 255, 0);

    int buttonWidth = 24;
    int buttonWidth2 = 32;
    int buttonHeight = 32;

    layout->setSpacing(16);

    auto addButton = [&](Action action, const QString &text, const QPixmap &pixmap) {
        auto vbox = layout.emplace<QVBoxLayout>().withoutMargin();
        {
            auto title = vbox.emplace<QHBoxLayout>().withoutMargin();
            title->addStretch(1);
            auto label = title.emplace<Label>(text);
            label->setHasOffset(false);
            label->setStyleSheet("color: #BBB");
            title->addStretch(1);

            auto hbox = vbox.emplace<QHBoxLayout>().withoutMargin();
            hbox->setSpacing(0);
            {
                auto button = hbox.emplace<RippleEffectButton>(nullptr);
                button->setPixmap(pixmap);
                button->setScaleIndependantSize(buttonHeight, buttonHeight);
                button->setBorderColor(QColor(255, 255, 255, 127));

                QObject::connect(button.getElement(), &RippleEffectButton::clicked, [this, action] {
                    this->buttonClicked.invoke(std::make_pair(action, -1));
                });
            }
        }
    };

    auto addTimeouts = [&](const QString &title_,
                           const std::vector<std::pair<QString, int>> &items) {
        auto vbox = layout.emplace<QVBoxLayout>().withoutMargin();
        {
            auto title = vbox.emplace<QHBoxLayout>().withoutMargin();
            title->addStretch(1);
            auto label = title.emplace<Label>(title_);
            label->setStyleSheet("color: #BBB");
            label->setHasOffset(false);
            title->addStretch(1);

            auto hbox = vbox.emplace<QHBoxLayout>().withoutMargin();
            hbox->setSpacing(0);

            for (const auto &item : items) {
                auto a = hbox.emplace<RippleEffectLabel2>();
                a->getLabel().setText(std::get<0>(item));

                if (std::get<0>(item).length() > 1) {
                    a->setScaleIndependantSize(buttonWidth2, buttonHeight);
                } else {
                    a->setScaleIndependantSize(buttonWidth, buttonHeight);
                }
                a->setBorderColor(color1);

                QObject::connect(a.getElement(), &RippleEffectLabel2::clicked, [
                    this, timeout = std::get<1>(item)
                ] { this->buttonClicked.invoke(std::make_pair(Action::Timeout, timeout)); });
            }
        }
    };

    addButton(Unban, "unban", getApp()->resources->buttons.unban);

    addTimeouts("sec", {{"1", 1}});
    addTimeouts("min", {
                           {"1", 1 * 60},
                           {"5", 5 * 60},
                           {"10", 10 * 60},
                       });
    addTimeouts("hour", {
                            {"1", 1 * 60 * 60},
                            {"4", 4 * 60 * 60},
                        });
    addTimeouts("days", {
                            {"1", 1 * 60 * 60 * 24},
                            {"3", 3 * 60 * 60 * 24},
                        });
    addTimeouts("weeks", {
                             {"1", 1 * 60 * 60 * 24 * 7},
                             {"2", 2 * 60 * 60 * 24 * 7},
                         });

    addButton(Ban, "ban", getApp()->resources->buttons.ban);
}

void UserInfoPopup::TimeoutWidget::paintEvent(QPaintEvent *)
{
    //    QPainter painter(this);

    //    painter.setPen(QColor(255, 255, 255, 63));

    //    painter.drawLine(0, this->height() / 2, this->width(), this->height() / 2);
}

}  // namespace chatterino
