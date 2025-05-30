#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSlider>
#include <QLabel>
#include <QVector>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

// Include required Qt3D headers:
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DCore/QEntity>
#include <Qt3DExtras/QOrbitCameraController>
#include <QVector3D>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    // 2D display views (you already had 3 QLabel views for 2D)
    QLabel *views[3];

    // Slider and image slices (using OpenCV cv::Mat)
    QSlider *slider;
    QVector<cv::Mat> imageSlices;
    int currentIndex = 0;

    // Qt3D members:
    Qt3DExtras::Qt3DWindow *view3D;
    QWidget *container3D;
    Qt3DCore::QEntity *rootEntity;
    Qt3DExtras::QOrbitCameraController *camController; // now a member variable!

    // Setup functions:
    void setupMenus();
    void setupSlider();
    void setup3DView();
    void update3DView();

    // Your helper:
    QImage matToQImage(const cv::Mat &mat);

    Qt3DCore::QEntity* sliceContainerEntity = nullptr;
    QVector3D voxelSize = QVector3D(1.0f, 1.0f, 5.0f); // x: width, y: height, z: slice spacing



private slots:
    void openImageSet();
    void loadAndDisplayImages();
    void onSliderChanged(int value);


protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

};

#endif // MAINWINDOW_H
