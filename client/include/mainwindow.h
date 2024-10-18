#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTcpServer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define FTP_DECLARE_COMMAND(inst)        void execFtpCmd##inst()
#define FTP_DEFINE_COMMAND(inst)         void MainWindow::execFtpCmd##inst()

enum ConnectionType {
    NOT_SPECIFIED,
    PASV,
    PORT
};

class MainWindow : public QMainWindow {
Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QTcpSocket sockClient;
    QTcpSocket sockPasv;
    QTcpSocket *sockData;
    QTcpServer serverPort;
    bool uiConnectedState = false;
    std::string commandToExec;
    enum ConnectionType transferMode = NOT_SPECIFIED;

    void netCtrlTx(const std::string &data);

    std::string netCtrlRx();

    FTP_DECLARE_COMMAND(PASV);

    FTP_DECLARE_COMMAND(PORT);

    FTP_DECLARE_COMMAND(USER);

    FTP_DECLARE_COMMAND(PASS);

    FTP_DECLARE_COMMAND(SYST);

    FTP_DECLARE_COMMAND(TYPE);

    FTP_DECLARE_COMMAND(LIST);

    FTP_DECLARE_COMMAND(RETR);

    FTP_DECLARE_COMMAND(STOR);

    FTP_DECLARE_COMMAND(CWD);

    FTP_DECLARE_COMMAND(MKD);

    FTP_DECLARE_COMMAND(PWD);

    FTP_DECLARE_COMMAND(RMD);

public slots:

    void fastConnectOrQuit();

    void txCommandByWidgets();

    void uploadFile();

    void retrieveFile(int row, int column);

    void networkErrorOccurred(QAbstractSocket::SocketError socketError);

    void networkConnected();
};

#endif // MAINWINDOW_H
