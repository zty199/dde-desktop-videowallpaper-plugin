// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERCONFIG_P_H
#define WALLPAPERCONFIG_P_H

#include "ddplugin_videowallpaper_global.h"
#include "wallpaperconfig.h"

#include <DConfig>

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class WallpaperConfigPrivate
{
public:
    WallpaperConfigPrivate(WallpaperConfig *qq);
    bool getEnable() const;

private:
    bool enable = false;
    DTK_CORE_NAMESPACE::DConfig *settings = nullptr;

    friend class WallpaperConfig;
    WallpaperConfig *q;
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#endif // WALLPAPERCONFIG_P_H
