#include "segmentationwindow.h"

SegmentationWindow::SegmentationWindow(const QList<cv::Mat> &images, QWidget *parent)
    : QDialog(parent), imageSlices(images)
{
    setWindowTitle("Segmentation Tools");

    QVBoxLayout *layout = new QVBoxLayout(this);

    segmentationCombo = new QComboBox(this);
    segmentationCombo->addItems({
        "Otsu Threshold",
        "Binary Threshold",
        "Adaptive Threshold",
        "Canny Edges"
    });

    layout->addWidget(segmentationCombo);

    applyButton = new QPushButton("Apply", this);
    undoButton = new QPushButton("Undo", this);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(applyButton);
    btnLayout->addWidget(undoButton);

    layout->addLayout(btnLayout);

    connect(applyButton, &QPushButton::clicked, this, &SegmentationWindow::applySegmentation);
    connect(undoButton, &QPushButton::clicked, this, &SegmentationWindow::undo);

    resize(400, 200);
    show();
}

QImage SegmentationWindow::matToQImage(const cv::Mat &mat)
{
    return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
}

void SegmentationWindow::applySegmentation()
{
    // Deep copy of current imageSlices to save in undoStack
    QList<cv::Mat> backup;
    for (const cv::Mat &img : imageSlices) {
        backup.append(img.clone());  // Deep copy
    }
    undoStack.push(backup);  // Push deep copy to stack

    QString method = segmentationCombo->currentText();

    for (cv::Mat &img : imageSlices) {
        if (method == "Otsu Threshold") {
            cv::threshold(img, img, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
        } else if (method == "Binary Threshold") {
            cv::threshold(img, img, 128, 255, cv::THRESH_BINARY);
        } else if (method == "Adaptive Threshold") {
            cv::adaptiveThreshold(img, img, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                                  cv::THRESH_BINARY, 11, 2);
        } else if (method == "Canny Edges") {
            cv::Canny(img, img, 100, 200);
        }
    }

    emit imagesSegmented(imageSlices);
}

void SegmentationWindow::undo()
{
    if (!undoStack.isEmpty()) {
        imageSlices = undoStack.pop();
        emit imagesSegmented(imageSlices);
    }
}
