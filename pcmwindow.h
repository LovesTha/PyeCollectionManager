#ifndef PCMWINDOW_H
#define PCMWINDOW_H

#include <QMainWindow>

namespace Ui {
class PCMWindow;
}

class PCMWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PCMWindow(QWidget *parent = 0);
    ~PCMWindow();

private:
    Ui::PCMWindow *ui;
};

#endif // PCMWINDOW_H
