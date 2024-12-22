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
    // 增加畫布大小
    canvas = QImage(1200, 800, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    createUI();
    setupNetwork();
}

void MainWindow::createUI()
{
    // 建立中央widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 建立主要垂直布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0); // 移除邊距

    // 建立工具列
    QToolBar *toolBar = new QToolBar(this);
    addToolBar(Qt::TopToolBarArea, toolBar);
    toolBar->setMovable(false); // 固定工具列

    // 建立工具列按鈕和控制項
    QPushButton *colorBtn = new QPushButton("顏色", this);
    QPushButton *eraserBtn = new QPushButton("橡皮擦", this);
    QPushButton *clearBtn = new QPushButton("清除", this);
    QPushButton *saveBtn = new QPushButton("保存", this);
    QPushButton *loadBtn = new QPushButton("載入", this);

    // 建立筆寬滑動條和標籤
    QWidget *sliderWidget = new QWidget(this);
    QHBoxLayout *sliderLayout = new QHBoxLayout(sliderWidget);
    QLabel *sliderLabel = new QLabel("筆寬:", this);
    QSlider *penWidthSlider = new QSlider(Qt::Horizontal, this);
    QLabel *widthValueLabel = new QLabel(QString::number(penWidth), this);

    // 設定滑動條
    penWidthSlider->setRange(1, 50);
    penWidthSlider->setValue(penWidth);
    penWidthSlider->setFixedWidth(150);

    // 將控制項加入滑動條布局
    sliderLayout->addWidget(sliderLabel);
    sliderLayout->addWidget(penWidthSlider);
    sliderLayout->addWidget(widthValueLabel);
    sliderLayout->setContentsMargins(5, 0, 5, 0);

    // 添加工具到工具列
    toolBar->addWidget(colorBtn);
    toolBar->addWidget(eraserBtn);
    toolBar->addWidget(sliderWidget);
    toolBar->addWidget(clearBtn);
    toolBar->addWidget(saveBtn);
    toolBar->addWidget(loadBtn);

    // 連接信號
    connect(colorBtn, &QPushButton::clicked, this, &MainWindow::setPenColor);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearCanvas);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveImage);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::loadImage);
    connect(eraserBtn, &QPushButton::clicked, this, &MainWindow::toggleEraser);

    // 連接滑動條信號
    connect(penWidthSlider, &QSlider::valueChanged, this, [this, widthValueLabel](int value) {
        penWidth = value;
        widthValueLabel->setText(QString::number(value));
    });

    // 設定橡皮擦按鈕可切換
    eraserBtn->setCheckable(true);

    // 設定視窗最小大小
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

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawImage(0, 0, canvas);
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

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDrawing = true;
        lastPoint = event->pos();

        if (isEraser) {
            QPainter painter(&canvas);
            painter.setPen(QPen(Qt::white, penWidth * 2, Qt::SolidLine, Qt::RoundCap));
            painter.drawPoint(lastPoint);
            update();
        }

        sendDrawingData(lastPoint, true);
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (isDrawing) {
        QPainter painter(&canvas);

        if (isEraser) {
            painter.setPen(QPen(Qt::white, penWidth * 2, Qt::SolidLine, Qt::RoundCap));
        } else {
            painter.setPen(QPen(currentColor, penWidth, Qt::SolidLine, Qt::RoundCap));
        }

        painter.drawLine(lastPoint, event->pos());
        lastPoint = event->pos();
        update();

        sendDrawingData(lastPoint, true);
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *)
{
    isDrawing = false;
    sendDrawingData(lastPoint, false);
}

void MainWindow::sendDrawingData(const QPoint &point, bool isDrawing)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << point << isDrawing << currentColor << penWidth;

    if (isServer) {
        for (QTcpSocket *client : clients) {
            client->write(data);
        }
    } else if (clientSocket) {
        clientSocket->write(data);
    }
}

void MainWindow::handleReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QDataStream stream(data);
    QPoint point;
    bool drawing;
    QColor color;
    int width;

    stream >> point >> drawing >> color >> width;

    if (drawing) {
        QPainter painter(&canvas);
        painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(lastPoint, point);
        lastPoint = point;
        update();
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

void MainWindow::setPenColor()
{
    QColor color = QColorDialog::getColor(currentColor, this);
    if (color.isValid()) {
        if (!isEraser) {
            currentColor = color;
            previousColor = color;
        }
    }
}

void MainWindow::clearCanvas()
{
    canvas.fill(Qt::white);
    update();
}

void MainWindow::saveImage()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Image",
                                                    "", "PNG files (*.png)");
    if (!fileName.isEmpty()) {
        canvas.save(fileName, "PNG");
    }
}

void MainWindow::loadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Load Image",
                                                    "", "Image files (*.png *.jpg *.bmp)");
    if (!fileName.isEmpty()) {
        canvas.load(fileName);
        update();
    }
}

MainWindow::~MainWindow()
{
}
