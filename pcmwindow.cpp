#include "pcmwindow.h"
#include "ui_pcmwindow.h"

PCMWindow::PCMWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PCMWindow)
{
    ui->setupUi(this);
}

PCMWindow::~PCMWindow()
{
    delete ui;
}
