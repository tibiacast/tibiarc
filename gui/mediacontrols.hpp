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

#ifndef __TRC_GUI_MEDIACONTROLS_HPP__
#define __TRC_GUI_MEDIACONTROLS_HPP__

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollBar>
#include <QToolButton>

namespace trc {
namespace GUI {

class ProgressScrollbar : public QScrollBar {
    Q_OBJECT

public:
    ProgressScrollbar(QWidget *parent = nullptr);

    void setFormatter(std::function<QString(int, int, int)> formatter);

protected:
    std::function<QString(int, int, int)> Formatter;

    virtual void paintEvent(QPaintEvent *) override;
};

class MediaControls : public QGroupBox {
    Q_OBJECT

    QGridLayout Layout;
    QToolButton PlayPauseButton;
    QToolButton StopButton;
    ProgressScrollbar Progress;
    QDoubleSpinBox SpeedBox;

public:
    explicit MediaControls(QWidget *parent = nullptr);
    virtual ~MediaControls();

    std::chrono::milliseconds maximum() const;
    std::chrono::milliseconds progress() const;
    double speed();

public slots:
    void setMaximum(std::chrono::milliseconds value);
    void setProgress(std::chrono::milliseconds value);
    void setSpeed(double speed);

signals:
    void progressChanged(std::chrono::milliseconds value);
    void speedChanged(double speed);

    void stop();
};

} // namespace GUI
} // namespace trc

#endif
