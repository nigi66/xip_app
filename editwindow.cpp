#include "editwindow.h"
#include <QDesktopWidget>  // Optional for screen geometry if needed


EditWindow::EditWindow(const QList<cv::Mat> &images, QWidget *parent)
    : QDialog(parent), imageSlices(images)
{
    setWindowTitle("Edit Images");

    QVBoxLayout *layout = new QVBoxLayout(this);

    filterCombo = new QComboBox(this);
    filterCombo->addItems({
        "None",
        "Gaussian Blur",
        "Sharpen",
        "Edge Detection",
        "Invert",
        "Brightness +",
        "Brightness -",
        "Contrast +",
        "Contrast -"
    });
    layout->addWidget(filterCombo);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    applyButton = new QPushButton("Apply", this);
    undoButton = new QPushButton("Undo", this);
    btnLayout->addWidget(applyButton);
    btnLayout->addWidget(undoButton);
    layout->addLayout(btnLayout);

//    preview = new QLabel(this);
//    preview->setFixedSize(256, 256);
//    preview->setStyleSheet("border: 1px solid gray;");
//    layout->addWidget(preview, 0, Qt::AlignHCenter);

    connect(applyButton, &QPushButton::clicked, this, &EditWindow::applyFilter);
    connect(undoButton, &QPushButton::clicked, this, &EditWindow::undoLast);

    // Show preview of first image
    //if (!imageSlices.isEmpty())
     //   preview->setPixmap(QPixmap::fromImage(matToQImage(imageSlices.first())));

    resize(400, 300);  // Ensures the window is a usable size
    move(100, 100);    // Optional: place it somewhere visible on screen
    show();            // Ensure it becomes visible when instantiated

}


QImage EditWindow::matToQImage(const cv::Mat &mat)
{
    return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
}

void EditWindow::applyFilter()
{
    // Deep copy for undo
    QList<cv::Mat> copiedSlices;
    for (const cv::Mat &img : imageSlices) {
        copiedSlices.append(img.clone());
    }
    undoStack.push(copiedSlices);  // Push deep copy

    QString filter = filterCombo->currentText();

    for (cv::Mat &img : imageSlices) {
        if (filter == "Gaussian Blur") {
            cv::GaussianBlur(img, img, cv::Size(5, 5), 1.5);
        } else if (filter == "Sharpen") {
            cv::Mat kernel = (cv::Mat_<float>(3,3) <<
                               0, -1,  0,
                              -1,  5, -1,
                               0, -1,  0);
            cv::filter2D(img, img, img.depth(), kernel);
        } else if (filter == "Edge Detection") {
            cv::Mat edges;
            cv::Canny(img, edges, 50, 150);
            img = edges;
        } else if (filter == "Invert") {
            img = 255 - img;
        } else if (filter == "Brightness +") {
            img += cv::Scalar(30);
        } else if (filter == "Brightness -") {
            img -= cv::Scalar(30);
        } else if (filter == "Contrast +") {
            img.convertTo(img, -1, 1.2, 0);  // alpha > 1
        } else if (filter == "Contrast -") {
            img.convertTo(img, -1, 0.8, 0);  // alpha < 1
        }
    }

    //preview->setPixmap(QPixmap::fromImage(matToQImage(imageSlices.first())));
    emit imagesEdited(imageSlices);
}


void EditWindow::undoLast()
{
    if (!undoStack.isEmpty()) {
        imageSlices = undoStack.pop();
        //preview->setPixmap(QPixmap::fromImage(matToQImage(imageSlices.first())));
        emit imagesEdited(imageSlices);
    }
}
