#ifndef PLAYER_H
#define PLAYER_H

#include <QWidget>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>//消息框
#include <QJsonArray>
#include <QCloseEvent>//关闭事件

namespace Ui {
class Player;
}

#define SEQUENCE      1
#define CIRCLE        2

class Player : public QWidget
{
    Q_OBJECT

public:
    explicit Player(QTcpSocket *, QString, QString, QWidget *parent = 0);
    ~Player();

private slots:
    void server_reply_slot();

    void timeout_slot();

    void on_playButton_clicked();

    void on_nextButton_clicked();

    void on_priorButton_clicked();

    void on_upButton_clicked();

    void on_downButton_clicked();

    void on_circleButton_clicked();

    void on_sequenceButton_clicked();

private:
    void player_send_data(QJsonObject &);

    void player_recv_data(QByteArray &);

    void player_update_info(QJsonObject &);

    void player_update_music(QJsonObject &);

    void player_get_music();

    void player_start_reply(QJsonObject &);

    void player_suspend_reply(QJsonObject &);

    void player_continue_reply(QJsonObject &);

    void player_next_reply(QJsonObject &);

    void player_prior_reply(QJsonObject &);

    void player_voice_reply(QJsonObject &);

    void player_mode_reply(QJsonObject &);

    void closeEvent(QCloseEvent *);

private:
    Ui::Player *ui;
    QTcpSocket *socket;
    QString appid;
    QString deviceid;
    QTimer *timer;
    quint8 flag; //用于主动获取音乐列表的标志位
    quint8 start_flag;
    quint8 suspend_flag;
};

#endif // PLAYER_H
