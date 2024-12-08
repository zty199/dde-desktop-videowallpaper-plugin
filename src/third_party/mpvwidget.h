#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H

#include <QOpenGLWidget>

#include <mpv/client.h>
#include <mpv/render_gl.h>

class MpvWidget final : public QOpenGLWidget
{
    Q_OBJECT

public:
    MpvWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
    ~MpvWidget();

    void command(const QVariant &params);

    void setProperty(const QString &name, const QVariant &value);
    QVariant getProperty(const QString &name) const;

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    void handle_mpv_event(mpv_event *event);

    static void on_update(void *ctx);
    static void wakeup(void *ctx);
    static void *get_proc_address(void *ctx, const char *name);

signals:
    void durationChanged(int value);
    void positionChanged(int value);

private slots:
    void on_mpv_events();
    void maybeUpdate();

private:
    mpv_handle *mpv;
    mpv_render_context *mpv_gl;
};

#endif // PLAYERWINDOW_H
