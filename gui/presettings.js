/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// mount and sync persistent storage to browser
Module["prepareFS"] = function () {
    if (!FS.analyzePath('tibiarc').exists) {
        FS.mkdir('/tibiarc');
    }

    FS.mount(IDBFS, {
        autoPersist : true
    }, '/tibiarc');

    FS.syncfs(true, function (err) {
        if (err) {
            Module.print(err);
        }
        removeRunDependency("prepareFS");
    });
}

Module.preRun = Module.preRun || [];

Module.preRun.push(function(){
    addRunDependency("prepareFS");
    Module.prepareFS();
})

// sync browser filesystem to persistent storage and unmount
window.addEventListener('beforeunload', function(event) {
    FS.syncfs(false, function(err) {
        if (err) {
            Module.print(err);
        } else {
            console.log("success sync")
        }
        FS.unmount('/tibiarc');
    });
}, false);
