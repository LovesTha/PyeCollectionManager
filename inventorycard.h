#ifndef INVENTORYCARD_H
#define INVENTORYCARD_H

#include <QString>

class InventoryCard
{
public:
    explicit InventoryCard(int Quantity);
    explicit InventoryCard(QString sInitLine);
    static void InitOrder(QString sInitLine);

    int iMyCount, iMyTradelistCount, iMyCardNumber;
    QString sMyName, sMyEdition, sMyCondition, sMyLanguage, sMyRarity;
    bool bMyFoil, bMySigned, bMyArtistProof, bMyAlteredArt, bMyMisprint, bMyPromo, bMyTextless;
    double dMySalePrice, dMyMarketPrice;
    static QMap<QString, unsigned int> qmTheStringIndex;

private:
    static bool InitOrderEstablished;
    static QVector<int> *pviTheFieldIndexes;
};

#endif // INVENTORYCARD_H
