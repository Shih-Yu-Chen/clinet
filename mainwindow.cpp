#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSlider>
#include <QLabel>
#include <QToolBar>
#include <QBuffer>
#include <QTimer>
#include <QDebug>

MainWindow::MainWindow(bool isServer, QWidget *parent)
    : QMainWindow(parent)
    , server(nullptr)
    , clientSocket(nullptr)
    , isServer(isServer)
    , isDrawing(false)
    , currentColor(Qt::black)
    , previousColor(Qt::black)
    , penWidth(2)
    , isEraser(false)
{
    canvas = QImage(1200, 800, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    createUI();
    setupNetwork();
}

void MainWindow::createUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QToolBar *toolBar = new QToolBar(this);
    addToolBar(Qt::TopToolBarArea, toolBar);
    toolBar->setMovable(false);

    QPushButton *colorBtn = new QPushButton("顏色", this);
    QPushButton *eraserBtn = new QPushButton("橡皮擦", this);
    QPushButton *clearBtn = new QPushButton("清除", this);
    QPushButton *saveBtn = new QPushButton("保存", this);
    QPushButton *loadBtn = new QPushButton("載入", this);

    QWidget *sliderWidget = new QWidget(this);
    QHBoxLayout *sliderLayout = new QHBoxLayout(sliderWidget);
    QLabel *sliderLabel = new QLabel("筆寬:", this);
    QSlider *penWidthSlider = new QSlider(Qt::Horizontal, this);
    QLabel *widthValueLabel = new QLabel(QString::number(penWidth), this);

    penWidthSlider->setRange(1, 50);
    penWidthSlider->setValue(penWidth);
    penWidthSlider->setFixedWidth(150);

    sliderLayout->addWidget(sliderLabel);
    sliderLayout->addWidget(penWidthSlider);
    sliderLayout->addWidget(widthValueLabel);
    sliderLayout->setContentsMargins(5, 0, 5, 0);

    toolBar->addWidget(colorBtn);
    toolBar->addWidget(eraserBtn);
    toolBar->addWidget(sliderWidget);
    toolBar->addWidget(clearBtn);
    toolBar->addWidget(saveBtn);
    toolBar->addWidget(loadBtn);

    connect(colorBtn, &QPushButton::clicked, this, &MainWindow::setPenColor);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearCanvas);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveImage);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::loadImage);
    connect(eraserBtn, &QPushButton::clicked, this, &MainWindow::toggleEraser);

    connect(penWidthSlider, &QSlider::valueChanged, this, [this, widthValueLabel](int value) {
        penWidth = value;
        widthValueLabel->setText(QString::number(value));
    });

    eraserBtn->setCheckable(true);
    setMinimumSize(1250, 850);
}

void MainWindow::setupNetwork()
{
    if (isServer) {
        server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, &MainWindow::handleNewConnection);
        startServer();
    } else {
        clientSocket = new QTcpSocket(this);
        connect(clientSocket, &QTcpSocket::readyRead, this, &MainWindow::handleReadyRead);
        connect(clientSocket, &QTcpSocket::disconnected, this, &MainWindow::handleDisconnected);
        connectToServer();
    }
}

void MainWindow::startServer()
{
    bool ok;
    QString serverIP = QInputDialog::getText(this, "Server Setup",
                                             "Enter IP address:", QLineEdit::Normal,
                                             "127.0.0.1", &ok);
    if (ok && !serverIP.isEmpty()) {
        int port = QInputDialog::getInt(this, "Server Setup",
                                        "Enter port:", 1234, 1, 65535, 1, &ok);
        if (ok) {
            if (server->listen(QHostAddress(serverIP), port)) {
                QMessageBox::information(this, "Server", "Server started successfully!");
            } else {
                QMessageBox::critical(this, "Error", "Failed to start server!");
            }
        }
    }
}

void MainWindow::connectToServer()
{
    bool ok;
    QString ip = QInputDialog::getText(this, "Connect to Server",
                                       "Enter server IP:", QLineEdit::Normal,
                                       "127.0.0.1", &ok);
    if (ok && !ip.isEmpty()) {
        int port = QInputDialog::getInt(this, "Connect to Server",
                                        "Enter server port:", 1234, 1, 65535, 1, &ok);
        if (ok) {
            clientSocket->connectToHost(ip, port);
            if (clientSocket->waitForConnected()) {
                QMessageBox::information(this, "Connection", "Connected to server!");
            } else {
                QMessageBox::critical(this, "Error", "Failed to connect to server!");
            }
        }
    }
}

void MainWindow::sendDrawingData(const QPoint &point, bool isDrawing)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    stream << QString("DRAW") << point << isDrawing << currentColor << penWidth;

    if (isServer) {
        serverLastPoint = point;
        for (QTcpSocket *client : clients) {
            client->write(data);
            client->flush();
        }
    } else if (clientSocket) {
        clientLastPoint = point;
        clientSocket->write(data);
        clientSocket->flush();
    }
}

void MainWindow::sendLoadedImage()
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    canvas.save(&buffer, "PNG");

    stream << QString("LOAD_IMAGE") << imageData;

    if (isServer) {
        for (QTcpSocket *client : clients) {
            client->write(data);
            client->flush();
        }
    } else if (clientSocket) {
        clientSocket->write(data);
        clientSocket->flush();
    }
}

void MainWindow::handleReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    buffer.append(socket->readAll());

    while (!buffer.isEmpty()) {
        QDataStream stream(buffer);
        stream.setVersion(QDataStream::Qt_5_15);

        if (buffer.size() < static_cast<int>(sizeof(quint32))) {
            break;
        }

        stream.startTransaction();

        QString command;
        stream >> command;
        qDebug() << "Attempting to read command:" << command;
        qDebug() << "Current buffer size:" << buffer.size();

        if (command == "LOAD_IMAGE") {
            QByteArray imageData;
            stream >> imageData;

            if (!stream.commitTransaction()) {
                qDebug() << "Incomplete data, waiting for more...";
                break;
            }

            qDebug() << "Received complete image data size:" << imageData.size();

            QImage loadedImage;
            if (loadedImage.loadFromData(imageData, "PNG")) {
                canvas = loadedImage;
                update();
                qDebug() << "Image loaded successfully";
            } else {
                qDebug() << "Failed to load image from data";
            }

            buffer.remove(0, stream.device()->pos());
        }
        else if (command == "CLEAR") {
            if (!stream.commitTransaction()) {
                break;
            }
            canvas.fill(Qt::white);
            update();
            buffer.remove(0, stream.device()->pos());
        }
        else if (command == "DRAW") {
            QPoint point;
            bool drawing;
            QColor color;
            int width;
            stream >> point >> drawing >> color >> width;

            if (!stream.commitTransaction()) {
                break;
            }

            if (drawing) {
                QPainter painter(&canvas);
                painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));

                // 如果是新的筆劃就開始新的點集
                if (isNewStroke) {
                    drawingPoints.clear();
                    isNewStroke = false;
                }

                if (!drawingPoints.isEmpty()) {
                    painter.drawLine(drawingPoints.last(), point);
                }
                drawingPoints.append(point);
                update();
            } else {
                isNewStroke = true;
            }

            buffer.remove(0, stream.device()->pos());
        }
    }
}

void MainWindow::handleDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        clients.removeOne(socket);
        socket->deleteLater();
    }
}

void MainWindow::handleNewConnection()
{
    QTcpSocket *newClient = server->nextPendingConnection();
    clients.append(newClient);
    connect(newClient, &QTcpSocket::readyRead, this, &MainWindow::handleReadyRead);
    connect(newClient, &QTcpSocket::disconnected, this, &MainWindow::handleDisconnected);
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QRect targetRect = this->rect();
    QRect sourceRect(0, 0, canvas.width(), canvas.height());
    painter.drawImage(targetRect, canvas, sourceRect);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    scaleCanvasToFit();
    update();
}

QPoint MainWindow::mapToCanvas(const QPoint &windowPoint)
{
    int x = windowPoint.x() * static_cast<double>(canvas.width()) / width();
    int y = windowPoint.y() * static_cast<double>(canvas.height()) / height();
    return QPoint(x, y);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDrawing = true;
        isNewStroke = true;
        drawingPoints.clear();
        QPoint point = mapToCanvas(event->pos());
        drawingPoints.append(point);
        sendDrawingData(point, true);
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (isDrawing) {
        QPoint point = mapToCanvas(event->pos());
        drawingPoints.append(point);

        QPainter painter(&canvas);
        painter.setPen(QPen(currentColor, penWidth, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(drawingPoints[drawingPoints.size()-2], point);
        update();

        sendDrawingData(point, true);
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *)
{
    if (isDrawing) {
        isDrawing = false;
        isNewStroke = true;
        sendDrawingData(drawingPoints.last(), false);
    }
}

void MainWindow::toggleEraser()
{
    isEraser = !isEraser;
    if (isEraser) {
        previousColor = currentColor;
        currentColor = Qt::white;
    } else {
        currentColor = previousColor;
    }
}

void MainWindow::setPenColor()
{
    QColor color = QColorDialog::getColor(currentColor, this);
    if (color.isValid()) {
        if (!isEraser) {
            currentColor = color;
        } else {
            previousColor = color;
            currentColor = Qt::white;
        }
    }
}

void MainWindow::clearCanvas()
{
    canvas.fill(Qt::white);
    update();

    // 準備清除命令數據
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    stream << QString("CLEAR");

    // 發送到所有連接的客戶端
    if (isServer) {
        for (QTcpSocket *client : clients) {
            client->write(data);
            client->flush();
        }
    } else if (clientSocket) {
        clientSocket->write(data);
        clientSocket->flush();
    }
}

void MainWindow::saveImage()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Image", "", "Images (*.png *.jpg *.bmp)");
    if (!fileName.isEmpty()) {
        canvas.save(fileName);
    }
}

void MainWindow::loadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Load Image", "", "Image files (*.png *.jpg *.bmp)");
    if (!fileName.isEmpty()) {
        QImage loadedImage;
        if (loadedImage.load(fileName)) {
            // 獲取畫布尺寸
            QSize canvasSize = canvas.size();
            QImage scaledImage;

            // 檢查圖片是否需要縮放
            if (loadedImage.width() > canvasSize.width() || loadedImage.height() > canvasSize.height()) {
                scaledImage = loadedImage.scaled(canvasSize,
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation);
            } else {
                scaledImage = loadedImage;
            }

            // 創建新的白色背景畫布
            QImage newCanvas(canvasSize, QImage::Format_RGB32);
            newCanvas.fill(Qt::white);

            // 計算居中位置
            int x = (canvasSize.width() - scaledImage.width()) / 2;
            int y = (canvasSize.height() - scaledImage.height()) / 2;

            // 在新畫布上繪製圖片
            QPainter painter(&newCanvas);
            painter.drawImage(x, y, scaledImage);

            // 更新畫布
            canvas = newCanvas;
            update();

            // 將圖片數據轉換為字節數組
            QByteArray imageData;
            QBuffer buffer(&imageData);
            buffer.open(QIODevice::WriteOnly);
            canvas.save(&buffer, "PNG");

            // 準備發送數據
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            stream.setVersion(QDataStream::Qt_5_15);

            // 寫入命令和圖片數據
            stream << QString("LOAD_IMAGE");
            stream << imageData;

            qDebug() << "Sending total data size:" << data.size();

            // 發送到所有連接的客戶端
            if (isServer) {
                for (QTcpSocket *client : clients) {
                    qint64 written = client->write(data);
                    client->flush();
                    qDebug() << "Written bytes:" << written;
                }
            } else if (clientSocket) {
                qint64 written = clientSocket->write(data);
                clientSocket->flush();
                qDebug() << "Written bytes:" << written;
            }
        }
    }
}

void MainWindow::updateCanvasSize()
{
    scaleCanvasToFit();
}

void MainWindow::scaleCanvasToFit()
{
    canvas = canvas.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

MainWindow::~MainWindow()
{
    // 清理網絡連接
    if (server) {
        server->close();
        delete server;
    }

    if (clientSocket) {
        clientSocket->disconnect();
        clientSocket->close();
        delete clientSocket;
    }

    // 清理所有客戶端連接
    for (QTcpSocket* client : clients) {
        client->disconnect();
        client->close();
        delete client;
    }
    clients.clear();
}
