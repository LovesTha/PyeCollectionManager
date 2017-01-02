#include "inventorycard.h"

#include <QMap>
#include <QVector>

QVector<int> *InventoryCard::pviTheFieldIndexes = 0;
bool InventoryCard::InitOrderEstablished = false;
QMap<QString, unsigned int> InventoryCard::qmTheStringIndex;

void InventoryCard::InitOrder(QString sInitLine)
{
    if(pviTheFieldIndexes == 0) pviTheFieldIndexes = new QVector<int>;
    for(int i = 0; i < 17; ++i) pviTheFieldIndexes->append(-1);

    QStringList elements = sInitLine.split(",");
    for(unsigned int iIndex = 0; !elements.isEmpty(); ++iIndex)
    {
        QString item = elements.takeFirst();
        if(item.compare("Count",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(0, iIndex);
        }
        else if(item.compare("Tradelist Count",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(1, iIndex);
        }
        else if(item.compare("Name",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(2, iIndex);
        }
        else if(item.compare("Edition",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(3, iIndex);
        }
        else if(item.compare("Card Number",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(4, iIndex);
        }
        else if(item.compare("Condition",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(5, iIndex);
        }
        else if(item.compare("Language",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(6, iIndex);
        }
        else if(item.compare("Foil",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(7, iIndex);
        }
        else if(item.compare("Signed",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(8, iIndex);
        }
        else if(item.compare("Artist Proof",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(9, iIndex);
        }
        else if(item.compare("Altered Art",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(10, iIndex);
        }
        else if(item.compare("Misprint",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(11, iIndex);
        }
        else if(item.compare("Promo",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(12, iIndex);
        }
        else if(item.compare("Textless",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(13, iIndex);
        }
        else if(item.compare("My Price",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(14, iIndex);
        }
        else if(item.compare("Rarity",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(15, iIndex);
        }
        else if(item.compare("Price",Qt::CaseInsensitive) == 0)
        {
            pviTheFieldIndexes->replace(16, iIndex);
        }
    }

    InitOrderEstablished = true;
}

InventoryCard::InventoryCard(int Quantity)
{
    iMyCount = Quantity;
    iMyTradelistCount = Quantity;
    iMyCardNumber = -1;
    sMyName = "";
    sMyEdition = "";
    sMyCondition = "";
    sMyLanguage = "";
    sMyRarity = "";
    bMyFoil = false;
    bMySigned = false;
    bMyArtistProof = false;
    bMyAlteredArt = false;
    bMyMisprint = false;
    bMyPromo = false;
    bMyTextless = false;
    dMySalePrice = -1;
    dMyMarketPrice = -1;
}

InventoryCard::InventoryCard(QString sInitLine) : InventoryCard(-1)
{
    QStringList elements = sInitLine.split(",");
    if(elements.size() < 17)
        return;

    if(!InitOrderEstablished)
        InitOrder(""); //will create the default order;

    if(pviTheFieldIndexes->at(0) > -1)
        iMyCount = elements.at(pviTheFieldIndexes->at(0)).toInt();
    else
        iMyCount = 0;

    if(pviTheFieldIndexes->at(1) > -1)
        iMyTradelistCount = elements.at(pviTheFieldIndexes->at(1)).toInt();
    else
        iMyTradelistCount = 0;

    if(pviTheFieldIndexes->at(4) > -1)
        iMyCardNumber = elements.at(pviTheFieldIndexes->at(4)).toInt();
    else
        iMyCardNumber = 0;

    if(pviTheFieldIndexes->at(16) > -1)
    {
        QString sTmp = elements.at(pviTheFieldIndexes->at(16));
        dMyMarketPrice =  sTmp.replace(QRegExp("\\$"),"").toFloat();
    }
    else
        dMyMarketPrice = 0;

    if(pviTheFieldIndexes->at(14) > -1)
        dMySalePrice = elements.at(pviTheFieldIndexes->at(14)).toFloat();
    else
        dMySalePrice = 0;

    if(pviTheFieldIndexes->at(2) > -1)
        sMyName      = elements.at(pviTheFieldIndexes->at(2));
    else
        dMyMarketPrice = 0;

    if(pviTheFieldIndexes->at(3) > -1)
        sMyEdition   = elements.at(pviTheFieldIndexes->at(3));
    else
        sMyEdition = "Unknown";
    if(pviTheFieldIndexes->at(5) > -1)
        sMyCondition = elements.at(pviTheFieldIndexes->at(5));
    else
        sMyCondition = "Near Mint";
    if(pviTheFieldIndexes->at(6) > -1)
        sMyLanguage  = elements.at(pviTheFieldIndexes->at(6));
    else
        sMyLanguage = "English";

    if(pviTheFieldIndexes->at(15) > -1)
        sMyRarity    = elements.at(pviTheFieldIndexes->at(15));
    else
        sMyRarity = "Special";

    if(pviTheFieldIndexes->at(7) > -1)
        bMyFoil        = !elements.at(pviTheFieldIndexes->at(7)).isEmpty();
    else
        bMyFoil = false;

    if(pviTheFieldIndexes->at(8) > -1)
        bMySigned      = !elements.at(pviTheFieldIndexes->at(8)).isEmpty();
    else
        bMySigned = false;
    if(pviTheFieldIndexes->at(9) > -1)
        bMyArtistProof = !elements.at(pviTheFieldIndexes->at(9)).isEmpty();
    else
        bMyArtistProof = false;

    if(pviTheFieldIndexes->at(10) > -1)
        bMyAlteredArt  = !elements.at(pviTheFieldIndexes->at(10)).isEmpty();
    else
        bMyAlteredArt = false;

    if(pviTheFieldIndexes->at(11) > -1)
        bMyMisprint    = !elements.at(pviTheFieldIndexes->at(11)).isEmpty();
    else
        bMyMisprint = false;

    if(pviTheFieldIndexes->at(12) > -1)
        bMyPromo       = !elements.at(pviTheFieldIndexes->at(12)).isEmpty();
    else
        bMyPromo = false;

    if(pviTheFieldIndexes->at(13) > -1)
        bMyTextless    = !elements.at(pviTheFieldIndexes->at(13)).isEmpty();
    else
        bMyTextless = false;

}
