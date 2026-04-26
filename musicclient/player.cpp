#include "player.h"
#include "ui_player.h"

Player::Player(QTcpSocket *s, QString id, QString devid, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Player)
{
    ui->setupUi(this);

    this->socket = s;
    this->appid = id;
    this->deviceid = devid;

    this->setWindowTitle("7SamrtSpeaker");

    flag = 1;//初始化标志位为1，表示第一次检测到音箱在线，主动获取音乐列表
    start_flag = 0;
    suspend_flag = 0;

    QFont f("黑体", 14);
    ui->playButton->setFont(f);
    ui->playButton->setText("▷");

    connect(socket, &QTcpSocket::connected, [this]()
    {
        QMessageBox::information(this, "连接提示", "连接服务器成功");
    });

    connect(socket, &QTcpSocket::disconnected, [this]()
    {
        QMessageBox::warning(this, "连接提示", "网络异常断开");
    });

    connect(socket, &QTcpSocket::readyRead, this, &Player::server_reply_slot);

    //创建2秒定时器
    timer = new QTimer();
    timer->start(2000);
    connect(timer, &QTimer::timeout, this, &Player::timeout_slot);
}

Player::~Player()
{
    delete ui;
}

/* 服务器回复槽函数 */
/* 这里主要考虑到音箱自主上报周期是2秒，ui显示回延迟*/
/* 所以每次执行完一个命令之后，音箱额外上报命令结果给服务器*/
// 输入参数：void
// 输出参数：void
// 返回值：void
// 接收服务器回复的数据，并根据回复的命令执行相应的操作
void Player::server_reply_slot()
{
    QByteArray recvData;

    player_recv_data(recvData);

    QJsonObject obj = QJsonDocument::fromJson(recvData).object();

    QString cmd = obj.value("cmd").toString();
    if (cmd == "info")
    {//更新信息
        player_update_info(obj);

        if (flag)//如果标志位为1，则主动获取音乐列表
        {
            player_get_music();
            flag = 0;
        }
    }
    else if (cmd == "upload_music")
    {//这里是音箱更新音乐列表之后主动上传音乐列表
        player_update_music(obj);
    }
    else if (cmd == "app_start_reply")
    {//命令音箱启动播放的回复，回复之后更新ui
        player_start_reply(obj);
    }
    else if (cmd == "app_suspend_reply")
    {//命令音箱暂停播放的回复
        player_suspend_reply(obj);
    }
    else if (cmd == "app_continue_reply")
    {//命令音箱继续播放的回复
        player_continue_reply(obj);
    }
    else if (cmd == "app_next_reply")
    {//命令音箱播放下一首的回复
        player_next_reply(obj);
    }
    else if (cmd == "app_prior_reply")
    {//命令音箱播放上一首的回复
        player_prior_reply(obj);
    }
    else if (cmd == "app_voice_up_reply" || cmd == "app_voice_down_reply")
    {//命令音箱变更音量的回复
        player_voice_reply(obj);
    }
    else if (cmd == "app_circle_reply" || cmd == "app_sequence_reply")
    {//命令音箱变更播放模式的回复
        player_mode_reply(obj);
    }
}

// 变更在线\离线模式的回复
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_mode_reply(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "offline")
    {
        ui->circleButton->setChecked(false);
        ui->sequenceButton->setChecked(true);

        QMessageBox::warning(this, "播放提示", "音箱离线");
    }
}

// 变更音量的回复
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_voice_reply(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "offline")
    {
        QMessageBox::warning(this, "播放提示", "音箱离线");
    }
    else if (result == "success")
    {
        int volume = obj.value("voice").toInt();

        QFont f("黑体", 14);
        ui->volumeLabel->setFont(f);
        ui->volumeLabel->setText(QString::number(volume));
    }
}

// 命令音箱播放下一首的回复
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_next_reply(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "offline")
    {
        QMessageBox::warning(this, "播放提示", "音箱离线");
    }
    else if (result == "success")
    {
        QString music = obj.value("music").toString();

        QFont f("黑体", 14);
        ui->musicLabel->setFont(f);
        ui->musicLabel->setText(music);
    }
}

// 命令音箱播放上一首的回复
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_prior_reply(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "offline")
    {
        QMessageBox::warning(this, "播放提示", "音箱离线");
    }
    else if (result == "success")
    {
        QString music = obj.value("music").toString();

        QFont f("黑体", 14);
        ui->musicLabel->setFont(f);
        ui->musicLabel->setText(music);
    }
}

// 命令音箱继续播放的回复
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_continue_reply(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "offline")
    {
        QMessageBox::warning(this, "播放提示", "音箱离线");
    }
    else if (result == "success")
    {
        suspend_flag = 0;

        QFont f("黑体", 14);
        ui->playButton->setFont(f);
        ui->playButton->setText("||");
    }
}

// 命令音箱暂停播放的回复
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_suspend_reply(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "offline")
    {
        QMessageBox::warning(this, "播放提示", "音箱离线");
    }
    else if (result == "success")
    {
        suspend_flag = 1;

        QFont f("黑体", 14);
        ui->playButton->setFont(f);
        ui->playButton->setText("▷");
    }
}

// 命令音箱启动播放的回复
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_start_reply(QJsonObject &obj)
{
    QString result = obj.value("result").toString();

    if (result == "offline")
    {
        QMessageBox::warning(this, "播放提示", "音箱离线");
    }
    else if (result == "failure")
    {
        QMessageBox::warning(this, "播放提示", "音箱启动失败");
    }
    else if (result == "success")
    {
        start_flag = 1;

        QFont f("黑体", 14);
        ui->playButton->setFont(f);
        ui->playButton->setText("||");
    }
}

// 第一次检测到音箱在线，主动获取音乐列表
// 输入参数：void
// 输出参数：void
// 返回值：void
void Player::player_get_music()
{
    QJsonObject obj;
    obj.insert("cmd", "app_get_music");

    player_send_data(obj);
}

// 更新音乐
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_update_music(QJsonObject &obj)
{
    QJsonArray arr = obj.value("music").toArray();

    QFont f("黑体", 14);
    ui->textEdit->setFont(f);

    QString result;

    for (int i = 0; i < arr.size(); i++)
    {
        result.append(arr.at(i).toString());//将音乐列表中的音乐名称添加到result中
        result.append('\n');//添加换行符
    }

    ui->textEdit->setText(result);//设置音乐列表文本框的文本
}

// 更新信息
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
// 通过更新的信息刷新界面上的设备ID、当前音乐、音量、播放模式等信息
void Player::player_update_info(QJsonObject &obj)
{
    QString cur_music = obj.value("cur_music").toString();
    QString dev_id = obj.value("deviceid").toString();
    QString status = obj.value("status").toString();

    int volume = obj.value("volume").toInt();
    int mode = obj.value("mode").toInt();

    QFont f("黑体", 14);
    //设置设备ID标签的字体和文本
    ui->devidLabel->setFont(f);
    ui->devidLabel->setText(dev_id);

    //设置当前音乐标签的字体和文本
    ui->musicLabel->setFont(f);
    ui->musicLabel->setText(cur_music);

    //设置音量标签的字体和文本
    ui->volumeLabel->setFont(f);
    ui->volumeLabel->setText(QString::number(volume));
    //设置播放模式按钮的选中状态
    if (mode == SEQUENCE)
    {
        ui->sequenceButton->setChecked(true);
    }
    else if (mode == CIRCLE)
    {
        ui->circleButton->setChecked(true);
    }
    //设置播放按钮的显示状态
    ui->playButton->setFont(f);
    if (status == "start")
    {//双引号表示字符串
        ui->playButton->setText("||");
        start_flag = 1;
        suspend_flag = 0;
    }
    else if (status == "stop")
    {
        ui->playButton->setText("▷");
        start_flag = 0;
        suspend_flag = 0;
    }
    else if (status == "suspend")
    {
        ui->playButton->setText("▷");
        start_flag = 1;
        suspend_flag = 1;
    }
}

// 接收数据从服务器
// 输入参数：字节数组的引用
// 输出参数：void
// 返回值：void
void Player::player_recv_data(QByteArray &ba)
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


// 定时器槽函数
void Player::timeout_slot()
{
    QJsonObject obj;
    obj.insert("cmd", "app_info");
    obj.insert("appid", appid);
    obj.insert("deviceid", deviceid);

    player_send_data(obj);
}

// 发送数据到服务器
// 输入参数：json对象的引用
// 输出参数：void
// 返回值：void
void Player::player_send_data(QJsonObject &obj)
{
    QByteArray sendData;
    QByteArray ba = QJsonDocument(obj).toJson();

    int size = ba.size();
    sendData.insert(0, (const char *)&size, 4);
    sendData.append(ba);

    socket->write(sendData);
}

/*   下面都是各类按钮点击事件的槽函数  */
/* 都是QT中ui设计器自动生成的信号与槽关系*/

// 播放按钮点击事件
// 输入参数：void
// 输出参数：void
// 返回值：void
// 根据播放按钮的当前状态，发送相应的命令给服务器
void Player::on_playButton_clicked()
{
    if (start_flag == 0)
    {//如果播放按钮的当前状态为停止，则发送启动播放命令
        QJsonObject obj;
        obj.insert("cmd", "app_start");

        player_send_data(obj);
    }
    else if (start_flag == 1 && suspend_flag == 1)
    {//如果播放按钮的当前状态为暂停，则发送继续播放命令
        QJsonObject obj;
        obj.insert("cmd", "app_continue");

        player_send_data(obj);
    }
    else if (start_flag == 1 && suspend_flag == 0)
    {//如果播放按钮的当前状态为播放，则发送暂停播放命令
        QJsonObject obj;
        obj.insert("cmd", "app_suspend");

        player_send_data(obj);
    }
}

// 下一首按钮点击事件
void Player::on_nextButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd", "app_next");

    player_send_data(obj);
}

// 上一首按钮点击事件
void Player::on_priorButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd", "app_prior");

    player_send_data(obj);
}

// 增加音量按钮点击事件
void Player::on_upButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd", "app_voice_up");

    player_send_data(obj);
}

// 减小音量按钮点击事件
void Player::on_downButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd", "app_voice_down");

    player_send_data(obj);
}

// 单曲循环按钮点击事件
// 基于QRadioButton实现二选一的单选按钮
void Player::on_circleButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd", "app_circle");

    player_send_data(obj);

    ui->circleButton->setChecked(true);// 单击按钮后，设置单选按钮为选中状态
}

// 列表循环按钮点击事件
void Player::on_sequenceButton_clicked()
{
    QJsonObject obj;
    obj.insert("cmd", "app_sequence");

    player_send_data(obj);

    ui->sequenceButton->setChecked(true);
}

// 关闭事件
// 输入参数：关闭事件的指针
// 输出参数：void
// 返回值：void
// 发送app离线命令给服务器，并关闭窗口
void Player::closeEvent(QCloseEvent *e)
{
    QJsonObject obj;
    obj.insert("cmd", "app_offline");

    player_send_data(obj);

    e->accept();
}
