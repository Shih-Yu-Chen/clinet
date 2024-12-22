// mainwindow.h
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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(bool isServer = false, QWidget *parent = nullptr);
    ~MainWindow();

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
};

#endif // MAINWINDOW_H
