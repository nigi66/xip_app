#include "objectdetectionwindow.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>

#include <fstream>

#include <opencv2/dnn.hpp>

ObjectDetectionWindow::ObjectDetectionWindow(const QList<cv::Mat> &images, QWidget *parent)
    : QDialog(parent), inputImages(images) {
    QVBoxLayout *layout = new QVBoxLayout(this);

    setWindowTitle("Object Detection");
    setMinimumSize(400, 200);  // Add this

    imageLabel = new QLabel("Click Detect to run YOLO object detection.");
    detectButton = new QPushButton("Run Detection");

    layout->addWidget(imageLabel);
    layout->addWidget(detectButton);

    connect(detectButton, &QPushButton::clicked, this, &ObjectDetectionWindow::runDetection);
    loadYoloModel();
}

void ObjectDetectionWindow::loadYoloModel() {
    // Replace with your YOLO paths
    std::string modelPath = "C:/Users/Me/Documents/xip_app/yolov3.weights";
    std::string configPath = "C:/Users/Me/Documents/xip_app/yolov3.cfg";
    yoloNet = cv::dnn::readNetFromDarknet(configPath, modelPath);
    yoloNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    yoloNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
}

cv::Mat ObjectDetectionWindow::detectObjects(const cv::Mat &image) {

    cv::Mat colorImage;
    if (image.channels() == 1) {
        cv::cvtColor(image, colorImage, cv::COLOR_GRAY2BGR);
    } else {
        colorImage = image;
    }

    cv::Mat blob;
    cv::dnn::blobFromImage(colorImage, blob,  1.0 / 255.0, cv::Size(416, 416), cv::Scalar(), true, false);
    yoloNet.setInput(blob);

    std::vector<cv::Mat> outputs;
    yoloNet.forward(outputs, yoloNet.getUnconnectedOutLayersNames());

    // Load class names (optional)
    std::vector<std::string> classNames;
    std::ifstream ifs("C:/Users/Me/Documents/xip_app/coco.names");  // Place coco.names in working dir
    std::string line;
    while (std::getline(ifs, line)) classNames.push_back(line);

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    float confThreshold = 0.5;
    float nmsThreshold = 0.4;

    for (auto &output : outputs) {
        for (int i = 0; i < output.rows; ++i) {
            float *data = output.ptr<float>(i);
            float confidence = data[4];

            if (confidence > confThreshold) {
                cv::Mat scores = output.row(i).colRange(5, output.cols);
                cv::Point classIdPoint;
                double maxClassScore;
                minMaxLoc(scores, nullptr, &maxClassScore, nullptr, &classIdPoint);

                if (maxClassScore > confThreshold) {
                    int centerX = static_cast<int>(data[0] * image.cols);
                    int centerY = static_cast<int>(data[1] * image.rows);
                    int width = static_cast<int>(data[2] * image.cols);
                    int height = static_cast<int>(data[3] * image.rows);
                    int left = centerX - width / 2;
                    int top = centerY - height / 2;

                    boxes.emplace_back(left, top, width, height);
                    classIds.push_back(classIdPoint.x);
                    confidences.push_back(static_cast<float>(maxClassScore));
                }
            }
        }
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

    cv::Mat result = image.clone();
    for (int idx : indices) {
        cv::Rect box = boxes[idx];
        int classId = classIds[idx];
        std::string label = classNames.empty() ? std::to_string(classId) : classNames[classId];
        cv::rectangle(result, box, cv::Scalar(0, 255, 0), 2);
        cv::putText(result, label, cv::Point(box.x, box.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    }

    return result;
}


void ObjectDetectionWindow::runDetection() {
    outputImages.clear();

    for (const auto &img : inputImages) {
        if (img.empty())
            continue;
        outputImages.append(detectObjects(img));
    }

    emit detectionCompleted(outputImages);
    QMessageBox::information(this, "Done", "Object detection completed.");
    close();
}
