// mainwindow.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QGridLayout>
#include <QHBoxLayout>
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
#include <Qt3DExtras/QOrbitCameraController>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    QGridLayout *grid = new QGridLayout;
    QWidget *central = new QWidget(this);
    central->setLayout(grid);
    setCentralWidget(central);

    // Create 3 QLabel views for 2D images
    for (int i = 0; i < 3; ++i) {
        views[i] = new QLabel(this);
        views[i]->setMinimumSize(200, 200);
        views[i]->setStyleSheet("background-color: black;");
        views[i]->setAlignment(Qt::AlignCenter);
        grid->addWidget(views[i], i / 2, i % 2);
    }

    // Create Qt3D window and container for the 3D view
    view3D = new Qt3DExtras::Qt3DWindow();
    container3D = QWidget::createWindowContainer(view3D);
    container3D->setMinimumSize(200, 200);  // same size as labels
    grid->addWidget(container3D, 1, 1);      // last cell in the 2x2 grid

    setupSlider();
    setupMenus();
    setup3DView();

    // Optionally load a default image set (replace with your folder)
    // openImageSet(); // Or comment and load manually later
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

void MainWindow::setup3DView()
{
    // setup your rootEntity, camera, light, controller etc.
    rootEntity = new Qt3DCore::QEntity();

    view3D->defaultFrameGraph()->setClearColor(QColor(128, 128, 128));

    Qt3DRender::QCamera *camera = view3D->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 4.0f/3.0f, 0.1f, 1000.0f);
    camera->setPosition(QVector3D(0, 500, 0));
    camera->setViewCenter(QVector3D(0, 0, 0));

    Qt3DCore::QEntity *lightEntity = new Qt3DCore::QEntity(rootEntity);
    auto light = new Qt3DRender::QPointLight(lightEntity);
    light->setColor("white");
    light->setIntensity(1);
    lightEntity->addComponent(light);
    auto lightTransform = new Qt3DCore::QTransform();
    lightTransform->setTranslation(camera->position());
    lightEntity->addComponent(lightTransform);

    auto camController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    camController->setLinearSpeed(50.0f);
    camController->setLookSpeed(180.0f);
    camController->setCamera(camera);

    view3D->setRootEntity(rootEntity);
}



void MainWindow::openImageSet() {
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
        "Select Image Slices",
        "",
        "Images (*.png *.jpg *.jpeg *.bmp)");

    if (fileNames.isEmpty()) return;

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

    const cv::Mat &img = imageSlices[currentIndex];
    if (img.empty()) return;

    // Show grayscale slice in views[0]
    views[0]->setPixmap(QPixmap::fromImage(matToQImage(img)));

    // Show simple edge and threshold versions in views 1 and 2
    cv::Mat edges, thresh;
    cv::Canny(img, edges, 100, 200);
    cv::threshold(img, thresh, 120, 255, cv::THRESH_BINARY);

    views[1]->setPixmap(QPixmap::fromImage(matToQImage(edges)));
    views[2]->setPixmap(QPixmap::fromImage(matToQImage(thresh)));

    // Update 3D stack view
    update3DView();

    statusBar()->showMessage(QString("Showing slice %1 / %2").arg(currentIndex + 1).arg(imageSlices.size()));
}

QImage MainWindow::matToQImage(const cv::Mat &mat) {
    if (mat.type() == CV_8UC1)
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    else if (mat.type() == CV_8UC3)
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888).rgbSwapped();
    return QImage();
}

#include <QTemporaryFile>
#include <QBuffer>

// ...

void MainWindow::update3DView() {
    // Clear previous entities (except root)
    auto children = rootEntity->children();
    for (QObject* childObj : children) {
        Qt3DCore::QNode *childNode = qobject_cast<Qt3DCore::QNode*>(childObj);
        if (childNode)
            delete childNode;
    }

    if (imageSlices.isEmpty()) return;

    int spacing = 2; // distance between slices on Z axis
    int half = imageSlices.size() / 2;

    for (int i = 0; i < imageSlices.size(); ++i) {
        const cv::Mat &slice = imageSlices[i];

        // Convert slice to QImage
        QImage qimg = matToQImage(slice).convertToFormat(QImage::Format_RGBA8888);

        // Save qimg to a temporary file (or to disk, for demo save to disk)
        QString tmpPath = QString("slice_%1.png").arg(i);
        qimg.save(tmpPath);

        // Create texture and load from file URL
        auto texture = new Qt3DRender::QTexture2D(rootEntity);
        auto textureImage = new Qt3DRender::QTextureImage(texture);
        textureImage->setSource(QUrl::fromLocalFile(tmpPath));
        texture->addTextureImage(textureImage);

        // Plane mesh
        auto planeMesh = new Qt3DExtras::QPlaneMesh();
        planeMesh->setWidth(slice.cols);
        planeMesh->setHeight(slice.rows);

        // Material with texture
        auto material = new Qt3DExtras::QTextureMaterial();
        material->setTexture(texture);

        // Entity
        Qt3DCore::QEntity *sliceEntity = new Qt3DCore::QEntity(rootEntity);
        sliceEntity->addComponent(planeMesh);
        sliceEntity->addComponent(material);

        auto transform = new Qt3DCore::QTransform();
        float zPos = (i - half) * spacing;
        transform->setTranslation(QVector3D(0, 0, zPos));
        sliceEntity->addComponent(transform);
    }
}


void MainWindow::onSliderChanged(int value) {
    if (value >= 0 && value < imageSlices.size()) {
        currentIndex = value;
        loadAndDisplayImages();
    }
}

