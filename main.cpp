#include "gamewidget.h"
#include "startmenu.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    StartMenu menu;
    QObject::connect(&menu, &StartMenu::startGame, [&]() {
        GameWidget *game = new GameWidget();
        game->show();
    });
    menu.show();

    return a.exec();
}