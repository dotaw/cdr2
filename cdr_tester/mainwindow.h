#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QHostAddress>
#include <QTcpSocket>
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_connect_button_clicked();
    void slotConnected();
    void slotDisconnected();
    void dataReceived();
    void connect_status();
    void can_data_sim_proc();
    void heart_data_sim_proc();
    void on_set_can_data_button_clicked();

    void on_set_fault_button_clicked();

    void on_get_disk_button_clicked();

    void on_set_heart_data_button_clicked();

    void on_set_time_data_button_clicked();

private:
    Ui::MainWindow *ui;
    bool status;
    bool status_exp;
    bool data_sim_status;
    bool heart_sim_status;
    bool fault_sim_status;
    QHostAddress *serverIP;
    QTcpSocket *tcpSocket;
    QDateTimeEdit *dateEdit;
};

#endif // MAINWINDOW_H
