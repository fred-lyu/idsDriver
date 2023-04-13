#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    CameraList cameraList;

    if(cameraList.isSingleCamOpenable())
    {
        cameraList.selectAll();
        cameraList.accept();

        auto infoList = cameraList.cameraInfo();
        qDebug() << "camera count:" << infoList.size();
        for (auto& camInfo : infoList)
        {
            m_info = camInfo;
        }
    }


    m_camera = QSharedPointer<Camera>(new Camera);
    m_pInitCamThread = new InitCameraThread(m_camera, m_info);
    connect(m_pInitCamThread, &InitCameraThread::finished, this, &MainWindow::onCameraOpenFinished);
    m_pInitCamThread->start();

    m_dialog = new QProgressDialog(parent);
    m_dialog->setLabelText(tr("Opening camera %1 [%2] ...").arg(QString(m_info.Model), QString::number(m_info.dwDeviceID)));
    m_dialog->setCancelButton(nullptr);
    m_dialog->setRange(0, 0);
    m_dialog->setAutoClose(false);
    m_dialog->setMinimumDuration(0);
    m_dialog->setWindowModality(Qt::ApplicationModal);
    m_dialog->setMinimumWidth(300);
    m_dialog->show();

    connect(this, &MainWindow::cameraOpenFinished, this, [this]() {
        if (1)
        {
            m_camera->captureVideo(true);
            m_camera->setTriggerMode(IS_SET_TRIGGER_HI_LO);
        }

    });




}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onCameraOpenFinished()
{
    int cameraOpenRetval = m_pInitCamThread->getRetval();
    delete m_pInitCamThread;

    if(m_dialog)
    {
        m_dialog->close();
        m_dialog->deleteLater();
        m_dialog = nullptr;
    }

    if (cameraOpenRetval == IS_SUCCESS)
    {
        setWindowTitle(m_camera->WindowTitle());

        const auto colorMode{qvariant_cast<SENSORINFO>(m_camera->sensorInfo()).nColorMode};

        int colormode = 0;
        if (IS_COLORMODE_BAYER == colorMode)
        {
            colormode = IS_CM_RGB8_PACKED;
        }
        else if (IS_COLORMODE_MONOCHROME == colorMode)
        {
            colormode = IS_CM_MONO8;
        }
        else
        {
            colormode = m_camera->colorMode().toInt();
        }

        if (colormode != 0 && m_camera->colorMode.setValue(colormode) != IS_SUCCESS)
        {
            QMessageBox::information(this, tr("Error!"), tr("SetColorMode failed"), QMessageBox::Ok);
        }

        /* get some special camera properties */
        ZeroMemory (&m_camera->m_CameraProps, sizeof(m_camera->m_CameraProps));

        // If the camera does not support a continuous AOI -> it uses special image formats
        m_camera->m_CameraProps.bUsesImageFormats = false;
        INT nAOISupported = 0;
        if (is_ImageFormat(m_camera->getCameraHandle(), IMGFRMT_CMD_GET_ARBITRARY_AOI_SUPPORTED, reinterpret_cast<void*>(&nAOISupported),
                           sizeof(nAOISupported)) == IS_SUCCESS)
        {
            m_camera->m_CameraProps.bUsesImageFormats = (nAOISupported == 0);
        }

        /* set the default image format, if used */
        if (m_camera->m_CameraProps.bUsesImageFormats)
        {
            // search the default formats
            m_camera->m_CameraProps.nImgFmtNormal  = m_camera->searchDefImageFormats(CAPTMODE_FREERUN | CAPTMODE_SINGLE);
            m_camera->m_CameraProps.nImgFmtDefaultNormal = m_camera->m_CameraProps.nImgFmtNormal;
            m_camera->m_CameraProps.nImgFmtTrigger = m_camera->searchDefImageFormats(CAPTMODE_TRIGGER_SOFT_SINGLE);
            m_camera->m_CameraProps.nImgFmtDefaultTrigger = m_camera->m_CameraProps.nImgFmtTrigger;
            // set the default formats
            if ((is_ImageFormat(m_camera->getCameraHandle(), IMGFRMT_CMD_SET_FORMAT, reinterpret_cast<void*>(&m_camera->m_CameraProps.nImgFmtNormal),
                                sizeof(m_camera->m_CameraProps.nImgFmtNormal))) == IS_SUCCESS)
            {
                //m_nImageFormat = nFormat;
                //bRet = TRUE;
            }
        }
        /* setup the capture parameter */
        m_camera->SetupCapture();
        connect(m_camera.data(), static_cast<void (Camera::*)(ImageBufferPtr)>(&Camera::frameReceived), this, &MainWindow::onFrameReceived, Qt::DirectConnection);
        //        connect(this, &SubWindow::updateDisplay, this, &SubWindow::onUpdateDisplay, Qt::DirectConnection);
        //        connect(this, &SubWindow::updateImageInfo, this, &SubWindow::onUpdateImageInfo);

        cameraOpenFinished();
        setVisible(true);
    }
    else
    {
        QMessageBox::warning(this, "Camera open", QString("Could not open camera, Error code: %1").arg(cameraOpenRetval));
        close();
    }
}

void MainWindow::onFrameReceived(ImageBufferPtr buffer)
{
    if (1)
    {
        uEyeAssist::QuEyeImage image(reinterpret_cast<uchar*>(buffer->data()),
                                     buffer->buffer_props().width,
                                     buffer->buffer_props().height,
                                     buffer->buffer_props().colorformat);

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        if (buffer->buffer_props().colorformat == IS_CM_BGR8_PACKED)
        {
            image = std::move(image).rgbSwapped();
        }
#endif

        emit updateDisplay(image);
        ui->label->setPixmap(QPixmap::fromImage(image.scaled(400,300,Qt::KeepAspectRatio)));
    }

    //    emit updateImageInfo(buffer->image_info());
    qDebug() << "device timestamp:" << buffer->image_info().u64TimestampDevice/10 << "us";
    qDebug() << "system timestamp:" << buffer->image_info().TimestampSystem.wMilliseconds << "ms";
}
