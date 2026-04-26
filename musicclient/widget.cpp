#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include "bind.h"
#include "player.h"

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    this->setWindowTitle("7SamrtSpeaker");

    QFont f("黑体", 14);
    ui->appEdit->setFont(f);
    ui->passEdit->setFont(f);

    socket = new QTcpSocket;
    //socket->connectToHost(QHostAddress("106.14.253.205"), 8000);
    socket->connectToHost(QHostAddress("121.41.167.17"), 8000);
    //lambda表达式,将其作为connected信号的槽函数
    connect(socket, &QTcpSocket::connected, [this]()
    {// 函数体
        QMessageBox::information(this, "连接提示", "连接服务器成功");
    });

    //lambda表达式,将其作为disconnected信号的槽函数
    connect(socket, &QTcpSocket::disconnected, [this]()
    {
        QMessageBox::warning(this, "连接提示", "网络异常断开");
    });
    //这里不是lambda表达式,而是普通函数
    //将readyRead信号与server_reply_slot槽函数绑定
    connect(socket, &QTcpSocket::readyRead, this, &Widget::server_reply_slot);
}

Widget::~Widget()
{
    delete ui;
}

//注册按钮点击事件
void Widget::on_registerButton_clicked()
{
    //获取appid和password
    QString appid = ui->appEdit->text();
    QString password = ui->passEdit->text();

    //发送给服务器   {"cmd":"app_register", "appid":"1001", "password":"1111"}
    QJsonObject obj;
    obj.insert("cmd", "app_register");
    obj.insert("appid", appid);
    obj.insert("password", password);

    widget_send_data(obj);
}
//发送数据到服务器
void Widget::widget_send_data(QJsonObject &obj)
{
    QByteArray sendData;//字节数组
    QByteArray ba = QJsonDocument(obj).toJson();//将json对象转换为json字符串

    int size = ba.size();//获取json字符串的长度
    sendData.insert(0, (const char *)&size, 4);//长度信息
    sendData.append(ba);//将json字符串添加到字节数组中

    socket->write(sendData);//发送数据
}
//服务器回复槽函数
void Widget::server_reply_slot()
{
    QByteArray recvData;

    widget_recv_data(recvData);
    //将json字符串转换为json对象
    QJsonObject obj = QJsonDocument::fromJson(recvData).object();
    //根据命令执行不同的处理函数
    QString cmd = obj.value("cmd").toString();//获取命令

    if (cmd == "app_register_reply")
    {
        app_register_handler(obj);
    }
    else if (cmd == "app_login_reply")
    {
        app_login_handler(obj);
    }
}

//接收数据从服务器
//输入参数：字节数组的引用
void Widget::widget_recv_data(QByteArray &ba)
{
    char buf[1024] = {0};
    qint64 size = 0;

    while (true)
    {
        size += socket->read(buf + size, sizeof(int) - size);
        if (size >= sizeof(int))
            break;
    }

    int len = *(int *)buf;
    size = 0;
    memset(buf, 0, sizeof(buf));

    while (true)
    {
        size += socket->read(buf + size, len - size);
        if (size >= len)
            break;
    }

    ba.append(buf);

    qDebug() << "收到服务器 " << len << " 字节 : " << ba;
}

//登录处理函数
//输入参数：json对象的引用
void Widget::app_login_handler(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "not_exist")
    {
        QMessageBox::warning(this, "登录提示", "用户不存在，请先注册");
    }
    else if (result == "password_error")
    {
        QMessageBox::warning(this, "登录提示", "密码或者用户名错误");
    }
    else if (result == "not_bind")
    {
        //QMessageBox::information(this, "登录提示", "登录成功 未绑定");

        socket->disconnect(SIGNAL(connected()));//断开信号与槽函数的绑定
        socket->disconnect(SIGNAL(disconnected()));//防止新的页面
        socket->disconnect(SIGNAL(readyRead()));

        //创建新的界面用于绑定设备，将socket和appid传递给新的界面
        Bind *m_bind = new Bind(socket, m_appid);
        m_bind->show();
        this->hide();
    }
    else if (result == "bind")
    {
        //QMessageBox::information(this, "登录提示", "登录成功 已经绑定");

        QString deviceid = obj.value("deviceid").toString();

        socket->disconnect(SIGNAL(connected()));
        socket->disconnect(SIGNAL(disconnected()));
        socket->disconnect(SIGNAL(readyRead()));

        //创建新的界面用于播放音乐，将socket和appid传递给新的界面
        Player *m_player = new Player(socket, m_appid, deviceid);
        m_player->show();
        this->hide();
    }
}

void Widget::app_register_handler(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "success")
    {
        QMessageBox::information(this, "注册提示", "注册成功");
    }
    else if (result == "failure")
    {
        QMessageBox::warning(this, "注册提示", "注册失败");
    }
}




void Widget::on_loginButton_clicked()
{
    QString appid = ui->appEdit->text();
    QString password = ui->passEdit->text();

    this->m_appid = appid;

    QJsonObject obj;
    obj.insert("cmd", "app_login");
    obj.insert("appid", appid);
    obj.insert("password", password);

    widget_send_data(obj);
}
