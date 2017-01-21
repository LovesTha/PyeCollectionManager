#ifndef ORACLECARD_H
#define ORACLECARD_H

#include <QString>
#include <QDateTime>

class OracleSet
{
public:
    QString sMySet, sMCISID, sGSID;
    QString getLogoPath(char cRarity) const;
    QString getLogoURL(char cRarity) const;
    QDateTime LastSelected;
    static QString sImagePath;
};

class OracleCard
{
public:
    OracleSet *mySet;
    quint64 iMultiverseID;
    QString sNameEn, sSequenceNumber;
    char cRarity;
    double dValue;
    static QString sImagePath;
    QString getImagePath() const;
    QString getImageURL() const;
    QString pucaInventoryLine(bool boolFoil) const;
    QString deckBoxInventoryLine(bool Foil) const;
    bool operator==(const OracleCard& rhs) const;

    static QString DeckBoxHeader;
    static QString PucaHeader;
};
#endif // ORACLECARD_H
