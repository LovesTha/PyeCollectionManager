#ifndef ORACLECARD_H
#define ORACLECARD_H

#include <QString>

class OracleCard
{
public:
    quint64 iMultiverseID;
    QString sNameEn, sMySet, sMCISID, sSequenceNumber, sGSID;
    char cRarity;
    double dValue;
    static QString sImagePath;
    QString getImagePath() const;
    QString getImageURL() const;
    QString getLogoPath() const;
    QString getLogoURL() const;
    QString deckBoxInventoryLine(bool Foil) const;
};
#endif // ORACLECARD_H
