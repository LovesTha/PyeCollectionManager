#include "setchoice.h"
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QPixmap>

SetChoice::SetChoice(OracleCard Card, QWidget *parent) : QPushButton(parent), MyCard(Card), manager(0)
{


    setText(MyCard.sMySet);
    QString sIconLocation = MyCard.getLogoPath();
    QString sIconPath = MyCard.getImagePath();
    QFileInfo ImageFileInfo(sIconPath);
    if(ImageFileInfo.exists() && ImageFileInfo.isFile())
    {
        DisplayImage(sIconPath);
    }
    else
    {
        setIcon(QIcon());
        manager = new QNetworkAccessManager();
        connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(ImageFetchFinished(QNetworkReply*)));
        manager->get(QNetworkRequest(QUrl(MyCard.getLogoURL())));
        sMyImageRequested = sIconPath;
    }
}

void SetChoice::DisplayImage(QString sImagePath)
{
    QImage qImage(sImagePath);
    QPixmap image(QPixmap::fromImage(qImage));
    image = image.scaled(120, 120,
                         Qt::AspectRatioMode::KeepAspectRatio,
                         Qt::SmoothTransformation);
    setIcon(image);
    setIconSize(QSize(120, 120));
}

void SetChoice::ImageFetchFinished(QNetworkReply* reply)
{
   //Check for errors first
   QImage* img2 = new QImage();
   img2->loadFromData(reply->readAll());

   if(img2->isNull())
   {
       return;
   }

   img2->save(sMyImageRequested, "JPG", 99);
   DisplayImage(sMyImageRequested);
}
