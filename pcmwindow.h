#ifndef PCMWINDOW_H
#define PCMWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTcpServer>
#include <QVector>
#include <QMap>
#include <QFile>
#include <QMediaPlayer>
#include "../GLib/oracle.h"


namespace Ui {
class PCMWindow;
}

class InventoryCard
{
public:
    InventoryCard(QString sInitLine);
    static void InitOrder(QString sInitLine);

    unsigned int iMyCount, iMyTradelistCount, iMyCardNumber;
    QString sMyName, sMyEdition, sMyCondition, sMyLanguage, sMyRarity;
    bool bMyFoil, bMySigned, bMyArtistProof, bMyAlteredArt, bMyMisprint, bMyPromo, bMyTextless;
    double dMySalePrice, dMyMarketPrice;
    static QMap<QString, unsigned int> qmTheStringIndex;

private:
    static bool InitOrderEstablished;
    static QVector<int> *pviTheFieldIndexes;
};


class PCMWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PCMWindow(QWidget *parent = 0);
    ~PCMWindow();

private:
    Ui::PCMWindow *ui;
    QTcpServer *pMyTCPServer;

    QMap<QString, int>        qmInventory;
    Oracle myOracle;

    QTextStream fMyCollectionOutput, fMyTradesOutput;

    //Sounds
    QMediaPlayer mySound;

private slots:
    void NewTCPConnection();
    void TCPSocketReadReady();
    void TCPDisconnected();
    void on_pbOpenCollection_clicked();
    void on_pbOpenDatabase_clicked();
    void on_pbOpenOutputs_clicked();
};

#endif // PCMWINDOW_H
