// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERCONFIG_H
#define WALLPAPERCONFIG_H

#include "ddplugin_videowallpaper_global.h"

#include <QObject>

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class WallpaperConfigPrivate;
class WallpaperConfig : public QObject
{
    Q_OBJECT

public:
    static WallpaperConfig *instance();
    void initialize();
    bool enable() const;
    void setEnable(bool);

signals:
    void changeEnableState(bool enable);

private slots:
    void configChanged(const QString &key);

protected:
    explicit WallpaperConfig(QObject *parent = nullptr);

private:
    friend class WallpaperConfigPrivate;
    WallpaperConfigPrivate *d;
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#define WpCfg WallpaperConfig::instance()

#endif // WALLPAPERCONFIG_H
