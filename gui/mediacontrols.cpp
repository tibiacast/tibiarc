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

#include "mediacontrols.hpp"

#include <QPainter>
#include <QStyleOptionSlider>

namespace trc {
namespace GUI {

ProgressScrollbar::ProgressScrollbar(QWidget *parent) : QScrollBar(parent) {
}

void ProgressScrollbar::setFormatter(
        std::function<QString(int, int, int)> formatter) {
    Formatter = formatter;
}

void ProgressScrollbar::paintEvent(QPaintEvent *event) {
    QScrollBar::paintEvent(event);

    QStyleOptionSlider option;
    QPainter painter(this);

    initStyleOption(&option);

    auto sliderBounds = style()->subControlRect(QStyle::CC_ScrollBar,
                                                &option,
                                                QStyle::SC_ScrollBarGroove,
                                                this);
    painter.drawText(sliderBounds,
                     Qt::AlignCenter,
                     Formatter(minimum(), maximum(), value()));
}

MediaControls::MediaControls(QWidget *parent)
    : QGroupBox(parent),
      Layout(this),
      PlayPauseButton(this),
      StopButton(this),
      Progress(this),
      SpeedBox(this) {
    PlayPauseButton.setMinimumSize(QSize(32, 32));
    PlayPauseButton.setMaximumSize(QSize(32, 32));
    PlayPauseButton.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackPause));

    Layout.addWidget(&PlayPauseButton, 0, 0, 1, 1);

    StopButton.setMinimumSize(QSize(32, 32));
    StopButton.setMaximumSize(QSize(32, 32));
    StopButton.setIcon(QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStop));

    Layout.addWidget(&StopButton, 0, 1, 1, 1);

    Progress.setMinimumSize(QSize(0, 32));
    Progress.setMaximumSize(QSize(16777215, 32));
    Progress.setSingleStep(1000);
    Progress.setPageStep(10000);
    Progress.setOrientation(Qt::Orientation::Horizontal);
    Progress.setInvertedAppearance(false);
    Progress.setInvertedControls(true);

    Progress.setFormatter([]([[maybe_unused]] int min,
                             int max,
                             int value) -> QString {
        auto elapsed = std::chrono::milliseconds(value),
             runtime = std::chrono::milliseconds(max);
        return std::format("{:%H:%M:%S} / {:%H:%M:%S}",
                           std::chrono::floor<std::chrono::seconds>(elapsed),
                           std::chrono::floor<std::chrono::seconds>(runtime))
                .c_str();
    });

    Layout.addWidget(&Progress, 0, 2, 1, 1);

    SpeedBox.setMinimumSize(QSize(64, 32));
    SpeedBox.setMaximumSize(QSize(64, 32));
    SpeedBox.setButtonSymbols(QAbstractSpinBox::ButtonSymbols::PlusMinus);
    SpeedBox.setMaximum(64.000000000000000);
    SpeedBox.setSingleStep(0.500000000000000);
    SpeedBox.setStepType(QAbstractSpinBox::StepType::DefaultStepType);
    SpeedBox.setValue(1.000000000000000);

    {
        QSizePolicy sizePolicy(QSizePolicy::Policy::Fixed,
                               QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(SpeedBox.sizePolicy().hasHeightForWidth());
        SpeedBox.setSizePolicy(sizePolicy);
    }

    Layout.addWidget(&SpeedBox, 0, 3, 1, 1);

    connect(&PlayPauseButton, &QToolButton::clicked, [this]() {
        if (SpeedBox.value() > 0.0) {
            SpeedBox.setValue(0.0);
        } else {
            SpeedBox.setValue(1.0);
        }
    });

    connect(&StopButton, &QToolButton::clicked, [this]() { emit stop(); });

    connect(&Progress, &QScrollBar::sliderPressed, [this]() {
        /* Pause the video while dragging the slider.
         *
         * Note that this does not suppress the `progressChanged` slider, it
         * merely provides a way for the user to prevent the video from
         * playing forward on its own while the user is dragging. */
        emit speedChanged(0.0);
    });

    connect(&Progress, &QScrollBar::sliderReleased, [this]() {
        emit speedChanged(SpeedBox.value());
    });

    connect(&Progress, &QScrollBar::valueChanged, [this]() {
        emit progressChanged(std::chrono::milliseconds(Progress.value()));
    });

    connect(&SpeedBox, &QDoubleSpinBox::valueChanged, [this](double value) {
        auto theme = (value > 0.0) ? QIcon::ThemeIcon::MediaPlaybackPause
                                   : QIcon::ThemeIcon::MediaPlaybackStart;
        PlayPauseButton.setIcon(QIcon::fromTheme(theme));
        emit speedChanged(value);
    });
}

MediaControls::~MediaControls() {
}

std::chrono::milliseconds MediaControls::maximum() const {
    return std::chrono::milliseconds(Progress.maximum());
}

double MediaControls::speed() {
    return SpeedBox.value();
}

std::chrono::milliseconds MediaControls::progress() const {
    return std::chrono::milliseconds(Progress.value());
}

void MediaControls::setMaximum(std::chrono::milliseconds value) {
    Progress.setMaximum(value.count());
}

void MediaControls::setSpeed(double value) {
    SpeedBox.setValue(value);
}

void MediaControls::setProgress(std::chrono::milliseconds value) {
    Progress.setValue(value.count());
}

} // namespace GUI
} // namespace trc
