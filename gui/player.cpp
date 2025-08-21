/*
 * Copyright 2025 "John Högberg"
 *
 * This file is part of tibiarc.
 *
 * tibiarc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
 */

#include "player.hpp"

#include "renderer.hpp"
#include "characterset.hpp"

#include <QGraphicsPixmapItem>
#include <QScrollBar>

#include <format>

namespace trc {
namespace GUI {

Player::Player(QWidget *parent)
    : QGraphicsView(parent),
      MainLayout(this),
      SidebarSplitter(this),
      ViewSplitter(&SidebarSplitter),
      ViewFrame(&ViewSplitter),
      ViewportLayout(&ViewFrame),
      Controls(&ViewFrame),
      Viewport(&ViewFrame),
      ChatTabs(&ViewSplitter),
      DefaultChannel(&ChatTabs),
      NPCChannel(&ChatTabs),
      Sidebar(&SidebarSplitter),
      FrameTicket(0) {
    {
        MainLayout.setHorizontalSpacing(0);
        MainLayout.setContentsMargins(0, 0, 0, 0);
    }

    SidebarSplitter.setOrientation(Qt::Orientation::Horizontal);
    ViewSplitter.setOrientation(Qt::Orientation::Vertical);

    {
        ViewFrame.setFrameShape(QFrame::Shape::StyledPanel);
        ViewFrame.setFrameShadow(QFrame::Shadow::Raised);

        ViewportLayout.setContentsMargins(0, 0, 0, -1);

        {
            QSizePolicy policy(QSizePolicy::Policy::Expanding,
                               QSizePolicy::Policy::Expanding);

            policy.setHorizontalStretch(0);
            policy.setVerticalStretch(0);
            policy.setHeightForWidth(Viewport.sizePolicy().hasHeightForWidth());

            Viewport.setSizePolicy(policy);
            Viewport.setSizeAdjustPolicy(
                    QAbstractScrollArea::SizeAdjustPolicy::AdjustIgnored);
            Viewport.setMinimumSize(QSize(480, 352));

            Viewport.setVerticalScrollBarPolicy(
                    Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
            Viewport.setHorizontalScrollBarPolicy(
                    Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

            Viewport.setInteractive(false);
            Viewport.setAlignment(Qt::AlignmentFlag::AlignLeading |
                                  Qt::AlignmentFlag::AlignLeft |
                                  Qt::AlignmentFlag::AlignTop);
            Viewport.setStyleSheet(
                    QString::fromUtf8("background-color: transparent;"));

            ViewportLayout.addWidget(&Viewport);
        }

        {
            Controls.setMaximumSize(QSize(16777215, 48));
            ViewportLayout.addWidget(&Controls);
        }

        ViewSplitter.addWidget(&ViewFrame);
    }

    {
        QFont font;

        font.setFamilies({QString::fromUtf8("Ubuntu Mono")});
        font.setBold(true);

        ChatTabs.setFont(font);
        ChatTabs.setMinimumSize(QSize(0, 200));
        ChatTabs.setMovable(true);

        ChatTabs.setStyleSheet(QString::fromUtf8(
                "QTabWidget { font-weight: bold;  }\n"
                "QTabWidget::pane { background: transparent; border: 1; }\n"
                "QTextEdit { background: transparent; font-weight: bold; "
                "font-family: serif }"));

        ViewSplitter.addWidget(&ChatTabs);
    }

    SidebarSplitter.addWidget(&ViewSplitter);

    {
        Sidebar.setMinimumSize(QSize(240, 0));
        Sidebar.setMaximumSize(QSize(240, 16777215));

        Sidebar.setStyleSheet(
                QString::fromUtf8("background-color: transparent;"));
        Sidebar.setVerticalScrollBarPolicy(
                Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
        Sidebar.setHorizontalScrollBarPolicy(
                Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        Sidebar.setAlignment(Qt::AlignmentFlag::AlignLeading |
                             Qt::AlignmentFlag::AlignLeft |
                             Qt::AlignmentFlag::AlignTop);

        SidebarSplitter.addWidget(&Sidebar);
    }

    MainLayout.addWidget(&SidebarSplitter, 0, 0, 1, 1);

    ChatTabs.setCurrentIndex(-1);

    /* */

    setScene(&BackgroundScene);
    Viewport.setScene(&ViewportScene);
    Sidebar.setScene(&SidebarScene);

    connect(&Controls, &MediaControls::stop, [this]() {
        FrameTicket++;

        Gamestate.reset();
        Recording.reset();

        emit stop();
    });

    connect(&Controls, &MediaControls::speedChanged, [this](double value) {
        BaseTick = std::chrono::milliseconds(Gamestate->CurrentTick);
        LastFrameAt = ScaleTime = std::chrono::steady_clock::now();
        Scale = value;
    });

    connect(&Controls,
            &MediaControls::progressChanged,
            [this](std::chrono::milliseconds progress) {
                ProcessEvents(progress);
                Gamestate->Messages.Prune(progress.count());
            });
}

Player::~Player() {
}

void Player::ResetInterface() {
    ChatTabs.clear();

    ChatChannels.clear();
    PrivateChannels.clear();
    DefaultChannel.clear();

    AddChatTab(DefaultChannel, "Default");

    if (Gamestate->Version.AtLeast(8, 20)) {
        AddChatTab(NPCChannel, "NPC");
    }
}

void Player::RenderFrame(size_t ticket) {
    if (FrameTicket != ticket) {
        return;
    }

    if (Scale > 0.0) {
        auto scaledTick =
                BaseTick +
                std::chrono::milliseconds(static_cast<int64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                                LastFrameAt - ScaleTime)
                                .count() *
                        Scale));
        auto progress = std::min(scaledTick, Recording->Runtime);
        Controls.setProgress(progress);
    }

    RenderViewport();
    RenderSidebar();

    auto timestamp = std::chrono::steady_clock::now();
    auto elapsed = timestamp - LastFrameAt;

    auto targetDelay = std::max<int>(
            0,
            16 - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                            .count());

    LastFrameAt = timestamp;
    QTimer::singleShot(targetDelay, [this, ticket] { RenderFrame(ticket); });
}

void Player::RenderViewport() {
    Renderer::Options options{.Width = Viewport.width(),
                              .Height = Viewport.height()};
    float scale = std::min(options.Width / (float)Renderer::NativeResolutionX,
                           options.Height / (float)Renderer::NativeResolutionY);
    int scaledWidth = Renderer::NativeResolutionX * scale;
    int scaledHeight = Renderer::NativeResolutionY * scale;

    ViewportScene.clear();

    {
        Canvas canvas(Renderer::NativeResolutionX,
                      Renderer::NativeResolutionY,
                      Canvas::Type::External);
        QImage image(Renderer::NativeResolutionX,
                     Renderer::NativeResolutionY,
                     QImage::Format::Format_RGBA8888);
        canvas.Buffer = image.scanLine(0);
        canvas.Stride = image.bytesPerLine();

        /* Wipe the background in the same manner Tibia does it, leaving empty
         * spots on the map black. */
        canvas.DrawRectangle(Pixel(0, 0, 0),
                             0,
                             0,
                             Renderer::NativeResolutionX,
                             Renderer::NativeResolutionY);
        Renderer::DrawGamestate(options, *Gamestate, canvas);

        auto item = ViewportScene.addPixmap(QPixmap::fromImage(image));
        item->setScale(scale);
    }

    {
        Canvas canvas(Renderer::NativeResolutionX * scale,
                      Renderer::NativeResolutionY * scale,
                      Canvas::Type::External);
        QImage image(Renderer::NativeResolutionX * scale,
                     Renderer::NativeResolutionY * scale,
                     QImage::Format::Format_RGBA8888);
        canvas.Buffer = image.scanLine(0);
        canvas.Stride = image.bytesPerLine();

        canvas.Wipe();
        Renderer::DrawOverlay(options, *Gamestate, canvas);

        ViewportScene.addPixmap(QPixmap::fromImage(image));
    }

    /* The automatic centering provided by Qt doesn't work properly, the view
     * may be slightly off-kilter after resizing.
     *
     * Handle it manually by using top-left alignment and shifting the scene
     * rect. */
    auto horizontalShift = (options.Width - scaledWidth) / 2;
    auto verticalShift = (options.Height - scaledHeight) / 2;
    Viewport.setSceneRect(QRect(-horizontalShift,
                                -verticalShift,
                                scaledWidth + horizontalShift,
                                scaledHeight + verticalShift));
    Viewport.update();
}

void Player::RenderSidebar() {
    const auto maxWidth = Sidebar.width();

    auto &state = *Gamestate;
    int baseY = 0;

    SidebarScene.clear();

    {
        const int height = Renderer::MeasureStatusBarsHeight(state) +
                           Renderer::MeasureInventoryAreaHeight(state) +
                           (state.Version.Features.IconBar
                                    ? Renderer::MeasureIconBarHeight(state)
                                    : 0);

        Canvas canvas(maxWidth, height, Canvas::Type::External);
        QImage image(maxWidth, height, QImage::Format::Format_RGBA8888);
        int offsetX = 0, offsetY = 0;

        canvas.Buffer = image.scanLine(0);
        canvas.Stride = image.bytesPerLine();
        canvas.Wipe();

        Renderer::DrawStatusBars(state, canvas, offsetX, offsetY);

        Renderer::DrawInventoryArea(state, canvas, offsetX, offsetY);

        if (state.Version.Features.IconBar) {
            Renderer::DrawIconBar(state, canvas, offsetX, offsetY);
        }

        auto item = SidebarScene.addPixmap(QPixmap::fromImage(image));
        item->setPos(QPointF((maxWidth / 2) - 76, baseY));

        Assert(height == offsetY);
        baseY += offsetY;
    }

    for (auto &[_, container] : state.Containers) {
        int height = Renderer::MeasureContainerHeight(state,
                                                      container,
                                                      false,
                                                      maxWidth);
        int offsetX = 0, offsetY = 0;

        Canvas canvas(maxWidth, height, Canvas::Type::External);
        QImage image(maxWidth, height, QImage::Format::Format_RGBA8888);

        canvas.Buffer = image.scanLine(0);
        canvas.Stride = image.bytesPerLine();
        canvas.Wipe();

        Renderer::DrawContainer(state,
                                canvas,
                                container,
                                false,
                                maxWidth,
                                height,
                                offsetX,
                                offsetY);

        auto item = SidebarScene.addPixmap(QPixmap::fromImage(image));
        item->setPos(QPointF(0, baseY));

        baseY += offsetY;
    }

    {
        int height = Renderer::MeasureSkillsHeight(state);
        int offsetX = 0, offsetY = 0;

        Canvas canvas(maxWidth, height, Canvas::Type::External);
        QImage image(maxWidth, height, QImage::Format::Format_RGBA8888);

        canvas.Buffer = image.scanLine(0);
        canvas.Stride = image.bytesPerLine();
        canvas.Wipe();

        Renderer::DrawSkills(state, canvas, maxWidth - 24, offsetX, offsetY);

        auto item = SidebarScene.addPixmap(QPixmap::fromImage(image));
        item->setPos(QPointF(0, baseY));

        baseY += offsetY;
    }

    Sidebar.update();
}

void Player::UpdateBackground() {
    const auto &background = Gamestate->Version.Icons.ClientBackground;

    Canvas canvas(background.Width, background.Height, Canvas::Type::External);
    QImage image(background.Width,
                 background.Height,
                 QImage::Format::Format_RGBA8888);
    int offsetX = 0, offsetY = 0;

    canvas.Buffer = image.scanLine(0);
    canvas.Stride = image.bytesPerLine();
    canvas.Wipe();

    Renderer::DrawClientBackground(*Gamestate,
                                   canvas,
                                   0,
                                   0,
                                   canvas.Width,
                                   canvas.Height);

    BackgroundScene.setBackgroundBrush(QBrush(image));
}

void Player::AddChatTab(QTextEdit &editor, const std::string &name) {
    auto &scrollbar = *editor.verticalScrollBar();

    editor.setUndoRedoEnabled(false);
    editor.setReadOnly(true);

    ChatTabs.addTab(&editor, name.c_str());
}

void Player::ProcessEvent([[maybe_unused]] std::chrono::milliseconds timestamp,
                          const Events::PrivateConversationOpened &event) {
    auto [it, added] = PrivateChannels.try_emplace(event.Name, &ChatTabs);

    if (added) {
        AddChatTab(it->second, event.Name.c_str());
    }
}

void Player::ProcessEvent([[maybe_unused]] std::chrono::milliseconds timestamp,
                          const Events::ChannelOpened &event) {
    auto [it, added] = ChatChannels.try_emplace(event.Id, &ChatTabs);

    if (added) {
        AddChatTab(it->second, event.Name.c_str());
    }
}

void Player::ProcessEvent([[maybe_unused]] std::chrono::milliseconds timestamp,
                          const Events::ChannelClosed &event) {
    auto it = ChatChannels.find(event.Id);

    if (it != ChatChannels.end()) {
        auto index = ChatTabs.indexOf(&it->second);
        ChatTabs.removeTab(index);
        ChatChannels.erase(it);
    }
}

template <typename Lambda>
static void AddChannelText(QTextEdit &editor, MessageMode mode, Lambda lambda) {
    auto &scrollbar = *editor.verticalScrollBar();
    bool followText = scrollbar.value() == scrollbar.maximum();

    QTextCursor cursor(editor.document());

    cursor.movePosition(QTextCursor::End);

    cursor.beginEditBlock();

    QTextCharFormat format = cursor.blockCharFormat();
    auto color = Message::TextColor(mode);

    format.setForeground(
            QColor(color.Red, color.Green, color.Blue, color.Alpha));
    cursor.setBlockCharFormat(format);

    lambda(cursor);

    cursor.insertBlock();
    cursor.endEditBlock();

    if (followText) {
        scrollbar.setValue(scrollbar.maximum());
    }
}

void Player::AddChatMessage(QTextEdit &editor,
                            std::chrono::milliseconds timestamp,
                            const Events::CreatureSpoke &event) {
    auto seconds = std::chrono::floor<std::chrono::seconds>(timestamp);
    AddChannelText(
            editor,
            event.Mode,
            [this, seconds, &event](QTextCursor &cursor) {
                if (event.AuthorLevel > 0) {
                    cursor.insertText(std::format("+{:%H:%M:%S} {} [{}]: {}",
                                                  seconds,
                                                  CharacterSet::ToPrintableUtf8(
                                                          event.AuthorName),
                                                  event.AuthorLevel,
                                                  CharacterSet::ToPrintableUtf8(
                                                          event.Message))
                                              .c_str());
                } else {
                    cursor.insertText(std::format("+{:%H:%M:%S} {}: {}",
                                                  seconds,
                                                  CharacterSet::ToPrintableUtf8(
                                                          event.AuthorName),
                                                  CharacterSet::ToPrintableUtf8(
                                                          event.Message))
                                              .c_str());
                }
            });
}

void Player::ProcessEvent(std::chrono::milliseconds timestamp,
                          const Events::CreatureSpokeInChannel &event) {
    auto it = ChatChannels.find(event.ChannelId);

    if (it != ChatChannels.end()) {
        AddChatMessage(it->second, timestamp, event);
    }
}

void Player::ProcessEvent(std::chrono::milliseconds timestamp,
                          const Events::CreatureSpokeOnMap &event) {
    switch (event.Mode) {
    case MessageMode::Say:
    case MessageMode::Spell:
    case MessageMode::Whisper:
    case MessageMode::Yell:
        AddChatMessage(DefaultChannel, timestamp, event);
        break;
    case MessageMode::NPCStart:
    case MessageMode::NPCContinued:
    case MessageMode::PlayerToNPC: {
        AddChatMessage(NPCChannel, timestamp, event);
        break;
    }
    default:
        break;
    }
}

void Player::ProcessEvent(std::chrono::milliseconds timestamp,
                          const Events::CreatureSpoke &event) {
    if (event.Mode != MessageMode::PrivateIn) {
        return;
    }

    auto [it, added] = PrivateChannels.try_emplace(event.AuthorName, &ChatTabs);

    if (added) {
        AddChatTab(it->second, event.AuthorName);
    }

    AddChatMessage(it->second, timestamp, event);
}

void Player::ProcessEvent(
        [[maybe_unused]] std::chrono::milliseconds timestamp,
        [[maybe_unused]] const Events::StatusMessageReceivedInChannel &event) {
}

void Player::ProcessEvent(std::chrono::milliseconds timestamp,
                          const Events::StatusMessageReceived &event) {
    auto seconds = std::chrono::floor<std::chrono::seconds>(timestamp);
    AddChannelText(DefaultChannel,
                   event.Mode,
                   [this, seconds, &event](QTextCursor &cursor) {
                       cursor.insertText(
                               std::format("+{:%H:%M:%S} {}",
                                           seconds,
                                           CharacterSet::ToPrintableUtf8(
                                                   event.Message))
                                       .c_str());
                   });
}

void Player::ProcessEvent(std::chrono::milliseconds timestamp,
                          const Events::Base &base) {
    Gamestate->CurrentTick = timestamp.count();
    base.Update(*Gamestate);

    switch (base.Kind()) {
    case Events::Type::PrivateConversationOpened:
        ProcessEvent(
                timestamp,
                static_cast<const Events::PrivateConversationOpened &>(base));
        break;
    case Events::Type::ChannelOpened:
        ProcessEvent(timestamp,
                     static_cast<const Events::ChannelOpened &>(base));
        break;
    case Events::Type::ChannelClosed:
        ProcessEvent(timestamp,
                     static_cast<const Events::ChannelClosed &>(base));
        break;
    case Events::Type::CreatureSpokeInChannel:
        ProcessEvent(timestamp,
                     static_cast<const Events::CreatureSpokeInChannel &>(base));
        break;
    case Events::Type::CreatureSpokeOnMap:
        ProcessEvent(timestamp,
                     static_cast<const Events::CreatureSpokeOnMap &>(base));
        break;
    case Events::Type::CreatureSpoke:
        ProcessEvent(timestamp,
                     static_cast<const Events::CreatureSpoke &>(base));
        break;
    case Events::Type::StatusMessageReceivedInChannel:
        ProcessEvent(
                timestamp,
                static_cast<const Events::StatusMessageReceivedInChannel &>(
                        base));
        break;
    case Events::Type::StatusMessageReceived:
        ProcessEvent(timestamp,
                     static_cast<const Events::StatusMessageReceived &>(base));
        break;
    default:
        break;
    }
}

void Player::ProcessEvents(std::chrono::milliseconds until) {
    if (until < std::chrono::milliseconds(Gamestate->CurrentTick)) {
        BaseTick = until;

        /* The user has selected a time in the past. Since we lack keyframes,
         * reset everything and start from the beginning. */
        ResetInterface();

        Needle = Recording->Frames.cbegin();

        Gamestate->Reset();
        Gamestate->CurrentTick = until.count();

        /* Fast-forward until the game state is sufficiently initialized. */
        while (!Gamestate->Creatures.contains(Gamestate->Player.Id) &&
               Needle != Recording->Frames.cend()) {
            for (auto &event : Needle->Events) {
                ProcessEvent(Needle->Timestamp, *event);
            }

            Needle = std::next(Needle);
        }
    }

    while (Needle != Recording->Frames.cend() && Needle->Timestamp <= until) {
        for (auto &event : Needle->Events) {
            ProcessEvent(Needle->Timestamp, *event);
        }

        Needle = std::next(Needle);
    }

    Gamestate->CurrentTick = until.count();
}

void Player::Open(const Version &version,
                  std::unique_ptr<Recordings::Recording> &&recording) {
    Recording = std::move(recording);
    Gamestate = std::make_unique<trc::Gamestate>(version);

    UpdateBackground();

    /* Reset the state by triggering the rewind logic through forcing a
     * gamestate tick far ahead in the future. */
    Gamestate->CurrentTick = std::numeric_limits<int>::max();
    BaseTick = std::chrono::milliseconds::zero();
    ProcessEvents(BaseTick);

    Controls.setMaximum(Recording->Runtime);
    Controls.setSpeed(1.0);
    Controls.setProgress(std::chrono::milliseconds::zero());

    /* Start the render loop.*/
    LastFrameAt = ScaleTime = std::chrono::steady_clock::now();
    Scale = 1.0;

    RenderFrame(++FrameTicket);
}

} // namespace GUI
} // namespace trc
