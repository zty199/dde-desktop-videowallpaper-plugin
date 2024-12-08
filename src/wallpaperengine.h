// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERENGINE_H
#define WALLPAPERENGINE_H

#include "ddplugin_videowallpaper_global.h"

#include <QObject>
#ifndef USE_LIBMPV
#include <QVideoFrame>
#endif

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class WallpaperEnginePrivate;
class WallpaperEngine : public QObject
{
    Q_OBJECT

public:
    explicit WallpaperEngine(QObject *parent = nullptr);
    ~WallpaperEngine() override;
    bool init();
    void turnOn(bool build = true);
    void turnOff();

public slots:
    void refreshSource();
    void build();
    void onDetachWindows();
    void geometryChanged();
    void play();
    void show();

    void releaseMemory();

private slots:
    bool registerMenu();
    void checkResource();
#ifndef USE_LIBMPV
    void catchImage(const QVideoFrame &frame);
#endif

private:
    friend class WallpaperEnginePrivate;
    WallpaperEnginePrivate *d;
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#endif // WALLPAPERENGINE_H
