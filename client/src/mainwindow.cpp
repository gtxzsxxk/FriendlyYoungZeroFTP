#include "../include/mainwindow.h"
#include "./ui_mainwindow.h"
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QThread>
#include <cstring>
#include <sstream>

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
    ui->input_directory->setEnabled(false);
    ui->input_password->setEchoMode(QLineEdit::Password);
    ui->progressBar->hide();
    ui->table_file->setColumnCount(5);
    ui->table_file->setHorizontalHeaderLabels({"文件名", "修改时间", "大小", "所有者/组", "属性"});
    ui->table_file->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->input_directory->setFocusPolicy(Qt::NoFocus);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::fastConnectOrQuit() {
    if (ui->button_fastconnect->text() == "快速连接") {
        auto host = ui->input_host->text();
        auto port = ui->input_port->text();
        username = ui->input_username->text().toStdString();
        password = ui->input_password->text().toStdString();
        if (host.isEmpty()) {
            auto msgbox = QMessageBox(QMessageBox::Warning, "警告", "请输入主机 IP 地址！");
            msgbox.exec();
            return;
        }
        if (port.isEmpty()) {
            port = "21";
        }
        if (username.empty()) {
            username = "anonymous";
        }
        if (password.empty()) {
            password = "anonymous@example.com";
        }
        sockClient.connectToHost(host, port.toInt());
    } else {
        if (uiConnectedState) {
            sockClient.close();
        }
        viewSetUIDisconnected();
    }
}

void MainWindow::txCommandByWidgets() {
    auto cmd = ui->input_command->text().toStdString();
    if (!cmd.empty()) {
        if (cmd.substr(0, 4) == "PASV") {
            commandToExec = cmd;
            execFtpCmdPASV();
        } else if (cmd.substr(0, 4) == "PORT") {
            commandToExec = cmd;
            execFtpCmdPORT();
        } else if (cmd.substr(0, 4) == "LIST") {
            commandToExec = cmd;
            execFtpCmdLIST();
        } else if (cmd.substr(0, 4) == "RETR") {
            commandToExec = cmd;
//            execFtpCmdRETR();
        } else if (cmd.substr(0, 4) == "STOR") {
            commandToExec = cmd;
//            execFtpCmdSTOR();
        } else {
            netCtrlTx(ui->input_command->text().toStdString() + "\r\n");
            netCtrlRx();
        }
        ui->input_command->setText("");
    }
}

void MainWindow::uploadFile() {

}

void MainWindow::retrieveFile(int row, int column) {

}

void MainWindow::netCtrlTx(const std::string &data) {
    ui->textBrowser->insertPlainText(("$ " + data).c_str());
    sockClient.write(data.c_str());
    sockClient.waitForBytesWritten();
    sockClient.flush();
}

std::string MainWindow::netCtrlRx() {
    ui->textBrowser->moveCursor(ui->textBrowser->textCursor().End);
    std::string str;
    if (sockClient.bytesAvailable() == 0) {
        sockClient.waitForReadyRead();
    }
    str = sockClient.readLine(8192).toStdString();
    if (!str.empty()) {
        ui->textBrowser->insertPlainText(QString::fromStdString(str));
    }
    ui->textBrowser->moveCursor(ui->textBrowser->textCursor().End);
    return str;
}

void MainWindow::networkErrorOccurred(QAbstractSocket::SocketError socketError) {
    /* TODO: 恢复到未连接状态 */
    auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "未能成功连接服务器！");
    ui->button_fastconnect->setText("快速连接");
    msgbox.exec();
}

void MainWindow::networkConnected() {
    auto resp = netCtrlRx();
    while (resp[3] != ' ') {
        resp = netCtrlRx();
    }
    viewSetUILoggedIn();
    commandToExec = "USER " + username + "\r\n";
    execFtpCmdUSER();
    commandToExec = "PASS " + password + "\r\n";
    execFtpCmdPASS();
    execFtpCmdPASV();
    execFtpCmdLIST();
}

FTP_DEFINE_COMMAND(PASV) {
    netCtrlTx("PASV\r\n");
    auto resp = netCtrlRx();
    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "服务器拒绝了被动模式！");
        msgbox.exec();
        return;
    }
    auto infoStart = resp.find("(") + 1;
    auto infoEnd = resp.find(")");
    auto infoStr = resp.substr(infoStart, infoEnd - infoStart);
    std::string hostAddr;
    int port, cnt = 0;
    auto infoStrDup = strdup(infoStr.c_str());
    char *part;
    for (part = strtok(infoStrDup, ","); part; part = strtok(nullptr, ",")) {
        if (cnt < 4) {
            if (cnt < 3) {
                hostAddr += std::string{part} + ".";
            } else {
                hostAddr += std::string{part};
            }
        } else {
            if (cnt == 4) {
                port = 256 * atoi(part);
            } else if (cnt == 5) {
                port += atoi(part);
            }
        }
        cnt++;
    }
    free(infoStrDup);
    if (cnt != 6) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "无法解析服务器的PASV返回！");
        msgbox.exec();
        return;
    }
    QThread::msleep(200);
    sockPasv.connectToHost(QString::fromStdString(hostAddr), port);
    sockData = &sockPasv;
    sockData->waitForConnected();
    transferMode = PASV;
}

FTP_DEFINE_COMMAND(PORT) {
    char *portCmd = strdup(commandToExec.c_str());
    char *cmdData = strtok(portCmd, " ");
    cmdData = strtok(nullptr, " ");
    std::string hostAddr;
    int port, cnt;
    auto infoStrDup = strdup(cmdData);
    char *part;
    for (part = strtok(infoStrDup, ","); part; part = strtok(nullptr, ",")) {
        if (cnt < 4) {
            if (cnt < 3) {
                hostAddr += std::string{part} + ".";
            } else {
                hostAddr += std::string{part};
            }
        } else {
            if (cnt == 4) {
                port = 256 * atoi(part);
            } else if (cnt == 5) {
                port += atoi(part);
            }
        }
        cnt++;
    }
    free(infoStrDup);
    free(portCmd);
    if (cnt != 6) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "无法解析服务器的PASV返回！");
        msgbox.exec();
        return;
    }
    serverPort.listen(QHostAddress::Any, port);
    connect(&serverPort, &QTcpServer::newConnection, [&]() {
        sockData = serverPort.nextPendingConnection();
    });
    netCtrlTx(commandToExec);
    auto resp = netCtrlRx();
    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "服务器拒绝了主动模式！");
        msgbox.exec();
        return;
    }

    transferMode = PORT;
}

FTP_DEFINE_COMMAND(USER) {
    netCtrlTx(commandToExec);
    auto resp = netCtrlRx();

    if (resp[0] != '3' || resp[1] != '3' || resp[2] != '1') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "服务器拒绝了用户登录！");
        msgbox.exec();
        return;
    }
}

FTP_DEFINE_COMMAND(PASS) {
    netCtrlTx(commandToExec);
    auto resp = netCtrlRx();
    while (resp[3] != ' ') {
        resp = netCtrlRx();
    }

    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "服务器拒绝了用户登录！");
        msgbox.exec();
        return;
    } else {
        ui->button_upload->setEnabled(true);
    }
}

FTP_DEFINE_COMMAND(LIST) {
    while (ui->table_file->rowCount()) {
        ui->table_file->removeRow(0);
    }

    if (transferMode == NOT_SPECIFIED) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "执行 LIST 之前你必须设置 PASV 或 PORT！");
        msgbox.exec();
        return;
    }

    netCtrlTx("LIST\r\n");
    auto resp = netCtrlRx();
    if (resp[0] != '1') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "LIST状态错误1！");
        msgbox.exec();
        return;
    }

    resp = netCtrlRx();
    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "LIST状态错误2！");
        msgbox.exec();
        return;
    }

    sockData->waitForReadyRead();
    auto *dataRecv = new char[8192 * 10];
    auto ret = sockData->read(dataRecv, 8192 * 10);
    if (ret <= 0) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "LIST 数据通道错误！");
        msgbox.exec();
        return;
    }
    dataRecv[ret] = 0;
    char *line;
    int row = 0;
    for (line = strtok(dataRecv, "\r\n"); line; line = strtok(nullptr, "\r\n")) {
        std::stringstream ss{line};
        std::string prop, links, owner, group, size, month, day, tm, filename;
        ss >> prop >> links >> owner >> group >> size >> month >> day >> tm >> filename;
        auto modifyTm = month + " " + day + " " + tm;
        ui->table_file->insertRow(row);
        for (int i = 0; i < 5; i++) {
            QTableWidgetItem *t;
            switch (i) {
                case 0:
                    t = new QTableWidgetItem{QString::fromStdString(filename)};
                    break;
                case 1:
                    t = new QTableWidgetItem{QString::fromStdString(modifyTm)};
                    break;
                case 2:
                    t = new QTableWidgetItem{QString::fromStdString(size)};
                    break;
                case 3:
                    t = new QTableWidgetItem{QString::fromStdString(owner + " / " + group)};
                    break;
                case 4:
                    t = new QTableWidgetItem{QString::fromStdString(prop)};
                    break;
            }
            ui->table_file->setItem(row, i, t);
        }
        row++;
    }
    delete[] dataRecv;
    sockData->close();
}

void MainWindow::viewSetUILoggedIn(void) {
    ui->button_exec_cmd->setEnabled(true);
    ui->button_upload->setEnabled(true);
    ui->button_fastconnect->setText("断开连接");
    uiConnectedState = true;
}

void MainWindow::viewSetUIDisconnected(void) {
    ui->button_exec_cmd->setEnabled(false);
    ui->button_upload->setEnabled(false);
    ui->button_fastconnect->setText("快速连接");
    uiConnectedState = false;
}
