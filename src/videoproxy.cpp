// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videoproxy.h"

#ifdef USE_LIBMPV
#include <DPlatformWindowHandle>

#include <third_party/mpvwidget.h>

#include <QEvent>
#include <QLayout>
#else
#include <QPainter>
#endif

using namespace ddplugin_videowallpaper;

#ifdef USE_LIBMPV
VideoProxy::VideoProxy(QWidget *parent)
    : QWidget(parent)
    , widget(new MpvWidget(this, Qt::FramelessWindowHint))
{
    initUI();
}

void VideoProxy::command(const QVariant &params)
{
    widget->command(params);
}

void VideoProxy::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widget);
}
#else
VideoProxy::VideoProxy(QWidget *parent)
    : QWidget(parent)
{
    auto pal = palette();
    pal.setColor(backgroundRole(), Qt::black);
    setPalette(pal);
    setAutoFillBackground(false);
}

VideoProxy::~VideoProxy()
{
}

void VideoProxy::updateImage(const QImage &img)
{
    image = img.scaled(img.size().boundedTo(QSize(1920, 1280)) * devicePixelRatioF(),
                       Qt::KeepAspectRatio, Qt::FastTransformation);
    image.setDevicePixelRatio(devicePixelRatioF());
    update();
}

void VideoProxy::clear()
{
    image.fill(palette().window().color());
    update();
}

void VideoProxy::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    if (image.isNull()) {
        return;
    }

    QSize tar = image.size() / devicePixelRatioF();
    int x = (rect().width() - tar.width()) / 2.0;
    int y = (rect().height() - tar.height()) / 2.0;
    // x = x < 0 ? 0 : x;
    // y = y < 0 ? 0 : y;

    painter.drawImage(x, y, image);

    QWidget::paintEvent(e);
}
#endif
