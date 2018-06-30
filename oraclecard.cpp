#include "oraclecard.h"

#include <QDir>

QString OracleCard::sImagePath = "/tmp/";
QString OracleSet::sImagePath = "/tmp/";

QString OracleCard::getImagePath() const
{
    if(mySet)
    {
        QDir path(QString("%1/%2").arg(sImagePath).arg(mySet->sMCISID));
        path.mkpath(".");
        return QString("%3/%1/%2.jpg").arg(mySet->sMCISID).arg(sSequenceNumber).arg(sImagePath);
    }
    else
    {
        return "";
    }
}

QString OracleCard::getImageURL() const
{
    return QString("http://gatherer.wizards.com/Handlers/Image.ashx?multiverseid=%1&type=card").arg(iMultiverseID);
}

QString OracleSet::getLogoPath(char cRarity) const
{
    QDir path(QString("%1/%2").arg(sImagePath).arg(sMCISID));
    path.mkpath(".");
    return QString("%3/%1/%2.jpg").arg(sMCISID).arg(cRarity).arg(sImagePath); //path makes sense because it will also have all the card images in that folder, not just the few rarities
}

QString OracleSet::getLogoURL(char cRarity) const
{
    return QString("http://gatherer.wizards.com/Handlers/Image.ashx?type=symbol&set=%1&size=large&rarity=%2").arg(sGSID).arg(cRarity);
}

QString OracleCard::PucaHeader("Count,Name,Expansion,Condition,Language\n");
QString OracleCard::pucaInventoryLine(bool Foil) const
{
    QString sInventory("1,\"%1\",\"%2\",Near Mint,English,%3\n");
    sInventory = sInventory.arg(sNameEn).arg(mySet->sMySet);
    sInventory = sInventory.arg(Foil ? "Foil" : "");
    return sInventory;
}

QString OracleCard::DeckBoxHeader("Count,Tradelist Count,Name,Edition,Card Number,Condition,Language,Foil,Signed,Artist Proof,Altered Art,Misprint,Promo,Textless,My Price,\n");
QString OracleCard::deckBoxInventoryLine(bool Foil) const
{
    QString sInventory("1,0,%1,%2,%3,Near Mint,English,%4,,,,,,,,\n");
    QString sName = sNameEn;
    if(sName.at(0) != '\"') //only if it doesn't start with a " already
        sName = sName.replace(QRegExp("\""), "\"\""); //some names include " in them, deckbox wants those replaced with a pair of ""
    //sName = sName.replace(QRegExp("[\?\!]"), "");
    if(sName == "Kill! Destroy!")
        sName = "Kill Destroy"; //deckbox stripped the ! from this name, but not others
    if(sName.at(0) != '\"') //only if it doesn't start with a " already
        sName = QString("\"") + sName + "\"";

    QString sSetLocal = this->mySet->sMySet;
    if(sSetLocal == QString("Planechase 2012 Edition"))
        sSetLocal = "Planechase 2012";
    if(sSetLocal == QString("Magic: The Gathering-Commander"))
        sSetLocal = "Commander";
    if(sSetLocal == QString("Commander 2013 Edition"))
        sSetLocal = "Commander 2013";
    if(sSetLocal == QString("Magic: The Gathering—Conspiracy"))
        sSetLocal = "Conspiracy";
    if(sSetLocal == QString("Modern Masters Edition"))
        sSetLocal = "Modern Masters";
    if(sSetLocal.contains("Masterpiece Series"))
        sSetLocal = sSetLocal.replace("Masterpiece Series: ", "");
    sSetLocal == sSetLocal.replace(QRegExp(" \\([0-9][0-9][0-9][0-9]\\)"), "");
    if(sSetLocal.at(0) != '\"') //only if it doesn't start with a " already
        sSetLocal = sSetLocal.replace(QRegExp("\""), "\"\""); //some names include " in them, deckbox wants those replaced with a pair of ""
    sSetLocal = QString("\"") + sSetLocal + "\"";

    sInventory = sInventory.arg(sName).arg(sSetLocal).arg(this->sSequenceNumber).arg(Foil ? "Foil" : ""); //arg 4 is Foil or blank
    return sInventory;
}

bool OracleCard::operator==(const OracleCard& rhs) const
{
    return this->iMultiverseID == rhs.iMultiverseID;
}
