#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTime>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    setWindowTitle(tr("记录仪测试工具Tester"));
    status = false;
    serverIP =new QHostAddress();
    ui->setupUi(this);
    dateEdit = ui->time_value;
    dateEdit->setCalendarPopup(true);
    data_sim_status = 0;
    heart_sim_status = 0;
    fault_sim_status = 0;

    /* 是否使能 */
    ui->can_send->setEnabled(false);
    ui->can_id_value->setEnabled(false);
    ui->time_send->setEnabled(false);
    ui->time_value->setEnabled(false);
    ui->heart_send->setEnabled(false);
    ui->sa_value->setEnabled(false);
    ui->file_record_fault->setEnabled(false);
    ui->mysql_record_fault->setEnabled(false);
    ui->disk_warning->setEnabled(false);
    ui->disk_alarm->setEnabled(false);
    ui->disk_null->setEnabled(false);
    ui->disk_size_max->setEnabled(false);
    ui->disk_size_now->setEnabled(false);
    ui->set_can_data_button->setEnabled(false);
    ui->set_time_data_button->setEnabled(false);
    ui->set_heart_data_button->setEnabled(false);
    ui->set_fault_button->setEnabled(false);
    ui->get_disk_button->setEnabled(false);

    /* 默认值 */
    ui->ip_value->setText(tr("192.168.8.136"));
    ui->port_value->setText(tr("9027"));

    /* 用于显示连接状态 */
    connect(ui->connect_button, SIGNAL(clicked()), this, SLOT(connect_status()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_connect_button_clicked()
{
    if(!status)
    {
        status_exp = true;

        /* 完成输入合法性检验 */
        QString ip = ui->ip_value->text();
        if(!serverIP->setAddress(ip))
        {
            QMessageBox::information(this,tr("错误"),tr("Err：收入的ip地址不正常!"));
            return;
        }
        if(ui->port_value->text()=="")
        {
            QMessageBox::information(this,tr("错误"),tr("Err：收入的端口号不正常!"));
            return;
        }

        QString str=ui->port_value->text();
        bool ok;
        int port = str.toInt(&ok);

        /* 创建了一个QTcpSocket类对象，并将信号/槽连接起来 */
        tcpSocket = new QTcpSocket(this);
        connect(tcpSocket,SIGNAL(connected()),this,SLOT (slotConnected()));
        connect(tcpSocket,SIGNAL(disconnected()),this,SLOT (slotDisconnected ()));
        connect(tcpSocket,SIGNAL(readyRead()),this,SLOT (dataReceived()));

        QString msg=tr("连接中........");
        ui->info_display->addItem(msg);

        tcpSocket->connectToHost(*serverIP,port);
    }
    else
    {
        status_exp = false;

        QString msg=tr("断开中........");
        ui->info_display->addItem(msg);

        tcpSocket->disconnectFromHost();

    }
}

void MainWindow::slotConnected()
{
    status=true;
    ui->can_send->setEnabled(true);
    ui->can_id_value->setEnabled(true);
    ui->time_send->setEnabled(true);
    ui->time_value->setEnabled(true);
    ui->heart_send->setEnabled(true);
    ui->sa_value->setEnabled(true);
    ui->file_record_fault->setEnabled(true);
    ui->mysql_record_fault->setEnabled(true);
    ui->disk_warning->setEnabled(true);
    ui->disk_alarm->setEnabled(true);
    ui->disk_null->setEnabled(true);
    ui->disk_size_max->setEnabled(true);
    ui->disk_size_now->setEnabled(true);
    ui->set_can_data_button->setEnabled(true);
    ui->set_time_data_button->setEnabled(true);
    ui->set_heart_data_button->setEnabled(true);
    ui->set_fault_button->setEnabled(true);
    ui->get_disk_button->setEnabled(true);
    ui->connect_button->setText(tr("断开"));

    QString msg=tr("连接完成！");
    ui->info_display->addItem(msg);
}

void MainWindow::slotDisconnected()
{
    status=false;                                   //将status状态复位
    ui->can_send->setEnabled(false);
    ui->can_id_value->setEnabled(false);
    ui->time_send->setEnabled(false);
    ui->time_value->setEnabled(false);
    ui->heart_send->setEnabled(false);
    ui->sa_value->setEnabled(false);
    ui->file_record_fault->setEnabled(false);
    ui->mysql_record_fault->setEnabled(false);
    ui->disk_warning->setEnabled(false);
    ui->disk_alarm->setEnabled(false);
    ui->disk_null->setEnabled(false);
    ui->disk_size_max->setEnabled(false);
    ui->disk_size_now->setEnabled(false);
    ui->set_can_data_button->setEnabled(false);
    ui->set_time_data_button->setEnabled(false);
    ui->set_heart_data_button->setEnabled(false);
    ui->set_fault_button->setEnabled(false);
    ui->get_disk_button->setEnabled(false);
    ui->connect_button->setText(tr("连接"));

    QString msg=tr("断开完成！");
    ui->info_display->addItem(msg);
}

void MainWindow::dataReceived()
{
    while(tcpSocket->bytesAvailable()>0)
    {
        QByteArray datagram;
        datagram.resize(tcpSocket->bytesAvailable());
        tcpSocket->read(datagram.data(),datagram.size());
        QString msg=datagram.data();

        ui->info_display->addItem(msg);

        if (msg.startsWith("SIZEMAX",Qt::CaseSensitive))
        {
            msg.remove(0, 8);
            ui->disk_size_max->setText(msg);
        }
        else if (msg.startsWith("SIZEVALID",Qt::CaseSensitive))
        {
            msg.remove(0, 10);
            ui->disk_size_now->setText(msg);
        }
        else
        {

        }
    }
}

void my_sleep(unsigned int msec)
{
    QTime dieTime = QTime::currentTime().addMSecs(msec);
    while( QTime::currentTime() < dieTime )
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void MainWindow::can_data_sim_proc()
{
    int i, j, z, k;
    char buffer[200];

    if (ui->can_send->checkState())
    {
        QString qstrSrc = ui->can_id_value->text();
        QByteArray ba = qstrSrc.toLatin1();
        const char *s = ba.data();

        i = 0;
        j = 0;
        z = 0;
        k = 0;
        memset(buffer, 0, 200);
        while(*s)
        {
            if(*s ==';')
            {
                if (j != 8)
                {
                    QMessageBox::information(this,tr("错误"),tr("Err：收入的can帧不正确，必须要4个字节!"));
                    return;
                }

                s++;
                QString msg1=tr("@CANDATA:") + QString(QLatin1String(buffer)) + tr(";");
                ui->info_display->addItem(msg1);
                tcpSocket->write(msg1.toLatin1(),msg1.length());
                data_sim_status = 1;
                memset(buffer, 0, 200);
                i = 0;
                j = 0;
                k = 1;
                z = 0;
                continue;
            }

            if (*s != ',' && z == 0)
            {
                j++;
            }
            else
            {
                if (j != 8)
                {
                    QMessageBox::information(this,tr("错误"),tr("Err：收入的can帧不正确，必须要4个字节!"));
                    return;
                }
                z = 1;
            }

            buffer[i] = *s;
            i++;
            s++;
        }

        if (k == 0)
        {
            QMessageBox::information(this,tr("错误"),tr("Err：收入的can帧不正确，必须要;结尾!"));
            return;
        }
    }
    else
    {
        QMessageBox::information(this,tr("提升"),tr("Info：请选择模拟项!"));
        ui->info_display->addItem("没有选中");
    }
}


void MainWindow::heart_data_sim_proc()
{
    int i, j, z, k;
    char buffer[200];

    if (ui->heart_send->checkState())
    {
        QString qstrSrc2 = ui->sa_value->text();
        QByteArray ba2 = qstrSrc2.toLatin1();
        const char *s2 = ba2.data();

        i = 0;
        j = 0;
        z = 0;
        k = 0;
        memset(buffer, 0, 200);
        while(*s2)
        {
            if(*s2 ==';')
            {
                if (j != 2)
                {
                    QMessageBox::information(this,tr("错误"),tr("Err：收入的SA不正确，必须要1个字节!"));
                    return;
                }

                s2++;
                QString msg3=tr("@HEARTDATA:") + QString(QLatin1String(buffer)) + tr(";");
                ui->info_display->addItem(msg3);
                tcpSocket->write(msg3.toLatin1(),msg3.length());
                heart_sim_status = 1;
                memset(buffer, 0, 200);
                i = 0;
                j = 0;
                k = 1;
                z = 0;
                continue;
            }

            if (*s2 != ',' && z == 0)
            {
                j++;
            }
            else
            {
                if (j != 2)
                {
                    QMessageBox::information(this,tr("错误"),tr("Err：输入的SA不正确，必须要1个字节!"));
                    return;
                }
                z = 1;
            }

            buffer[i] = *s2;
            i++;
            s2++;
        }

        if (k == 0)
        {
            QMessageBox::information(this,tr("错误"),tr("Err：输入的SA不正确，必须要;结尾!"));
            return;
        }
    }
    else
    {
        QMessageBox::information(this,tr("提升"),tr("Info：请选择模拟项!"));
         ui->info_display->addItem("没有选中");
    }
}

void MainWindow::connect_status()
{
    int i;
    QString s;

    for (i = 1; i < 11; i++)
    {
        my_sleep(1000);
        if (status_exp == status)
        {
            return;
        }
        s = QString::number(i) + "s";
        ui->info_display->addItem(s);
    }

    if (i == 11)
    {
        QString msg;
        if (status_exp)
        {
            msg=tr("连接失败！");
        }
        else
        {
            msg=tr("断开失败！");
        }

        ui->info_display->addItem(msg);
    }
    return;
}

void MainWindow::on_set_can_data_button_clicked()
{
    if (data_sim_status == 0)
    {
        can_data_sim_proc();
        if (data_sim_status == 1)
        {
            ui->set_can_data_button->setText(tr("取消"));
        }
    }
    else
    {
        data_sim_status = 0;
        ui->set_can_data_button->setText(tr("模拟"));
        ui->info_display->addItem("数据模拟取消");
        QString msg=tr("CANDEL:");
        ui->info_display->addItem(msg);
        tcpSocket->write(msg.toLatin1(),msg.length());
    }
}

void MainWindow::on_set_fault_button_clicked()
{
    if (fault_sim_status == 0)
    {
        fault_sim_status = 1;
        ui->set_fault_button->setText(tr("取消"));
        QString msg= tr("FAULTSIM:") + QString::number(ui->file_record_fault->checkState()) + QString::number(ui->mysql_record_fault->checkState())
                + QString::number(ui->disk_warning->checkState()) + QString::number(ui->disk_alarm->checkState()) + QString::number(ui->disk_null->checkState());
        ui->info_display->addItem(msg);
        tcpSocket->write(msg.toLatin1(),msg.length());
    }
    else
    {
        fault_sim_status = 0;
        ui->set_fault_button->setEnabled(false);
        ui->info_display->addItem("故障模拟取消");
        QString msg1= tr("FAULTSIM:00000");
        ui->info_display->addItem(msg1);
        tcpSocket->write(msg1.toLatin1(),msg1.length());

        my_sleep(5000);
        QString msg=tr("FAULTDEL:");
        ui->info_display->addItem(msg);
        tcpSocket->write(msg.toLatin1(),msg.length());
        ui->set_fault_button->setText(tr("模拟"));
        ui->set_fault_button->setEnabled(true);
    }
}

void MainWindow::on_get_disk_button_clicked()
{
    QString msg=tr("GETDISKSIZE:");
    ui->info_display->addItem(msg);
    tcpSocket->write(msg.toLatin1(),msg.length());
}

void MainWindow::on_set_heart_data_button_clicked()
{
    if (heart_sim_status == 0)
    {
        heart_data_sim_proc();
        if (heart_sim_status == 1)
        {
            ui->set_heart_data_button->setText(tr("取消"));
        }
    }
    else
    {
        heart_sim_status = 0;
        ui->set_heart_data_button->setText(tr("模拟"));
        ui->info_display->addItem("心跳模拟取消");
        QString msg=tr("HEARTDEL:");
        ui->info_display->addItem(msg);
        tcpSocket->write(msg.toLatin1(),msg.length());
    }
}

void MainWindow::on_set_time_data_button_clicked()
{
    if (ui->time_send->checkState())
    {
        QString strYear = dateEdit->sectionText(QDateTimeEdit::YearSection);
        QString strMonth = dateEdit->sectionText(QDateTimeEdit::MonthSection);
        QString strDay = dateEdit->sectionText(QDateTimeEdit::DaySection);
        QString strHour = dateEdit->sectionText(QDateTimeEdit::HourSection);
        QString strMinute = dateEdit->sectionText(QDateTimeEdit::MinuteSection);
        bool ok;
        int year = strYear.toInt(&ok);

        if (year < 2000)
        {
             QMessageBox::information(this,tr("错误"),tr("Err：时间年份要大于2000!"));
             return;
        }
        year = year - 2000;
        QString msg2=tr("TIMESET:") + QString::number(year)+tr("-") + strMonth+tr("-") + strDay+tr("-") + strHour+tr("-") + strMinute+tr("-") + tr("0");
        ui->info_display->addItem(msg2);
        tcpSocket->write(msg2.toLatin1(),msg2.length());
    }
    else
    {
        QMessageBox::information(this,tr("提示"),tr("Info：请选择模拟项!"));
        ui->info_display->addItem("没有选中");
    }
}
