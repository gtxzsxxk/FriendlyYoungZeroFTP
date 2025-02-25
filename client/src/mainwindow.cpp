#include "../include/mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QThread>
#include <cstring>
#include <sstream>
#include <fstream>
#include <filesystem>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    connect(ui->button_fastconnect, &QPushButton::clicked, this, &MainWindow::fastConnectOrQuit);
    connect(ui->button_exec_cmd, &QPushButton::clicked, this, &MainWindow::txCommandByWidgets);
    connect(ui->button_upload, &QPushButton::clicked, this, &MainWindow::uploadFile);
    connect(ui->button_cdup, &QPushButton::clicked, this, &MainWindow::cdUp);
    connect(ui->input_command, &QLineEdit::returnPressed, this, &MainWindow::txCommandByWidgets);
    connect(ui->table_file, &QTableWidget::cellDoubleClicked, this, &MainWindow::retrieveFileFromTable);
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
    ui->input_directory->setText(QString::fromStdString(std::filesystem::current_path().string()));
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
            netCtrlTx("QUIT\r\n");
            netCtrlRx();
            sockClient.close();
        }
        viewSetUIDisconnected();
    }
}

void MainWindow::txCommandByWidgets() {
    auto cmd = ui->input_command->text().toStdString();
    if (!cmd.empty()) {
        if (cmd.substr(0, 4) == "QUIT") {
            netCtrlTx("QUIT\r\n");
            netCtrlRx();
            if (uiConnectedState) {
                sockClient.close();
            }
            viewSetUIDisconnected();
        } else if (cmd.substr(0, 4) == "PASV") {
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
            execFtpCmdRETR();
        } else if (cmd.substr(0, 4) == "STOR") {
            commandToExec = cmd;
            execFtpCmdSTOR();
        } else if (cmd.substr(0, 3) == "CWD") {
            commandToExec = cmd;
            execFtpCmdCWD();
        } else {
            netCtrlTx(ui->input_command->text().toStdString() + "\r\n");
            netCtrlRx();
        }
        ui->input_command->setText("");
    }
}

void MainWindow::uploadFile() {
    auto fileName = QFileDialog::getOpenFileName(this, "Open File",
                                                 QString::fromStdString(std::filesystem::current_path().string()),
                                                 "All Files(*)");

    if (fileName.isEmpty()) {
        return;
    }

    commandToExec = "PASV";
    execFtpCmdPASV();
    commandToExec = "STOR " + fileName.toStdString();
    execFtpCmdSTOR();
}

void MainWindow::retrieveFileFromTable(int row, int column) {
    if (ui->table_file->item(row, 4)->text()[0] == 'd') {
        commandToExec = "CWD " + ui->table_file->item(row, 0)->text().toStdString();
        execFtpCmdCWD();
    } else {
        commandToExec = "PASV";
        execFtpCmdPASV();
        commandToExec = "RETR " + ui->table_file->item(row, 0)->text().toStdString();
        execFtpCmdRETR();
    }
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
    if (sockClient.isOpen()) {
        str = sockClient.readLine(8192).toStdString();
        if (!str.empty()) {
            ui->textBrowser->insertPlainText(QString::fromStdString(str));
        }
        ui->textBrowser->moveCursor(ui->textBrowser->textCursor().End);
        return str;
    }
    return "";
}

void MainWindow::networkErrorOccurred(QAbstractSocket::SocketError socketError) {
    viewSetUIDisconnected();
    sockClient.close();
    auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "连接服务器失败！");
    msgbox.exec();
}

void MainWindow::networkConnected() {
    auto resp = netCtrlRx();
    while (!resp.empty() && resp[3] != ' ') {
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
    int port = 0, cnt = 0;
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
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "无法解析服务器的 PASV 返回！");
        msgbox.exec();
        return;
    }
    QThread::msleep(100);
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
    int port = 0, cnt = 0;
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
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "无法解析服务器的 PORT 返回！");
        msgbox.exec();
        return;
    }
    serverPort.listen(QHostAddress::Any, port);
    connect(&serverPort, &QTcpServer::newConnection, [&]() {
        auto tmp = serverPort.nextPendingConnection();
        if (tmp) {
            sockData = tmp;
        }
    });
    netCtrlTx(commandToExec + "\r\n");
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

    if (transferMode == PORT) {
        while (sockData == nullptr) {
            QThread::msleep(100);
        }
    }

    resp = netCtrlRx();
    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "LIST状态错误2！");
        msgbox.exec();
        return;
    }

    std::string listDataStd;
    auto *dataRecv = new char[65536];
    while (true) {
        if (sockData->bytesAvailable() == 0) {
            sockData->waitForReadyRead();
        }
        auto ret = sockData->read(dataRecv, 65536);
        if (ret == 0) {
            break;
        } else if (ret < 0) {
            /* 错误 */
            break;
        }
        listDataStd += std::string{dataRecv, static_cast<std::string::size_type>(ret)};
    }
    auto *listData = strdup(listDataStd.c_str());

    char *line;
    int row = 0;
    for (line = strtok(listData, "\r\n"); line; line = strtok(nullptr, "\r\n")) {
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
            t->setFlags(t->flags() & (~Qt::ItemIsEditable));
            ui->table_file->setItem(row, i, t);
        }
        row++;
    }
    delete[] dataRecv;
    free(listData);
    sockData->close();
    if (transferMode == PORT) {
        serverPort.close();
    }
    transferMode = NOT_SPECIFIED;
}

FTP_DEFINE_COMMAND(RETR) {
    if (transferMode == NOT_SPECIFIED) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "执行 RETR 之前你必须设置 PASV 或 PORT！");
        msgbox.exec();
        return;
    }

    std::string filename;
    if (commandToExec.length() <= 5) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "执行 RETR 的指令格式不对！");
        msgbox.exec();
        return;
    }
    filename = commandToExec.substr(5);
    netCtrlTx("RETR " + filename + "\r\n");
    auto resp = netCtrlRx();
    if (resp[0] != '1') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "RETR状态错误1！");
        msgbox.exec();
        return;
    }

    if (transferMode == PORT) {
        while (sockData == nullptr) {
            QThread::msleep(100);
        }
    }

    resp = netCtrlRx();
    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "RETR状态错误2！");
        msgbox.exec();
        return;
    }

    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        sockData->close();
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "无法写入文件！");
        msgbox.exec();
        return;
    }

    auto *dataRecv = new char[65536];
    while (true) {
        if (sockData->bytesAvailable() == 0) {
            sockData->waitForReadyRead();
        }
        auto ret = sockData->read(dataRecv, 65536);
        if (ret == 0) {
            break;
        } else if (ret < 0) {
            /* 错误 */
            break;
        }
        outFile.write(dataRecv, ret);
    }
    delete[] dataRecv;
    outFile.close();
    sockData->close();
    if (transferMode == PORT) {
        serverPort.close();
    }
    transferMode = NOT_SPECIFIED;
}

FTP_DEFINE_COMMAND(STOR) {
    if (transferMode == NOT_SPECIFIED) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "执行 STOR 之前你必须设置 PASV 或 PORT！");
        msgbox.exec();
        return;
    }

    std::string filename;
    if (commandToExec.length() <= 5) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "执行 STOR 的指令格式不对！");
        msgbox.exec();
        return;
    }
    filename = commandToExec.substr(5);
    netCtrlTx("STOR " + filename + "\r\n");
    auto resp = netCtrlRx();
    if (resp[0] != '1') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "STOR状态错误1！");
        msgbox.exec();
        return;
    }

    if (transferMode == PORT) {
        while (sockData == nullptr) {
            QThread::msleep(100);
        }
    }

    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        sockData->close();
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "无法打开文件！");
        msgbox.exec();
        return;
    }

    char buffer[4096];
    while (inFile.read(buffer, sizeof(buffer))) {
        auto bytesSent = sockData->write(buffer, inFile.gcount());
        if (bytesSent < 0) {
            auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "无法发送文件！");
            msgbox.exec();
            break;
        }
    }
    if (inFile.gcount() > 0) {
        auto bytesSent = sockData->write(buffer, inFile.gcount());
        if (bytesSent < 0) {
            auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "无法发送文件！");
            msgbox.exec();
        }
    }
    sockData->waitForBytesWritten();
    sockData->flush();
    inFile.close();
    sockData->close();

    resp = netCtrlRx();
    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "STOR状态错误2！");
        msgbox.exec();
    }
    if (transferMode == PORT) {
        serverPort.close();
    }
    transferMode = NOT_SPECIFIED;
}

FTP_DEFINE_COMMAND(CWD) {
    std::string dirName;
    if (commandToExec.length() <= 5) {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "执行 CWD 的指令格式不对！");
        msgbox.exec();
        return;
    }
    dirName = commandToExec.substr(4);
    netCtrlTx("CWD " + dirName + "\r\n");
    auto resp = netCtrlRx();
    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "CWD 状态错误！");
        msgbox.exec();
        return;
    }

    commandToExec = "PASV";
    execFtpCmdPASV();
    commandToExec = "LIST";
    execFtpCmdLIST();
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
    ui->textBrowser->clear();
    while (ui->table_file->rowCount()) {
        ui->table_file->removeRow(0);
    }
    uiConnectedState = false;
}

void MainWindow::cdUp() {
    netCtrlTx("CDUP\r\n");
    auto resp = netCtrlRx();
    if (resp[0] != '2') {
        auto msgbox = QMessageBox(QMessageBox::Warning, "错误", "CDUP 状态错误！");
        msgbox.exec();
        return;
    }

    commandToExec = "PASV";
    execFtpCmdPASV();
    commandToExec = "LIST";
    execFtpCmdLIST();
}
