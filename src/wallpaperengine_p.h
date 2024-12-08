// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERENGINE_P_H
#define WALLPAPERENGINE_P_H

#include "ddplugin_videowallpaper_global.h"
#include "videoproxy.h"

#include <QFileSystemWatcher>
#include <QRect>
#include <QUrl>
#ifndef USE_LIBMPV
#include <QMediaPlayer>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include "videosurface.hpp"
#include <QMediaPlaylist>
#else
#include <QVideoSink>
#endif
#endif

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class WallpaperEngine;
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
#ifndef USE_LIBMPV
    QMediaPlayer *player = nullptr;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    VideoSurface *surface = nullptr;
    QMediaPlaylist *playlist = nullptr;
#else
    QVideoSink *surface = nullptr;
#endif
#endif
    QList<QUrl> videos;

    friend class WallpaperEngine;
    WallpaperEngine *q;
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#endif // WALLPAPERENGINE_P_H
