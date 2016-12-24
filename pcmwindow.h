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


namespace Ui {
class PCMWindow;
}

class InventoryCard
{
public:
    InventoryCard(int Quantity);
    InventoryCard(QString sInitLine);
    static void InitOrder(QString sInitLine);

    int iMyCount, iMyTradelistCount, iMyCardNumber;
    QString sMyName, sMyEdition, sMyCondition, sMyLanguage, sMyRarity;
    bool bMyFoil, bMySigned, bMyArtistProof, bMyAlteredArt, bMyMisprint, bMyPromo, bMyTextless;
    double dMySalePrice, dMyMarketPrice;
    static QMap<QString, unsigned int> qmTheStringIndex;

private:
    static bool InitOrderEstablished;
    static QVector<unsigned int> *pviTheFieldIndexes;
};

class OracleCard
{
public:
    quint64 iMultiverseID;
    QString sNameEn, sMySet, sID, sSequenceNumber;
    double dValue;
    static QString sImagePath;
    QString getImagePath() const;
    QString getImageURL() const;
    QString deckBoxInventoryLine(bool Foil) const;
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

    QMap<quint64, InventoryCard>        qmMyRegularInventory, qmMyFoilInventory;
    QMap<quint64, InventoryCard>        qmMyRegularPriceGuide, qmMyFoilPriceGuide;
    QMap<quint64, QString>    qmMultiverse;
    QMap<QString, quint64>    qmMultiInverse;
    QMap<QString, QString>    qmTheSetCode;
    QMap<quint64, OracleCard> qmOracle;

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

private slots:
    void NewTCPConnection();
    void TCPSocketReadReady();
    void TCPDisconnected();
    void on_pbOpenCollection_clicked();
    void on_pbOpenDatabase_clicked();
    void on_pbOpenOutputs_clicked();
    void ImageFetchFinished(QNetworkReply* reply);
    void on_pbFullCardListDB_clicked();
};

#endif // PCMWINDOW_H
