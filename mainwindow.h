// mainwindow.h
#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QVector>
#include <opencv2/opencv.hpp>
#include <QSlider>
#include <Qt3DCore/QEntity>
#include <Qt3DExtras/Qt3DWindow>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QLabel *views[4];
    QVector<cv::Mat> imageSlices;
    int currentIndex = 0;

    QSlider *slider = nullptr;

    // Qt 3D stuff
    Qt3DExtras::Qt3DWindow *view3d = nullptr;
    QWidget *container3d = nullptr;
    Qt3DCore::QEntity *rootEntity = nullptr;

    void setupMenus();
    void setupSlider();
    void setup3DView();

    void openImageSet();
    void loadAndDisplayImages();

    QImage matToQImage(const cv::Mat &mat);
    void update3DView();

    Qt3DExtras::Qt3DWindow *view3D = nullptr;        // <-- Add this
    QWidget *container3D = nullptr;                   // <-- Add this

private slots:
    void onSliderChanged(int value);
};

