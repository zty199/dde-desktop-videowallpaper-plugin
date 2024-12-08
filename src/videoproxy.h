// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIDEOPROXY_H
#define VIDEOPROXY_H

#include "ddplugin_videowallpaper_global.h"

#include <QWidget>

#ifdef USE_LIBMPV
class MpvWidget;
#endif

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

#ifdef USE_LIBMPV
class VideoProxy : public QWidget
{
    Q_OBJECT

public:
    VideoProxy(QWidget *parent = nullptr);

    void command(const QVariant &params);

private:
    void initUI();

private:
    MpvWidget *widget = nullptr;
};
#else
class VideoProxy : public QWidget
{
    Q_OBJECT

public:
    explicit VideoProxy(QWidget *parent = nullptr);
    ~VideoProxy();

    void updateImage(const QImage &img);
    void clear();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QImage image;
};
#endif

typedef QSharedPointer<VideoProxy> VideoProxyPointer;

DDP_VIDEOWALLPAPER_END_NAMESPACE

#endif // VIDEOPROXY_H
