#ifndef PCMWINDOW_H
#define PCMWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTcpServer>
#include <QVector>
#include <QMap>

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
    static QVector<unsigned int> *pviTheFieldIndexes;
    };

class PCMWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PCMWindow(QWidget *parent = 0);
    ~PCMWindow();

private:
    Ui::PCMWindow *ui;
    QLabel *pqlMyImage;
    QTcpServer *pMyTCPServer;

    QVector<QTcpSocket*> vMyTCPSockets;

private slots:
    void NewTCPConnection();
    void TCPSocketReadReady();
    void TCPDisconnected();
};

#endif // PCMWINDOW_H
