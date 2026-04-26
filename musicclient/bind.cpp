#include "bind.h"
#include "ui_bind.h"
#include "player.h"

Bind::Bind(QTcpSocket *s, QString id, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Bind)
{
    ui->setupUi(this);

    this->socket = s;
    this->appid = id;

    this->setWindowTitle("7SamrtSpeaker");

    QFont f("黑体", 14);
    ui->deviceEdit->setFont(f);

    connect(socket, &QTcpSocket::connected, [this]()
    {
        QMessageBox::information(this, "连接提示", "连接服务器成功");
    });

    connect(socket, &QTcpSocket::disconnected, [this]()
    {
        QMessageBox::warning(this, "连接提示", "网络异常断开");
    });

    connect(socket, &QTcpSocket::readyRead, this, &Bind::server_reply_slot);
}

void Bind::server_reply_slot()
{
    QByteArray recvData;

    bind_recv_data(recvData);

    QJsonObject obj = QJsonDocument::fromJson(recvData).object();

    QString cmd = obj.value("cmd").toString();

    if (cmd == "app_bind_reply")
    {
        QString result = obj.value("result").toString();
        if (result == "success")
        {
            socket->disconnect(SIGNAL(connected()));
            socket->disconnect(SIGNAL(disconnected()));
            socket->disconnect(SIGNAL(readyRead()));

            Player *m_player = new Player(socket, appid, m_deviceid);
            m_player->show();
            this->hide();
        }
    }
}

void Bind::bind_recv_data(QByteArray &ba)
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

Bind::~Bind()
{
    delete ui;
}

void Bind::on_bindButton_clicked()
{
    QString deviceid = ui->deviceEdit->text();
    this->m_deviceid = deviceid;

    QJsonObject obj;
    obj.insert("cmd", "app_bind");
    obj.insert("deviceid", deviceid);
    obj.insert("appid", appid);

    bind_send_data(obj);
}

void Bind::bind_send_data(QJsonObject &obj)
{
    QByteArray sendData;
    QByteArray ba = QJsonDocument(obj).toJson();

    int size = ba.size();
    sendData.insert(0, (const char *)&size, 4);
    sendData.append(ba);

    socket->write(sendData);
}
