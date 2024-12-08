// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERENGINE_H
#define WALLPAPERENGINE_H

#include "ddplugin_videowallpaper_global.h"

#include <QObject>
#ifndef USE_LIBDMR
#include <QMediaPlayer>

class QVideoFrame;
#endif

namespace ddplugin_videowallpaper {

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
#ifndef USE_LIBDMR
    void catchImage(const QVideoFrame &frame);
#endif

private:
    friend class WallpaperEnginePrivate;
    WallpaperEnginePrivate *d;
};

} // namespace ddplugin_videowallpaper

#endif // WALLPAPERENGINE_H
