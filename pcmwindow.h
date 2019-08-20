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
#include <QTimer>
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

    QMap<quint64, InventoryCard>        qmMyRegularCollectorInventory, qmMyFoilCollectorInventory;
    QMap<QString, InventoryCard>        qmMyRegularPlayerInventory, qmMyFoilPlayerInventory;
    QMap<quint64, InventoryCard>        qmMyRegularPriceGuide, qmMyFoilPriceGuide;
    QMultiMap<QString, quint64>    qmmMultiInverse;
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

    QNetworkAccessManager *imageFetchManager, *oracleVersionFetchManager, *oracleFetchManager;
    QList<OracleCard> lRequestedImages;
    //QString sMyImageRequested;
    void DisplayImage(QString);

    QTextStream fMyCollectionOutput, fMyTradesOutput;

    void LoadInventory(QMap<quint64, InventoryCard>* qmRegularInventory, QMap<quint64, InventoryCard>* qmFoilInventory, QString sFileSource, bool AddNotMax, bool InvertValue = false);

    void StatusString(QString sMessage, bool bError = false);

    //my sounds
    QUrl qTrash, qCoins, qKeep, qWeird;

    void HandleSingleCard(OracleCard card);
    void HandleMultipleCards(OracleCard card, QList<quint64> lCardIDs);
    void trash(QString card);

    bool isFoil();
    bool Needed(OracleCard card, int *iRegCount = 0, int *iFoilCnt = 0);
    void CleanSetSelection();

    bool bOracleVersionRetrieved, bNewOracleRetrieved;
    QString sMyOracleVersion;
    void recordOracleVersion();

    void onStartOracleCheck();

    void StartCardRepeatWindow(uint msecs);
    bool cardRepeatWindow;
    unsigned int LastCardMultiverse;

private slots:
    void NewTCPConnection();
    void ScryGlassRequestReceived();
    void TCPDisconnected();
    void on_pbOpenCollection_clicked();
    void on_pbOpenDatabase_clicked();
    void on_pbOpenOutputs_clicked();
    void ImageFetchFinished(QNetworkReply* reply);
    void OracleVersionFetchFinished(QNetworkReply* reply);
    void OracleFetchFinished(QNetworkReply* reply);
    void on_pbFullCardListDB_clicked();
    void on_soundsLocationLineEdit_textChanged(const QString &arg1);
    void setSelected();
    void on_FetchOracle_clicked();
    void cardRepeatWindowFinish();
};

#endif // PCMWINDOW_H
