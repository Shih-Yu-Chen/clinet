#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTcpServer>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QColor>
#include <QPoint>
#include <QImage>
#include <QVector>
#include <QInputDialog>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QDataStream>
#include <QResizeEvent>


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(bool isServer = false, QWidget *parent = nullptr);
    ~MainWindow();
    void resizeEvent(QResizeEvent *event) override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void connectToServer();
    void handleReadyRead();
    void handleDisconnected();
    void startServer();
    void handleNewConnection();
    void setPenColor();
    void clearCanvas();
    void saveImage();
    void loadImage();
    void toggleEraser();

private:
    void createUI();
    void setupNetwork();
    void sendDrawingData(const QPoint &point, bool isDrawing);

    void sendImageData(const QImage &image); // Sync image functionality

    void updateCanvasSize(); // Update canvas size

    void scaleCanvasToFit(); // Scale the canvas to fit the window size
    void sendLoadedImage();

    QPoint mapToCanvas(const QPoint &windowPoint); // Map window coordinates to canvas coordinates
    QTcpServer *server;
    QTcpSocket *clientSocket;
    QVector<QTcpSocket*> clients;
    bool isServer;
    QImage canvas;
    bool isDrawing;
    QPoint lastPoint;
    QColor currentColor;
    QColor previousColor;
    int penWidth;
    bool isEraser;

    QByteArray buffer;  // 用於存儲未完成的數據

    QByteArray receivedImageData;  // 用於存儲接收的圖片數據
    int expectedImageSize;         // 期望接收的圖片大小
    static const int CHUNK_SIZE = 32768;  // 32KB 的分塊大小
};

#endif // MAINWINDOW_H


