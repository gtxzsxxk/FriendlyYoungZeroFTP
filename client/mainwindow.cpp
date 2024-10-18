#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    connect(ui->button_fastconnect, &QPushButton::clicked, this, &MainWindow::fastConnectOrQuit);
    connect(ui->button_exec_cmd, &QPushButton::clicked, this, &MainWindow::txCommandByWidgets);
    connect(ui->button_upload, &QPushButton::clicked, this, &MainWindow::uploadFile);
    connect(ui->input_command, &QLineEdit::returnPressed, this, &MainWindow::txCommandByWidgets);
    connect(ui->table_file, &QTableWidget::cellDoubleClicked, this, &MainWindow::retrieveFile);
    connect(&sockClient, &QTcpSocket::errorOccurred, this, &MainWindow::networkErrorOccurred);
    connect(&sockClient, &QTcpSocket::connected, this, &MainWindow::networkConnected);

    ui->button_exec_cmd->setEnabled(false);
    ui->button_upload->setEnabled(false);
    ui->input_password->setEchoMode(QLineEdit::Password);
    ui->progressBar->hide();
    ui->table_file->setColumnCount(5);
    ui->table_file->setHorizontalHeaderLabels({"文件名", "修改时间", "大小", "所有者/组", "状态"});
    ui->table_file->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->input_command->setFocusPolicy(Qt::NoFocus);
    ui->input_directory->setFocusPolicy(Qt::NoFocus);


}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::fastConnectOrQuit() {
    if (ui->button_fastconnect->text() == "快速连接") {
        auto host = ui->input_host->text();
        auto port = ui->input_port->text();
        auto username = ui->input_username->text();
        auto password = ui->input_password->text();
        if (host.isEmpty()) {
            auto msgbox = QMessageBox(QMessageBox::Warning, "警告", "请输入主机 IP 地址！");
            msgbox.exec();
            return;
        }
        if (port.isEmpty()) {
            port = "21";
        }
        if (username.isEmpty()) {
            username = "anonymous";
        }
        if (password.isEmpty()) {
            username = "anonymous@example.com";
        }
        sockClient.connectToHost(host, port.toInt());
    } else {
        if (uiConnectedState) {
            sockClient.close();
        }
        ui->button_fastconnect->setText("快速连接");
    }
}

void MainWindow::txCommandByWidgets() {

}

void MainWindow::uploadFile() {

}

void MainWindow::retrieveFile(int row, int column) {

}

void MainWindow::netCtrlTx(const std::string &data) {
    ui->textBrowser->insertPlainText(("Client -> " + data).c_str());
    sockClient.write(data.c_str());
}

std::string MainWindow::netCtrlRx() {
    ui->textBrowser->moveCursor(ui->textBrowser->textCursor().End);
    char buffer[1024];
    auto ret = sockClient.read(buffer, 1024);
    return {buffer};
}

void MainWindow::networkErrorOccurred(QAbstractSocket::SocketError socketError) {
    /* TODO: 恢复到未连接状态 */
    auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "未能成功连接服务器！");
    ui->button_fastconnect->setText("快速连接");
    msgbox.exec();
}

void MainWindow::networkConnected() {
    auto msgbox = QMessageBox(QMessageBox::Warning, "喜报", "成功连接服务器！");
    ui->button_fastconnect->setText("断开连接");
    uiConnectedState = true;
    msgbox.exec();
}

