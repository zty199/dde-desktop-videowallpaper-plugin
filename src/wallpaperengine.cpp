// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wallpaperengine.h"
#include "wallpaperengine_p.h"
#include "wallpaperconfig.h"
#include "videowallpapermenuscene.h"

#include <dfm-base/dfm_desktop_defines.h>
#include <dfm-base/utils/universalutils.h>
#include <desktoputils/ddpugin_eventinterface_helper.h>
#include <desktoputils/menu_eventinterface_helper.h>
#include <desktoputils/widgetutil.h>

#include <DPlatformWindowHandle>
#include <DNotifySender>

#include <QDir>
#include <QStandardPaths>
#ifndef USE_LIBMPV
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
    Q_ASSERT(win);
    return win->property(DesktopFrameProperty::kPropScreenName).toString();
}

static QMap<QString, QWidget *> rootMap()
{
    QList<QWidget *> root = ddplugin_desktop_util::desktopFrameRootWindows();
    QMap<QString, QWidget *> ret;
    for (QWidget *win : root) {
        QString name = getScreenName(win);
        if (name.isEmpty()) {
            continue;
        }
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
    for (const QFileInfo &file : dir.entryInfoList(QDir::Files)) {
        ret << QUrl::fromLocalFile(file.absoluteFilePath());
    }

    return ret;
}

VideoProxyPointer WallpaperEnginePrivate::createWidget(QWidget *root)
{
    /**
     * NOTE: https://doc.qt.io/qt-6/qopenglwidget.html
     *
     * When dynamically adding a QOpenGLWidget into a widget hierarchy,
     * e.g. by parenting a new QOpenGLWidget to a widget
     * where the corresponding top-level widget is already shown on screen,
     * the associated native window may get implicitly destroyed and recreated
     * if the QOpenGLWidget is the first of its kind within its window.
     *
     * This is because the window type changes from RasterSurface to OpenGLSurface
     * and that has platform-specific implications.
     * This behavior is new in Qt 6.4.
     *
     * So change root window's surfaceType to QSurface::OpenGLSurface first.
     */
    root->windowHandle()->setSurfaceType(QSurface::OpenGLSurface);

    VideoProxyPointer bwp(new VideoProxy(root));
    bwp->setProperty(DesktopFrameProperty::kPropScreenName, getScreenName(root));
    bwp->setProperty(DesktopFrameProperty::kPropWidgetName, "videowallpaper");
    bwp->setProperty(DesktopFrameProperty::kPropWidgetLevel, 5.1);
    QRect geometry = relativeGeometry(root->geometry()); // scaled area
    bwp->setGeometry(geometry);

    const QString &screenName = getScreenName(root);
    fmDebug() << "screen name" << screenName << "geometry" << root->geometry() << bwp.get();

    bwp->hide();

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

    connect(WpCfg, &WallpaperConfig::changeEnableState, this, [this](bool e) {
        if (WpCfg->enable() == e) {
            return;
        }

        WpCfg->setEnable(e);
        if (e) {
            turnOn();
        } else {
            turnOff();
        }
    });

    if (WpCfg->enable()) {
        turnOn();
    }

    return true;
}

void WallpaperEngine::turnOn(bool b)
{
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowAboutToBeBuilded, &WallpaperEngine::onDetachWindows);
    CanvasCoreSubscribe(signal_DesktopFrame_WindowBuilded, &WallpaperEngine::build);
    CanvasCoreSubscribe(signal_DesktopFrame_WindowShowed, &WallpaperEngine::play);
    CanvasCoreSubscribe(signal_DesktopFrame_GeometryChanged, &WallpaperEngine::geometryChanged);

    d->watcher = new QFileSystemWatcher(this);
    d->watcher->addPath(d->sourcePath());
    /**
     * FIXME: when a large video file is copied to directory,
     * in which case player might not be able to play.
     */
    connect(d->watcher, &QFileSystemWatcher::directoryChanged, this, &WallpaperEngine::refreshSource);

#ifndef USE_LIBMPV
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

    if (b) {
        build();
        refreshSource();
        show();
    }
}

void WallpaperEngine::turnOff()
{
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowAboutToBeBuilded, &WallpaperEngine::onDetachWindows);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowBuilded, &WallpaperEngine::build);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowShowed, &WallpaperEngine::play);
    CanvasCoreUnsubscribe(signal_DesktopFrame_GeometryChanged, &WallpaperEngine::geometryChanged);

    delete d->watcher;
    d->watcher = nullptr;

#ifdef USE_LIBMPV
    for (const VideoProxyPointer &bwp : d->widgets.values()) {
        bwp->command(QVariantList {"stop"});
        bwp->hide();
    }
#else
    d->player->setSource(QUrl());
    delete d->player;
    d->player = nullptr;

    delete d->surface;
    d->surface = nullptr;

    d->clearWidgets();
#endif

    d->videos.clear();

    // show background.
    d->setBackgroundVisible(true);

#ifdef USE_LIBMPV
    // release memory
    releaseMemory();
#endif
}

void WallpaperEngine::refreshSource()
{
    d->videos = d->getVideos(d->sourcePath());
    checkResource();

    if (d->videos.isEmpty()) {
#ifdef USE_LIBMPV
        for (const VideoProxyPointer &bwp : d->widgets.values()) {
            bwp->command(QVariantList {"stop"});
        }
        releaseMemory();
#else
        d->player->setSource(QUrl());
        for (const VideoProxyPointer &bwp : d->widgets.values()) {
            bwp->clear();
        }
#endif
        return;
    }

#ifdef USE_LIBMPV
    releaseMemory();
    for (const VideoProxyPointer &bwp : d->widgets.values()) {
        bwp->command(QVariantList {"loadfile", d->videos.constFirst().toLocalFile()});
    }
#else
    d->player->setSource(d->videos.constFirst());
    d->player->play();
#endif
}

void WallpaperEngine::build()
{
    // clean up invalid widget
    auto cleanupInvalidWidgets = [this] {
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

        const QString &screenName = getScreenName(primary);
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
    for (const VideoProxyPointer &bwp : d->widgets.values()) {
        bwp->setParent(nullptr);
    }
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
        if (d->videos.isEmpty()) {
            return;
        }
#ifdef USE_LIBMPV
        // TODO: implement playlist
        for (const VideoProxyPointer &bwp : d->widgets.values()) {
            bwp->command(QVariantList {"loadfile", d->videos.constFirst()});
        }
#else
        // TODO: implement playlist
        d->player->setSource(d->videos.constFirst());
        d->player->play();
#endif
        d->setBackgroundVisible(false);
        show();
    }
}

void WallpaperEngine::show()
{
    // relayout
    dpfSlotChannel->push("ddplugin_core", "slot_DesktopFrame_LayoutWidget");
    for (const VideoProxyPointer &bwp : d->widgets.values()) {
        bwp->show();
    }
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
    }

    return false;
}

void WallpaperEngine::checkResource()
{
    if (d->videos.isEmpty()) {
        QString text = tr("Please add the video file to %0").arg(d->sourcePath());
        Dtk::Core::DUtil::DNotifySender(text)
            .appName(tr("Video Wallpaper"))
            .appIcon("deepin-toggle-desktop")
            .timeOut(5000)
            .call();
    }
}

#ifndef USE_LIBMPV
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

    for (const VideoProxyPointer &bwp : d->widgets.values()) {
        bwp->updateImage(frame.toImage());
    }
}
#endif
