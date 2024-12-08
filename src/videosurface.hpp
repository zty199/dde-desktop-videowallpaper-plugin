// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIDEOSURFACE_HPP
#define VIDEOSURFACE_HPP

#include "ddplugin_videowallpaper_global.h"

#include <QAbstractVideoSurface>

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class VideoSurface : public QAbstractVideoSurface
{
    Q_OBJECT

public:
    explicit VideoSurface(QObject *parent = nullptr) { }

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override
    {
        if (type == QAbstractVideoBuffer::NoHandle) {
            return QList<QVideoFrame::PixelFormat> {
                QVideoFrame::Format_ARGB32,
                QVideoFrame::Format_ARGB32,
                QVideoFrame::Format_ARGB32_Premultiplied,
                QVideoFrame::Format_RGB32};
        }

        return {};
    }

    bool present(const QVideoFrame &frame) override
    {
        emit videoFrameChanged(frame);
        return true;
    }

signals:
    void videoFrameChanged(const QVideoFrame &);
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#endif // VIDEOSURFACE_HPP
