#ifndef OBJECTDETECTIONWINDOW_H
#define OBJECTDETECTIONWINDOW_H

#include <QDialog>
#include <QList>
#include <opencv2/opencv.hpp>

class QLabel;
class QPushButton;

class ObjectDetectionWindow : public QDialog {
    Q_OBJECT
public:
    explicit ObjectDetectionWindow(const QList<cv::Mat> &images, QWidget *parent = nullptr);

signals:
    void detectionCompleted(QList<cv::Mat> detectedImages);

private slots:
    void runDetection();

private:
    QList<cv::Mat> inputImages;
    QList<cv::Mat> outputImages;

    QLabel *imageLabel;
    QPushButton *detectButton;

    cv::dnn::Net yoloNet;
    void loadYoloModel();
    cv::Mat detectObjects(const cv::Mat &image);
};

#endif // OBJECTDETECTIONWINDOW_H
