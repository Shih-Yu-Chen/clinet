#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

MainWindow::MainWindow(bool isServer, QWidget *parent)
    : QMainWindow(parent)
    , isServer(isServer)
    , isDrawing(false)
    , currentColor(Qt::black)
    , penWidth(2)
    , isEraser(false)
    , server(nullptr)
    , clientSocket(nullptr)
{
    // Initialize canvas
    canvas = QImage(800, 600, QImage::Format_RGB32);
    canvas.fill(Qt::white);

    createUI();
    setupNetwork();
}

void MainWindow::createUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    QHBoxLayout *toolLayout = new QHBoxLayout();

    // Create buttons
    QPushButton *colorBtn = new QPushButton("顏色", this);
    QPushButton *clearBtn = new QPushButton("清除", this);
    QPushButton *saveBtn = new QPushButton("保存", this);
    QPushButton *loadBtn = new QPushButton("載入", this);

    // Connect signals
    connect(colorBtn, &QPushButton::clicked, this, &MainWindow::setPenColor);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearCanvas);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveImage);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::loadImage);

    // Add widgets to layout
    toolLayout->addWidget(colorBtn);
    toolLayout->addWidget(clearBtn);
    toolLayout->addWidget(saveBtn);
    toolLayout->addWidget(loadBtn);
    toolLayout->addStretch();

    mainLayout->addLayout(toolLayout);
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

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDrawing = true;
        lastPoint = event->pos();
        sendDrawingData(lastPoint, true);
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (isDrawing) {
        QPainter painter(&canvas);
        painter.setPen(QPen(currentColor, penWidth, Qt::SolidLine, Qt::RoundCap));
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
        currentColor = color;
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
