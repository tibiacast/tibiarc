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

#ifndef __TRC_GUI_PLAYER_HPP__
#define __TRC_GUI_PLAYER_HPP__

#include "versions.hpp"
#include "recordings.hpp"
#include "gamestate.hpp"
#include "events.hpp"

#include "mediacontrols.hpp"

#include <string>
#include <chrono>
#include <map>

#include <QFrame>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace trc {
namespace GUI {

class Player : public QGraphicsView {
    Q_OBJECT

    /* User interface, note that the field order is significant as they must
     * be constructed in parent-child order. */
    QGridLayout MainLayout;
    QSplitter SidebarSplitter;
    QSplitter ViewSplitter;
    QFrame ViewFrame;
    QVBoxLayout ViewportLayout;
    MediaControls Controls;
    QGraphicsView Viewport;
    QTabWidget ChatTabs;
    QTextEdit DefaultChannel;
    QTextEdit NPCChannel;
    QGraphicsView Sidebar;

    std::map<uint32_t, QTextEdit> ChatChannels;
    std::map<std::string, QTextEdit> PrivateChannels;

    void ResetInterface();

    /* Rendering */
    QGraphicsScene BackgroundScene;
    QGraphicsScene SidebarScene;
    QGraphicsScene ViewportScene;

    std::chrono::steady_clock::time_point LastFrameAt;

    /* Ticket value for render/update loops; if a timer fires and its ticket
     * value differs from the one stored here, it nops instead of rendering
     * another frame.
     *
     * This lets us "cancel" the one-shot timer required for decent frame
     * timing without a fuss. */
    size_t FrameTicket;

    void RenderFrame(size_t cookie);
    void RenderViewport();
    void RenderSidebar();

    void UpdateBackground();

    /* Event handling */
    void AddChatTab(QTextEdit &editor, const std::string &name);
    void AddChatMessage(QTextEdit &editor,
                        std::chrono::milliseconds timestamp,
                        const Events::CreatureSpoke &event);

    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::PrivateConversationOpened &event);
    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::ChannelOpened &event);
    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::ChannelClosed &event);
    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::CreatureSpokeInChannel &event);
    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::CreatureSpokeOnMap &event);
    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::CreatureSpoke &event);
    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::StatusMessageReceivedInChannel &event);
    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::StatusMessageReceived &event);
    void ProcessEvent(std::chrono::milliseconds timestamp,
                      const Events::Base &base);

    /* Playback */
    std::unique_ptr<trc::Gamestate> Gamestate;
    std::unique_ptr<Recordings::Recording> Recording;
    std::list<Recordings::Recording::Frame>::const_iterator Needle;

    std::chrono::milliseconds BaseTick;
    std::chrono::steady_clock::time_point LastUpdate;
    std::chrono::time_point<std::chrono::steady_clock> ScaleTime;
    double Scale;

    void ProcessEvents(std::chrono::milliseconds until);

public:
    explicit Player(QWidget *parent = nullptr);
    virtual ~Player();

    void Open(const Version &version,
              std::unique_ptr<Recordings::Recording> &&recording);

signals:
    void stop();
};

} // namespace GUI
} // namespace trc

#endif
