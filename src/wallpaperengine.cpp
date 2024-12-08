// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wallpaperengine_p.h"
#include "util/ddpugin_eventinterface_helper.h"
#include "wallpaperconfig.h"
#include "videowallpapermenuscene.h"

#include "util/menu_eventinterface_helper.h"

#include "dfm-base/dfm_desktop_defines.h"

#include <QDir>
#include <QStandardPaths>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QDebug>
#ifndef USE_LIBDMR
#include <QVideoSink>
#include <QVideoFrame>
#ifdef ENABLE_AUDIO_OUTPUT
#include <QAudioOutput>
#include <QAudioDevice>
#include <QMediaDevices>
#endif
#endif
#include <QTimer>

#include <malloc.h>

using namespace ddplugin_videowallpaper;
DFMBASE_USE_NAMESPACE

#define CanvasCoreSubscribe(topic, func) \
    dpfSignalDispatcher->subscribe("ddplugin_core", QT_STRINGIFY2(topic), this, func);

#define CanvasCoreUnsubscribe(topic, func) \
    dpfSignalDispatcher->unsubscribe("ddplugin_core", QT_STRINGIFY2(topic), this, func);

static QString getScreenName(QWidget *win)
{
    return win->property(DesktopFrameProperty::kPropScreenName).toString();
}

static QMap<QString, QWidget *> rootMap()
{
    QList<QWidget *> root = ddplugin_desktop_util::desktopFrameRootWindows();
    QMap<QString, QWidget *> ret;
    for (QWidget *win : root) {
        QString name = getScreenName(win);
        if (name.isEmpty())
            continue;
        ret.insert(name, win);
    }

    return ret;
}

WallpaperEnginePrivate::WallpaperEnginePrivate(WallpaperEngine *qq)
    : q(qq)
{
}

QList<QUrl> WallpaperEnginePrivate::getVideos(const QString &path)
{
    QList<QUrl> ret;
    QDir dir(path);
    for (const QFileInfo &file : dir.entryInfoList(QDir::Files))
        ret << QUrl::fromLocalFile(file.absoluteFilePath());

    return ret;
}

VideoProxyPointer WallpaperEnginePrivate::createWidget(QWidget *root)
{
    const QString screenName = getScreenName(root);
    VideoProxyPointer bwp(new VideoProxy());

    bwp->setParent(root);
    QRect geometry = relativeGeometry(root->geometry()); // scaled area
    bwp->setGeometry(geometry);

    bwp->setProperty(DesktopFrameProperty::kPropScreenName, getScreenName(root));
    bwp->setProperty(DesktopFrameProperty::kPropWidgetName, "videowallpaper");
    bwp->setProperty(DesktopFrameProperty::kPropWidgetLevel, 5.1);

    fmDebug() << "screen name" << screenName << "geometry" << root->geometry() << bwp.get();
    return bwp;
}

void WallpaperEnginePrivate::setBackgroundVisible(bool v)
{
    QList<QWidget *> roots = ddplugin_desktop_util::desktopFrameRootWindows();
    for (QWidget *root : roots) {
        for (QObject *obj : root->children()) {
            if (QWidget *wid = dynamic_cast<QWidget *>(obj)) {
                QString type = wid->property(DesktopFrameProperty::kPropWidgetName).toString();
                if (type == "background") {
                    wid->setVisible(v);
                }
            }
        }
    }
}

QString WallpaperEnginePrivate::sourcePath() const
{
    QString path = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first() + "/video-wallpaper";
    return path;
}

void WallpaperEnginePrivate::clearWidgets()
{
    foreach (auto videoProxy, widgets.values()) {
        videoProxy.clear();
    }
    widgets.clear();
}

WallpaperEngine::WallpaperEngine(QObject *parent)
    : QObject(parent)
    , d(new WallpaperEnginePrivate(this))
{
}

WallpaperEngine::~WallpaperEngine()
{
    turnOff();
}

bool WallpaperEngine::init()
{
    WpCfg->initialize();

    QFileInfo source(d->sourcePath());
    if (!source.exists()) {
        source.absoluteDir().mkpath(source.fileName());
    }
    fmInfo() << "the wallpaper resource is in" << source.absoluteFilePath();

    if (!registerMenu()) {
        // waiting canvas menu
        dpfSignalDispatcher->subscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded", this, &WallpaperEngine::registerMenu);
    }

    /**
     * FIXME: add delay before video play if video-wallpaper is enabled,
     * to avoid video stuttering when dde-shell startup
     */
    QTimer::singleShot(5000, [this] {
        connect(WpCfg, &WallpaperConfig::changeEnableState, this, [this](bool e) {
            if (WpCfg->enable() == e)
                return;
            WpCfg->setEnable(e);
            if (e) {
                turnOn();
            } else
                turnOff();
        });

        if (WpCfg->enable()) {
            turnOn();
        }
    });

    return true;
}

void WallpaperEngine::turnOn(bool b)
{
    Q_ASSERT(d->watcher == nullptr);

    CanvasCoreSubscribe(signal_DesktopFrame_WindowShowed, &WallpaperEngine::play);
    CanvasCoreSubscribe(signal_DesktopFrame_WindowBuilded, &WallpaperEngine::build);
    CanvasCoreSubscribe(signal_DesktopFrame_GeometryChanged, &WallpaperEngine::geometryChanged);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowAboutToBeBuilded, &WallpaperEngine::onDetachWindows);

    d->watcher = new QFileSystemWatcher(this);
    {
        d->watcher->addPath(d->sourcePath());
        /**
         * FIXME: when a large video file is copied to directory,
         * seems that signal is emitted before copy is finished,
         * in which case player might not be able to play.
         */
        connect(d->watcher, &QFileSystemWatcher::directoryChanged, this, &WallpaperEngine::refreshSource);
    }

#ifndef USE_LIBDMR
    d->player = new QMediaPlayer(nullptr);
    connect(
        d->player, &QMediaPlayer::sourceChanged, this, [&] {
            // try to release memory
            releaseMemory();
        },
        Qt::QueuedConnection);

    d->surface = new QVideoSink;
    connect(d->surface, &QVideoSink::videoFrameChanged, this, &WallpaperEngine::catchImage);

    d->player->setVideoSink(d->surface);
    d->player->setLoops(QMediaPlayer::Infinite);

#ifdef ENABLE_AUDIO_OUTPUT
    QAudioOutput *output = new QAudioOutput(QMediaDevices::defaultAudioOutput(), d->player);
    d->player->setAudioOutput(output);
#endif
#endif

    refreshSource();
    if (b) {
        build();
        show();
    }
}

void WallpaperEngine::turnOff()
{
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowShowed, &WallpaperEngine::play);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowBuilded, &WallpaperEngine::build);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowBuilded, &WallpaperEngine::onDetachWindows);
    CanvasCoreUnsubscribe(signal_DesktopFrame_GeometryChanged, &WallpaperEngine::geometryChanged);

    d->watcher->deleteLater();
    d->watcher = nullptr;

#ifndef USE_LIBDMR
    d->player->setSource(QUrl());
    d->player->deleteLater();
    d->player = nullptr;

    d->surface->deleteLater();
    d->surface = nullptr;
#endif

    d->clearWidgets();
    d->videos.clear();

    // show background.
    d->setBackgroundVisible(true);
}

void WallpaperEngine::refreshSource()
{
    bool wasVideosEmpty = d->videos.isEmpty();
    d->videos = d->getVideos(d->sourcePath());
    checkResource();

    if (d->videos.isEmpty()) {
        d->player->setSource(QUrl());
        for (const VideoProxyPointer &bwp : d->widgets.values())
            bwp->clear();
        return;
    }

#ifndef USE_LIBDMR
    bool run = d->player->playbackState() == QMediaPlayer::PlayingState;
    if (run || wasVideosEmpty) {
        d->player->setSource(d->videos.constFirst());
        d->player->play();
    }
#else
    for (const VideoProxyPointer &bwp : d->widgets.values())
        bwp->setPlayList(d->videos);
#endif
}

void WallpaperEngine::build()
{
    // clean up invalid widget
    auto cleanupInvalidWidgets = [&] {
        auto winMap = rootMap();
        for (const QString &sp : d->widgets.keys()) {
            if (!winMap.contains(sp)) {
                auto videoProxy = d->widgets.take(sp);
                videoProxy.clear();
                fmInfo() << "remove screen:" << sp;
            }
        }
    };

    QList<QWidget *> root = ddplugin_desktop_util::desktopFrameRootWindows();
    if (root.size() == 1) {
        QWidget *primary = root.first();
        if (primary == nullptr) {
            // get screen failed, clear all widget
            d->clearWidgets();
            fmCritical() << "get primary screen failed return.";
            cleanupInvalidWidgets();
            return;
        }

        const QString screenName = getScreenName(primary);
        if (screenName.isEmpty()) {
            fmWarning() << "can not get screen name from root window";
            cleanupInvalidWidgets();
            return;
        }

        VideoProxyPointer bwp = d->widgets.value(screenName);
        if (!bwp.isNull()) {
            // update widget
            bwp->setParent(primary);
            QRect geometry = d->relativeGeometry(primary->geometry()); // scaled area
            bwp->setGeometry(geometry);
        } else {
            // add new widget
            fmInfo() << "screen:" << screenName << "added, create it.";
            bwp = d->createWidget(primary);
            d->widgets.insert(screenName, bwp);
        }
    } else {
        // check whether to add
        for (QWidget *win : root) {
            const QString screenName = getScreenName(win);
            if (screenName.isEmpty()) {
                fmWarning() << "can not get screen name from root window";
                continue;
            }

            VideoProxyPointer bwp = d->widgets.value(screenName);
            if (!bwp.isNull()) {
                // update widget
                bwp->setParent(win);
                QRect geometry = d->relativeGeometry(win->geometry()); // scaled area
                bwp->setGeometry(geometry);
            } else {
                // add new widget
                fmInfo() << "screen:" << screenName << "added, create it.";
                bwp = d->createWidget(win);
                d->widgets.insert(screenName, bwp);
            }
        }
    }

    cleanupInvalidWidgets();
}

void WallpaperEngine::onDetachWindows()
{
    for (const VideoProxyPointer &bwp : d->widgets.values())
        bwp->setParent(nullptr);
}

void WallpaperEngine::geometryChanged()
{
    auto winMap = rootMap();
    for (auto itor = d->widgets.begin(); itor != d->widgets.end(); ++itor) {
        VideoProxyPointer bw = itor.value();
        auto *win = winMap.value(itor.key());
        if (win == nullptr) {
            fmCritical() << "can not get root " << itor.key() << getScreenName(win);
            continue;
        }

        if (bw.get() != nullptr) {
            QRect geometry = d->relativeGeometry(win->geometry()); // scaled area
            bw->setGeometry(geometry);
        }
    }
}

void WallpaperEngine::play()
{
    if (WpCfg->enable()) {
#ifndef USE_LIBDMR
        if (d->videos.isEmpty()) {
            return;
        }
        // TODO: implement playlist
        d->player->setSource(d->videos.constFirst());
        d->player->play();
#else
        for (const VideoProxyPointer &bwp : d->widgets.values())
            bwp->setPlayList(d->videos);
#endif
        d->setBackgroundVisible(false);
        show();
    }
}

void WallpaperEngine::show()
{
    // relayout
    dpfSlotChannel->push("ddplugin_core", "slot_DesktopFrame_LayoutWidget");
    for (const VideoProxyPointer &bwp : d->widgets.values())
        bwp->show();
}

void WallpaperEngine::releaseMemory()
{
    // QMetaObject::invokeMethod(
    //     this, [] { malloc_trim(0); },
    //     Qt::QueuedConnection);

    /**
     * FIXME: adding a little delay seems that
     * can release as much memory as possible
     */
    QTimer::singleShot(800, this, [=] {
        malloc_trim(0);
    });
}

bool WallpaperEngine::registerMenu()
{
    if (dfmplugin_menu_util::menuSceneContains("CanvasMenu")) {
        // register menu for canvas
        dfmplugin_menu_util::menuSceneRegisterScene(VideoWallpaerMenuCreator::name(), new VideoWallpaerMenuCreator());
        dfmplugin_menu_util::menuSceneBind(VideoWallpaerMenuCreator::name(), "CanvasMenu");

        dpfSignalDispatcher->unsubscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded", this, &WallpaperEngine::registerMenu);
        return true;
    } else
        return false;
}

void WallpaperEngine::checkResource()
{
    if (d->videos.isEmpty()) {
        QString text = tr("Please add the video file to %0").arg(d->sourcePath());
        QDBusInterface notify("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications");
        notify.setTimeout(1000);
        QDBusPendingReply<uint> p = notify.asyncCall(QString("Notify"),
                                                     QString("Video Wallpaper"), // title
                                                     static_cast<uint>(0),
                                                     QString("deepin-toggle-desktop"), // icon
                                                     text,
                                                     QString(), QStringList(), QVariantMap(), 5000);
    }
}

#ifndef USE_LIBDMR
void WallpaperEngine::catchImage(const QVideoFrame &frame)
{
    /**
     * FIXME: QVideoFrame::toImage might cause SIGSEGV
     * in Qt6, seems to be QVideoFrame mapped failed
     * so try to map manually first
     *
     * (related to vdpau/vaapi?)
     */
    QVideoFrame tmp(frame);
    bool ret = tmp.map(QVideoFrame::ReadOnly);
    if (!ret) {
        return;
    }
    // release memcpy
    tmp.unmap();

    for (const VideoProxyPointer &bwp : d->widgets.values())
        bwp->updateImage(frame.toImage());
}
#endif
