#include "customobjectdetectionwindow.h"
#include <QMessageBox>
#include <QPixmap>
#include <QDebug>
#include <QApplication>


CustomObjectDetectionWindow::CustomObjectDetectionWindow(const QList<cv::Mat> &images, QWidget *parent)
    : QDialog(parent), originalImages(images)
{
    setWindowTitle("Custom Object Detection - YOLOv11 ONNX");

    mainLayout = new QVBoxLayout(this);

    statusLabel = new QLabel("Ready to run detection.", this);
    mainLayout->addWidget(statusLabel);

    runButton = new QPushButton("Run Detection", this);
    mainLayout->addWidget(runButton);

    closeButton = new QPushButton("Close", this);
    mainLayout->addWidget(closeButton);

    // Scroll area for showing images with detections
    scrollArea = new QScrollArea(this);
    imageContainer = new QWidget(this);
    imageLayout = new QVBoxLayout(imageContainer);
    imageContainer->setLayout(imageLayout);
    scrollArea->setWidget(imageContainer);
    scrollArea->setWidgetResizable(true);
    mainLayout->addWidget(scrollArea);

    connect(runButton, &QPushButton::clicked, this, &CustomObjectDetectionWindow::runDetection);
    connect(closeButton, &QPushButton::clicked, this, &CustomObjectDetectionWindow::closeWindow);

    // Load your ONNX model - adjust path as needed
    QString modelPath = "C:/Users/Me/Documents/xip_app/model.onnx"; // <-- Change this path to your model location
    loadModel(modelPath);
}

CustomObjectDetectionWindow::~CustomObjectDetectionWindow()
{
}

void CustomObjectDetectionWindow::loadModel(const QString &modelPath)
{
    try {
        net = cv::dnn::readNetFromONNX(modelPath.toStdString());
        if (net.empty()) {
            QMessageBox::critical(this, "Error", "Failed to load ONNX model.");
        } else {
            statusLabel->setText("Model loaded successfully.");
        }
    } catch (const cv::Exception &ex) {
        QMessageBox::critical(this, "Error", QString("Exception loading model: %1").arg(ex.what()));
    }
}

cv::Mat CustomObjectDetectionWindow::preprocess(const cv::Mat &image)
{
    // Normalize slice (like Python's 2nd and 98th percentile clip and scale)
    cv::Mat imgFloat;
    image.convertTo(imgFloat, CV_32F);

    double p2, p98;
    cv::Mat sorted;
    cv::sort(imgFloat.reshape(1,1), sorted, cv::SORT_ASCENDING);
    int len = sorted.cols;
    p2 = sorted.at<float>(0, std::max(0, int(0.02 * len)));
    p98 = sorted.at<float>(0, std::min(len - 1, int(0.98 * len)));

    cv::Mat clipped;
    cv::threshold(imgFloat, clipped, p2, 255, cv::THRESH_TOZERO);
    cv::threshold(clipped, clipped, p98, p98, cv::THRESH_TRUNC);

    clipped = (clipped - p2) * (255.0 / (p98 - p2));
    clipped.convertTo(clipped, CV_8U);

    // Convert grayscale to BGR
    cv::Mat imgBGR;
    cv::cvtColor(clipped, imgBGR, cv::COLOR_GRAY2BGR);

    // Resize to model input size (assuming 640x640, change if needed)
    cv::Mat blob;
    cv::resize(imgBGR, blob, cv::Size(640, 640));

    // Convert to float and normalize [0,1]
    blob.convertTo(blob, CV_32F, 1.0 / 255);

    return blob;
}

QList<cv::Rect> CustomObjectDetectionWindow::postprocess(const cv::Mat &image, const cv::Mat &outputs, float confThreshold)
{
    QList<cv::Rect> boxes;
    std::vector<float> confidences;

    // Assuming output shape: Nx85 (x, y, w, h, conf, 80 class scores)
    // You may need to adjust this depending on your model output format

    const int dimensions = outputs.cols;  // 85 for COCO; adjust for your model
    const int rows = outputs.rows;

    for (int i = 0; i < rows; ++i) {
        const float *data = outputs.ptr<float>(i);
        float confidence = data[4];
        if (confidence >= confThreshold) {
            // Find class with max score
            float maxClassScore = 0;
            int classId = -1;
            for (int c = 5; c < dimensions; ++c) {
                if (data[c] > maxClassScore) {
                    maxClassScore = data[c];
                    classId = c - 5;
                }
            }
            float finalScore = confidence * maxClassScore;
            if (finalScore >= confThreshold) {
                // Convert from center x,y,w,h to rect
                int cx = int(data[0] * image.cols);
                int cy = int(data[1] * image.rows);
                int w = int(data[2] * image.cols);
                int h = int(data[3] * image.rows);
                int left = cx - w / 2;
                int top = cy - h / 2;

                boxes.append(cv::Rect(left, top, w, h));
                confidences.push_back(finalScore);
            }
        }
    }

    // Non-max suppression to remove overlapping boxes
    std::vector<int> indices;
    std::vector<cv::Rect> rects(boxes.begin(), boxes.end());
    cv::dnn::NMSBoxes(rects, confidences, confThreshold, 0.4f, indices);

    QList<cv::Rect> filteredBoxes;
    for (int idx : indices) {
        filteredBoxes.append(rects[idx]);
    }
    return filteredBoxes;
}

void CustomObjectDetectionWindow::drawBoxes(cv::Mat &image, const QList<cv::Rect> &boxes, const std::vector<float> &confidences)
{
    for (int i = 0; i < boxes.size(); ++i) {
        cv::rectangle(image, boxes[i], cv::Scalar(0, 255, 0), 2);
        if (i < confidences.size()) {
            std::string label = cv::format("%.2f", confidences[i]);
            int baseLine = 0;
            cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
            int top = std::max(boxes[i].y, labelSize.height);
            cv::putText(image, label, cv::Point(boxes[i].x, top - 4),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        }
    }
}

void CustomObjectDetectionWindow::runDetection()
{
    if (net.empty()) {
        QMessageBox::warning(this, "Warning", "Model is not loaded.");
        return;
    }

    statusLabel->setText("Running detection... Please wait.");
    QApplication::processEvents();

    detectedImages.clear();

    float confThreshold = 0.6f;

    for (int i = 0; i < originalImages.size(); ++i) {
        cv::Mat preprocessed = preprocess(originalImages[i]);

        // Create blob from preprocessed image (already normalized and resized)
        cv::Mat blob = cv::dnn::blobFromImage(preprocessed);

        net.setInput(blob);

        cv::Mat outputs = net.forward();

        // outputs shape depends on your model, may need reshaping
        // for YOLOv5/YOLOv7-like, outputs shape is usually Nx85
        cv::Mat reshapedOutputs = outputs.reshape(1, outputs.total() / 85);

        QList<cv::Rect> boxes = postprocess(originalImages[i], reshapedOutputs, confThreshold);

        // Draw boxes on a copy of original image (convert grayscale to BGR for drawing)
        cv::Mat imgColor;
        cv::cvtColor(originalImages[i], imgColor, cv::COLOR_GRAY2BGR);

        // NOTE: We don't have confidences vector separately here — you'd need to modify postprocess to output confidences if you want to draw labels

        for (const auto &box : boxes) {
            cv::rectangle(imgColor, box, cv::Scalar(0, 255, 0), 2);
        }

        detectedImages.append(imgColor);
    }

    displayImages(detectedImages);

    statusLabel->setText(QString("Detection completed on %1 images.").arg(detectedImages.size()));
    emit detectionCompleted(detectedImages);
}

void CustomObjectDetectionWindow::displayImages(const QList<cv::Mat> &images)
{
    // Clear previous
    QLayoutItem *child;
    while ((child = imageLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    for (const auto &img : images) {
        cv::Mat rgb;
        cv::cvtColor(img, rgb, cv::COLOR_BGR2RGB);
        QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        QLabel *label = new QLabel;
        label->setPixmap(QPixmap::fromImage(qimg).scaled(320, 320, Qt::KeepAspectRatio));
        imageLayout->addWidget(label);
    }
    imageContainer->setLayout(imageLayout);
}

void CustomObjectDetectionWindow::closeWindow()
{
    this->close();
}
