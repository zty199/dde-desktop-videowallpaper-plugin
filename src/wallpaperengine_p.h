// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERENGINE_P_H
#define WALLPAPERENGINE_P_H

#include "wallpaperengine.h"
#include "videoproxy.h"

#include <QFileSystemWatcher>
#include <QUrl>

#ifndef USE_LIBDMR
class QMediaPlayer;
#endif

namespace ddplugin_videowallpaper {

class WallpaperEnginePrivate
{
public:
    explicit WallpaperEnginePrivate(WallpaperEngine *qq);
    inline QRect relativeGeometry(const QRect &geometry)
    {
        return QRect(QPoint(0, 0), geometry.size());
    }
    static QList<QUrl> getVideos(const QString &path);

private:
    VideoProxyPointer createWidget(QWidget *root);
    void setBackgroundVisible(bool v);
    QString sourcePath() const;
    QMap<QString, VideoProxyPointer> widgets;
    void clearWidgets();

private:
    QFileSystemWatcher *watcher = nullptr;

    QList<QUrl> videos;
#ifndef USE_LIBDMR
    QMediaPlayer *player = nullptr;
    QVideoSink *surface = nullptr;
#endif

    friend class WallpaperEngine;
    WallpaperEngine *q;
};

} // namespace ddplugin_videowallpaper

#endif // WALLPAPERENGINE_P_H
