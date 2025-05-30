#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QBuffer>
#include <QtCore>
#include <QPainter>
#include <QPen>

#include <Qt3DRender/QCamera>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImage>
#include <Qt3DExtras/QPlaneMesh>
#include <Qt3DRender/QMaterial>
#include <Qt3DExtras/QDiffuseSpecularMaterial>
#include <Qt3DExtras/QTextureMaterial>
#include <Qt3DCore/QEntity>

#include <Qt3DExtras/QForwardRenderer>

#include <Qt3DRender/QPointLight>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/Qt3DWindow>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Central widget with grid layout for 2D and 3D views.
    QGridLayout *grid = new QGridLayout;
    QWidget *central = new QWidget(this);
    central->setLayout(grid);
    setCentralWidget(central);

    // Create 3 QLabel views for 2D images.
    for (int i = 0; i < 3; ++i) {
        views[i] = new QLabel(this);
        views[i]->setMinimumSize(200, 200);
        views[i]->setStyleSheet("background-color: black;");
        views[i]->setAlignment(Qt::AlignCenter);
        grid->addWidget(views[i], i / 2, i % 2);
    }

    // Create Qt3D window and container for the 3D view.
    view3D = new Qt3DExtras::Qt3DWindow();
    container3D = QWidget::createWindowContainer(view3D);
    //container3D->setMinimumSize(400, 300); // adjust size as needed

    // Make sure the container accepts mouse events.
    container3D->setFocusPolicy(Qt::StrongFocus);
    container3D->setMouseTracking(true);
    grid->addWidget(container3D, 1, 1); // last cell in grid

    setupSlider();
    setupMenus();
    setup3DView();

    // Optionally, you can load a default set of images.
    // openImageSet(); // uncomment if you want to load images from file
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setupMenus() {
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QAction *openSetAct = fileMenu->addAction("Open &Image Set...");
    connect(openSetAct, &QAction::triggered, this, &MainWindow::openImageSet);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &MainWindow::close);

    QMenu *editMenu = menuBar()->addMenu("&Edit");
    QAction *editImageAct = editMenu->addAction("Edit Current &Image...");
    connect(editImageAct, &QAction::triggered, this, &MainWindow::openEditWindow);

}

void MainWindow::setupSlider() {
    slider = new QSlider(Qt::Horizontal, this);
    slider->setMinimum(0);
    slider->setMaximum(0);
    slider->setValue(0);
    slider->setEnabled(false);
    connect(slider, &QSlider::valueChanged, this, &MainWindow::onSliderChanged);
    statusBar()->addPermanentWidget(slider, 1);
}

void MainWindow::setup3DView() {
    // Create the root entity for the 3D scene.
    rootEntity = new Qt3DCore::QEntity();

    addCoordinateAxes(rootEntity);

    view3D->defaultFrameGraph()->setClearColor(QColor(128, 128, 128));

    // Setup camera: place it back along the Z-axis so it sees the slice volume.
    Qt3DRender::QCamera *camera = view3D->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 4.0f/3.0f, 0.1f, 2000.0f);
    camera->setPosition(QVector3D(0, 0, 1000));   // along Z-axis
    camera->setViewCenter(QVector3D(0, 0, 0));

    // Setup a simple white point light.
    Qt3DCore::QEntity *lightEntity = new Qt3DCore::QEntity(rootEntity);
    auto light = new Qt3DRender::QPointLight(lightEntity);
    light->setColor("white");
    light->setIntensity(1);
    lightEntity->addComponent(light);
    auto lightTransform = new Qt3DCore::QTransform();
    lightTransform->setTranslation(camera->position());
    lightEntity->addComponent(lightTransform);

    // Create the orbit camera controller and store it as a member.
    camController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    camController->setCamera(camera);
    camController->setLinearSpeed(50.0f);
    camController->setLookSpeed(180.0f);

    // Attach the root entity to the view.
    view3D->setRootEntity(rootEntity);

    container3D->setFocusPolicy(Qt::StrongFocus);
    container3D->setMouseTracking(true);
    container3D->setFocus();
}


void MainWindow::openImageSet() {
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
        "Select Image Slices",
        "",
        "Images (*.png *.jpg *.jpeg *.bmp)");

    if (fileNames.isEmpty())
        return;

    imageSlices.clear();
    const int targetWidth = 256;
    const int targetHeight = 256;
    for (const QString &filePath : fileNames) {
        cv::Mat img = cv::imread(filePath.toStdString(), cv::IMREAD_GRAYSCALE);
        if (!img.empty()) {
            if (img.cols != targetWidth || img.rows != targetHeight) {
                cv::resize(img, img, cv::Size(targetWidth, targetHeight));
            }
            imageSlices.append(img);
        }
    }
    if (imageSlices.isEmpty()) {
        QMessageBox::warning(this, "Error", "No valid images loaded.");
        slider->setEnabled(false);
        return;
    }
    slider->setMaximum(imageSlices.size() - 1);
    slider->setValue(0);
    slider->setEnabled(true);
    currentIndex = 0;
    loadAndDisplayImages();
}


void MainWindow::loadAndDisplayImages() {
    if (imageSlices.isEmpty() || currentIndex < 0 || currentIndex >= imageSlices.size())
        return;

    const int depth = imageSlices.size();
    const int height = imageSlices[0].rows;
    const int width = imageSlices[0].cols;

    // Axial view (already loaded)
    const cv::Mat &axial = imageSlices[currentIndex];
    // Axial view: XY plane
    views[0]->setPixmap(QPixmap::fromImage(
        drawAxisLines(matToQImage(axial), currentIndex, currentIndex, Qt::red, Qt::green)));


    // Coronal view: YZ slice (height vs depth)
    cv::Mat coronal(height, depth, CV_8UC1);
    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y)
            coronal.at<uchar>(y, z) = imageSlices[z].at<uchar>(y, currentIndex);  // X = currentIndex

    // Coronal view: YZ plane
    views[1]->setPixmap(QPixmap::fromImage(
        drawAxisLines(matToQImage(coronal), currentIndex, currentIndex, Qt::blue, Qt::green)));

    // Sagittal view: XZ slice (width vs depth)
    cv::Mat sagittal(depth, width, CV_8UC1);
    for (int z = 0; z < depth; ++z)
        for (int x = 0; x < width; ++x)
            sagittal.at<uchar>(z, x) = imageSlices[z].at<uchar>(currentIndex, x);  // Y = currentIndex

    // Sagittal view: XZ plane
    views[2]->setPixmap(QPixmap::fromImage(
        drawAxisLines(matToQImage(sagittal), currentIndex, currentIndex, Qt::blue, Qt::red)));


    // Update 3D view
    update3DView();

    statusBar()->showMessage(QString("Showing slice %1 / %2")
        .arg(currentIndex + 1).arg(imageSlices.size()));
}


QImage MainWindow::matToQImage(const cv::Mat &mat) {
    if (mat.type() == CV_8UC1)
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    else if (mat.type() == CV_8UC3)
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888).rgbSwapped();
    return QImage();
}


#include <QTemporaryFile>
#include <QStandardPaths>
#include <Qt3DExtras/QCuboidMesh>


void MainWindow::update3DView() {
    if (sliceContainerEntity) {
        delete sliceContainerEntity;
        sliceContainerEntity = nullptr;
    }

    if (imageSlices.isEmpty())
        return;

    sliceContainerEntity = new Qt3DCore::QEntity(rootEntity);

    const int numSlices = imageSlices.size();
    const float sliceSpacing = voxelSize.z(); // Z voxel spacing
    const int half = numSlices / 2;

    for (int i = 0; i < numSlices; ++i) {
        const cv::Mat &slice = imageSlices[i];
        QImage qimg = matToQImage(slice).convertToFormat(QImage::Format_RGBA8888);

        // Save to temp file
        QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString filename = tmpDir + QString("/slice_%1.png").arg(i);
        qimg.save(filename);

        // Texture
        auto *texture = new Qt3DRender::QTexture2D();
        auto *textureImage = new Qt3DRender::QTextureImage();
        textureImage->setSource(QUrl::fromLocalFile(filename));
        texture->addTextureImage(textureImage);
        texture->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        texture->setGenerateMipMaps(true);

        auto *material = new Qt3DExtras::QTextureMaterial();
        material->setTexture(texture);

        auto *boxMesh = new Qt3DExtras::QCuboidMesh();
        boxMesh->setXExtent(qimg.width() * voxelSize.x());
        boxMesh->setYExtent(qimg.height() * voxelSize.y());
        boxMesh->setZExtent(1.0f);  // 1 cm thickness


        // Position
        auto *transform = new Qt3DCore::QTransform();
        float yPos = (i - half) * sliceSpacing;
        transform->setTranslation(QVector3D(0, 0, yPos));

        // Entity
        auto *sliceEntity = new Qt3DCore::QEntity(sliceContainerEntity);
        sliceEntity->addComponent(boxMesh);
        sliceEntity->addComponent(material);
        sliceEntity->addComponent(transform);
    }
}


void MainWindow::onSliderChanged(int value) {
    if (value >= 0 && value < imageSlices.size()) {
        currentIndex = value;
        loadAndDisplayImages();
    }
}


cv::Mat3b MainWindow::volumeAsMat3b() {
    int depth = imageSlices.size();
    int height = imageSlices[0].rows;
    int width = imageSlices[0].cols;

    cv::Mat3b volume(depth, height, width);  // 3D matrix

    for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                volume(z, y, x) = imageSlices[z].at<uchar>(y, x);
            }
        }
    }

    return volume;
}



#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DCore/QTransform>

void MainWindow::addCoordinateAxes(Qt3DCore::QEntity *parent) {
    float arrowLength = 200.0f;
    float arrowRadius = 0.5f;

    struct Axis {
        QVector3D direction;
        QColor color;
    };

    QVector<Axis> axes = {
        { QVector3D(1, 0, 0), Qt::red   }, // X
        { QVector3D(0, 1, 0), Qt::green }, // Y
        { QVector3D(0, 0, 1), Qt::blue  }  // Z
    };

    for (const Axis &axis : axes) {
        auto *cylinder = new Qt3DExtras::QCylinderMesh();
        cylinder->setLength(arrowLength);
        cylinder->setRadius(arrowRadius);
        cylinder->setRings(10);
        cylinder->setSlices(20);

        auto *material = new Qt3DExtras::QPhongMaterial();
        material->setDiffuse(axis.color);

        auto *transform = new Qt3DCore::QTransform();

        // Align cylinder with axis
        QQuaternion rotation = QQuaternion::rotationTo(QVector3D(0, 1, 0), axis.direction);
        transform->setRotation(rotation);
        transform->setTranslation(axis.direction.normalized() * arrowLength / 2.0f);

        auto *arrowEntity = new Qt3DCore::QEntity(parent);
        arrowEntity->addComponent(cylinder);
        arrowEntity->addComponent(material);
        arrowEntity->addComponent(transform);
    }
}


QImage MainWindow::drawAxisLines(const QImage &image, int vertical, int horizontal,
                                  QColor verticalColor, QColor horizontalColor) {
    QImage annotated = image.convertToFormat(QImage::Format_ARGB32);
    QPainter painter(&annotated);

    if (vertical >= 0 && vertical < annotated.width()) {
        painter.setPen(QPen(verticalColor, 1));
        painter.drawLine(vertical, 0, vertical, annotated.height());
    }

    if (horizontal >= 0 && horizontal < annotated.height()) {
        painter.setPen(QPen(horizontalColor, 1));
        painter.drawLine(0, horizontal, annotated.width(), horizontal);
    }

    return annotated;
}


#include "editwindow.h"  // You'll create this class

void MainWindow::openEditWindow() {
    if (imageSlices.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No images to edit.");
        return;
    }

    EditWindow *editor = new EditWindow(QList<cv::Mat>::fromVector(imageSlices), nullptr);
    connect(editor, &EditWindow::imagesEdited, this, [=](QList<cv::Mat> newImages) {
        imageSlices = QVector<cv::Mat>(newImages.begin(), newImages.end());
        loadAndDisplayImages();
    });

    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->show();
}

