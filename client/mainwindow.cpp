#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    connect(ui->button_fastconnect, &QPushButton::clicked, this, &MainWindow::fastConnectOrQuit);
    connect(ui->button_exec_cmd, &QPushButton::clicked, this, &MainWindow::txCommandByWidgets);
    connect(ui->button_upload, &QPushButton::clicked, this, &MainWindow::uploadFile);
    connect(ui->input_command, &QLineEdit::returnPressed, this, &MainWindow::txCommandByWidgets);
    connect(ui->table_file, &QTableWidget::cellDoubleClicked, this, &MainWindow::retrieveFile);

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
    if(ui->button_fastconnect->text() == "快速连接") {
        auto host = ui->input_host->text();
        auto port = ui->input_port->text();
        auto username = ui->input_username->text();
        auto password = ui->input_password->text();

        ui->button_fastconnect->setText("断开连接");
    } else {
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

}

const std::string &&MainWindow::netCtrlRx() {
    return {};
}

