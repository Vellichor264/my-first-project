#ifndef STARTMENU_H
#define STARTMENU_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>

class StartMenu : public QWidget
{
    Q_OBJECT
public:
    explicit StartMenu(QWidget *parent = nullptr);

signals:
    void startGame();   // 点击开始游戏按钮时发射
    void quitGame();    // 点击退出按钮时发射

private slots:
    void onStartClicked();
    void onQuitClicked();

private:
    QPushButton *m_startBtn;
    QPushButton *m_quitBtn;
};

#endif // STARTMENU_H