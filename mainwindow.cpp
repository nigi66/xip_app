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
    container3D->setMinimumSize(400, 300); // adjust size as needed

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
    container3D->setAttribute(Qt::WA_TransparentForMouseEvents, false);
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

    const cv::Mat &img = imageSlices[currentIndex];
    if (img.empty())
        return;

    // Display the grayscale slice and its processed versions in the 2D views.
    views[0]->setPixmap(QPixmap::fromImage(matToQImage(img)));
    cv::Mat edges, thresh;
    cv::Canny(img, edges, 100, 200);
    cv::threshold(img, thresh, 120, 255, cv::THRESH_BINARY);
    views[1]->setPixmap(QPixmap::fromImage(matToQImage(edges)));
    views[2]->setPixmap(QPixmap::fromImage(matToQImage(thresh)));

    // Update the 3D view with all slices.
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

#include <QBuffer>

void MainWindow::update3DView() {
    // Clear previous slice entities (delete all children of the rootEntity that are QNodes)
    const auto children = rootEntity->children();
    for (QObject* childObj : children) {
        Qt3DCore::QNode *childNode = qobject_cast<Qt3DCore::QNode*>(childObj);
        if (childNode)
            delete childNode;
    }
    if (imageSlices.isEmpty())
        return;

    // Adjust stacking parameters:
    const int numSlices = imageSlices.size();
    const float spacing = 20.0f; // larger spacing to see separate layers clearly
    const int half = numSlices / 2;

    // For each image slice, create a textured plane.
    for (int i = 0; i < numSlices; ++i) {
        const cv::Mat &slice = imageSlices[i];

        // Convert OpenCV image to QImage.
        QImage qimg = matToQImage(slice).convertToFormat(QImage::Format_RGBA8888);

        // Save QImage to a temporary file.
        QString tmpPath = QString("slice_%1.png").arg(i);
        if (!qimg.save(tmpPath))
            qDebug() << "Failed to save" << tmpPath;

        // Create texture and load it from file.
        auto *texture = new Qt3DRender::QTexture2D(rootEntity);
        auto *textureImage = new Qt3DRender::QTextureImage();
        textureImage->setSource(QUrl::fromLocalFile(tmpPath));
        texture->addTextureImage(textureImage);
        texture->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm); // ensure alpha channel

        // Create a plane mesh using the slice dimensions.
        auto *planeMesh = new Qt3DExtras::QPlaneMesh();
        planeMesh->setWidth(slice.cols);
        planeMesh->setHeight(slice.rows);

        // Create a material that uses the texture.
        auto *material = new Qt3DExtras::QTextureMaterial();
        material->setTexture(texture);

        // Create an entity for this slice.
        Qt3DCore::QEntity *sliceEntity = new Qt3DCore::QEntity(rootEntity);
        sliceEntity->addComponent(planeMesh);
        sliceEntity->addComponent(material);

        // Position each slice along the Y axis.
        auto *transform = new Qt3DCore::QTransform();
        float yPos = (i - half) * spacing;
        transform->setTranslation(QVector3D(0, yPos, 0));
        sliceEntity->addComponent(transform);
    }
}

void MainWindow::onSliderChanged(int value) {
    if (value >= 0 && value < imageSlices.size()) {
        currentIndex = value;
        loadAndDisplayImages();
    }
}


bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == container3D && (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)) {
        qDebug() << "Mouse event in 3D container!";
    }
    return QMainWindow::eventFilter(obj, event);
}

