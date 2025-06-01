#ifndef EDITWINDOW_H
#define EDITWINDOW_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QStack>
#include <opencv2/opencv.hpp>

#include "segmentationwindow.h"

#include <QDialog>

class EditWindow : public QDialog  // Not QWidget
{
    Q_OBJECT

public:
    explicit EditWindow(const QList<cv::Mat> &images, QWidget *parent = nullptr);


signals:
    void imagesEdited(QList<cv::Mat> newImages);

private slots:
    void applyFilter();
    void undoLast();

private:
    QLabel *preview;
    QComboBox *filterCombo;
    QPushButton *applyButton;
    QPushButton *undoButton;
    QPushButton *segmentationButton;

    QList<cv::Mat> imageSlices;
    QStack<QList<cv::Mat>> undoStack;

    QImage matToQImage(const cv::Mat &mat);


};

#endif // EDITWINDOW_H
