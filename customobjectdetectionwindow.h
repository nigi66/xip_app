#ifndef CUSTOMOBJECTDETECTIONWINDOW_H
#define CUSTOMOBJECTDETECTIONWINDOW_H

#include <QDialog>
#include <QList>
#include <QImage>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <opencv2/opencv.hpp>

class CustomObjectDetectionWindow : public QDialog
{
    Q_OBJECT
public:
    explicit CustomObjectDetectionWindow(const QList<cv::Mat> &images, QWidget *parent = nullptr);
    ~CustomObjectDetectionWindow();

signals:
    void detectionCompleted(const QList<cv::Mat> &detectedImages);

private slots:
    void runDetection();
    void closeWindow();

private:
    QList<cv::Mat> originalImages;
    QList<cv::Mat> detectedImages;

    QLabel *statusLabel;
    QPushButton *runButton;
    QPushButton *closeButton;

    QVBoxLayout *mainLayout;
    QScrollArea *scrollArea;
    QWidget *imageContainer;
    QVBoxLayout *imageLayout;

    cv::dnn::Net net;

    void loadModel(const QString &modelPath);
    cv::Mat preprocess(const cv::Mat &image);
    QList<cv::Rect> postprocess(const cv::Mat &image, const cv::Mat &outputs, float confThreshold);

    void displayImages(const QList<cv::Mat> &images);
    void drawBoxes(cv::Mat &image, const QList<cv::Rect> &boxes, const std::vector<float> &confidences);
};

#endif // CUSTOMOBJECTDETECTIONWINDOW_H
