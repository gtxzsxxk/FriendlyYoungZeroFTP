#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define FTP_DECLARE_INST(inst)        void execFtpCmd##inst()

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QTcpSocket sockClient;
    bool uiConnectedState = false;

    void netCtrlTx(const std::string &data);

    std::string netCtrlRx();

    FTP_DECLARE_INST(PASV);
    FTP_DECLARE_INST(PORT);
    FTP_DECLARE_INST(USER);

public slots:
    void fastConnectOrQuit();

    void txCommandByWidgets();

    void uploadFile();

    void retrieveFile(int row, int column);

    void networkErrorOccurred(QAbstractSocket::SocketError socketError);

    void networkConnected();
};
#endif // MAINWINDOW_H
