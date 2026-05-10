#include "startmenu.h"
#include <QApplication>

StartMenu::StartMenu(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(400, 300);
    setWindowTitle("保卫萝卜 - 开始界面");

    m_startBtn = new QPushButton("开始游戏", this);
    m_quitBtn = new QPushButton("退出游戏", this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_startBtn);
    layout->addWidget(m_quitBtn);
    setLayout(layout);

    connect(m_startBtn, &QPushButton::clicked, this, &StartMenu::onStartClicked);
    connect(m_quitBtn, &QPushButton::clicked, this, &StartMenu::onQuitClicked);
}

void StartMenu::onStartClicked()
{
    emit startGame();
    close(); // 关闭菜单窗口
}

void StartMenu::onQuitClicked()
{
    emit quitGame();
    QApplication::quit();
}