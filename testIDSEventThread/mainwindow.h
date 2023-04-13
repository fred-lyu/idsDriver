#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QThread>
#include <QProgressDialog>
#include <QMessageBox>

#include "eventthread.h"
#include "camera.h"
#include "cameralist.h"
#include "queyeimage.h"

namespace Ui {
class MainWindow;
}

class InitCameraThread : public QThread
{
private:
    QSharedPointer<Camera> m_Camera;
    UEYE_CAMERA_INFO camera_info;
    int retval{};
public:
    InitCameraThread(QSharedPointer<Camera> Camera, UEYE_CAMERA_INFO info) :
        m_Camera(std::move(Camera)),
        camera_info(info) {}

    void run() override
    {
        retval = m_Camera->Open(camera_info, true);
    }
    int getRetval() const {
        return retval;
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


protected slots:
    void onCameraOpenFinished();
    void onFrameReceived(ImageBufferPtr buffer);

signals:
    void cameraOpenFinished();
    void updateDisplay(const QImage& image);
    void updateImageInfo(const UEYEIMAGEINFO& image_info);

private:
    Ui::MainWindow *ui;

    InitCameraThread *m_pInitCamThread;

    QSharedPointer<Camera> m_camera;

    UEYE_CAMERA_INFO m_info;

    QProgressDialog* m_dialog;


};

#endif // MAINWINDOW_H
