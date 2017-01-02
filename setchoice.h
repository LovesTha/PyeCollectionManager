#ifndef SETCHOICE_H
#define SETCHOICE_H

#include <QPushButton>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include "oraclecard.h"

class SetChoice : public QPushButton
{
    Q_OBJECT
public:
    explicit SetChoice(OracleCard Card, QWidget *parent = 0);
    OracleCard MyCard;

signals:

public slots:
    void ImageFetchFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager *manager;
    QString sMyImageRequested;
    void DisplayImage(QString sImagePath);
};

#endif // SETCHOICE_H
