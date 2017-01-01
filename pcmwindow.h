#ifndef PCMWINDOW_H
#define PCMWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTcpServer>
#include <QVector>
#include <QMap>
#include <QJsonDocument>
//#include <QXmlStreamReader>
#include <QFile>
#include <QNetworkAccessManager>
#include <QMediaPlayer>
#include <QUrl>
#include "inventorycard.h"
#include "oraclecard.h"


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
    QTcpServer *pMyTCPServer;

    QMap<quint64, InventoryCard>        qmMyRegularInventory, qmMyFoilInventory;
    QMap<quint64, InventoryCard>        qmMyRegularPriceGuide, qmMyFoilPriceGuide;
    QMap<quint64, QString>    qmMultiverse;
    QMap<QString, quint64>    qmMultiInverse;
    QMap<QString, QString>    qmTheSetCode;
    QMap<quint64, OracleCard> qmOracle;

    //Sounds
    QMediaPlayer mySound;

    QJsonParseError jError;
    //QXmlStreamReader reader;
    //void ReadOracle();
    //void ReadSets();
    //void ReadCards();
    //void ReadSet();
    //void ReadCard();

    QNetworkAccessManager *manager;
    QString sMyImageRequested;
    void DisplayImage(QString);

    QTextStream fMyCollectionOutput, fMyTradesOutput;

    void LoadInventory(QMap<quint64, InventoryCard>* qmRegularInventory, QMap<quint64, InventoryCard>* qmFoilInventory, QString sFileSource, bool AddNotMax);

    void StatusString(QString sMessage, bool bError = false);

    //my sounds
    QUrl qTrash, qCoins, qKeep, qWeird;

private slots:
    void NewTCPConnection();
    void TCPSocketReadReady();
    void TCPDisconnected();
    void on_pbOpenCollection_clicked();
    void on_pbOpenDatabase_clicked();
    void on_pbOpenOutputs_clicked();
    void ImageFetchFinished(QNetworkReply* reply);
    void on_pbFullCardListDB_clicked();
    void on_soundsLocationLineEdit_textChanged(const QString &arg1);
};

#endif // PCMWINDOW_H
