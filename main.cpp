#include "pcmwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    PCMWindow w;
    w.show();

    return a.exec();
}
