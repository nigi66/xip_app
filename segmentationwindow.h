#ifndef SEGMENTATIONWINDOW_H
#define SEGMENTATIONWINDOW_H

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QStack>
#include <opencv2/opencv.hpp>

class SegmentationWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SegmentationWindow(const QList<cv::Mat> &images, QWidget *parent = nullptr);

signals:
    void imagesSegmented(QList<cv::Mat> segmentedImages);

private slots:
    void applySegmentation();
    void undo();

private:
    QList<cv::Mat> imageSlices;
    QStack<QList<cv::Mat>> undoStack;

    QComboBox *segmentationCombo;
    QPushButton *applyButton;
    QPushButton *undoButton;

    QImage matToQImage(const cv::Mat &mat);
};

#endif // SEGMENTATIONWINDOW_H
