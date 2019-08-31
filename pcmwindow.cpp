#include "pcmwindow.h"
#include "ui_pcmwindow.h"
#include <QTcpSocket>
#include <QFile>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QtNetwork>
#include <map>
#include <QSettings>
#include "setchoice.h"

PCMWindow::PCMWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PCMWindow),
    bOracleVersionRetrieved(false),
    bNewOracleRetrieved(false)
{
    ui->setupUi(this);

    pMyTCPServer = new QTcpServer(this);
    pMyTCPServer->setMaxPendingConnections(99);
    if(!pMyTCPServer->listen(QHostAddress::Any, 8080))
    {
        ui->imageLabel->setText("Couldn't open port 8080, try restarting this application after you have changed something");
    }
    connect(pMyTCPServer, SIGNAL(newConnection()), SLOT(NewTCPConnection()));

    imageFetchManager = new QNetworkAccessManager();
    connect(imageFetchManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(ImageFetchFinished(QNetworkReply*)));
    oracleFetchManager= new QNetworkAccessManager();
    connect(oracleFetchManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(OracleFetchFinished(QNetworkReply*)));
    oracleVersionFetchManager = new QNetworkAccessManager();
    connect(oracleVersionFetchManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(OracleVersionFetchFinished(QNetworkReply*)));

    QCoreApplication::setOrganizationName("Pye");
    QCoreApplication::setOrganizationDomain("cerberos.id.au");
    QCoreApplication::setApplicationName("Collection Manager");

    setWindowTitle("Pye Collection Manager - 0.2-Alpha-r1");

    //Loading Settings
    QSettings Config;
    ui->cardDatabaseLocationLineEdit->setText(Config.value("Oracle Database Location", "/mtg/AllSetsArray.json").toString());
    ui->collectionOutputLineEdit->setText(Config.value("Collection Output", "/mtg/collection.csv").toString());
    ui->collectionSourceLineEdit->setText(Config.value("Collection Input", "/mtg/Inventory_User_2016.December.24.csv").toString());
    ui->deckBoxPriceInputLineEdit->setText(Config.value("Deckbox Price Database", "/mtg/AllTheCards_2016.December.24.csv").toString());
    ui->fullCardListLocationLineEdit->setText(Config.value("Full Card List for Deckbox Import", "/mtg/DeckBoxFullCardList.csv").toString());
    ui->imageLocationLineEdit->setText(Config.value("Image Storage Location", "/mtg/images").toString());
    ui->tradeOutputLineEdit->setText(Config.value("Pucatrade Trades Output", "/mtg/trades.csv").toString());
    ui->tradeValueThresholdDoubleSpinBox->setValue(Config.value("Trade Minimum Value", "0.10").toDouble());
    ui->quantityToKeepSpinBox->setValue(Config.value("Quantity To Keep", "4").toInt());
    ui->soundsLocationLineEdit->setText(Config.value("Sounds Location", "/mtg/sounds/").toString());
    ui->IsAPlayerCheckBox->setChecked(Config.value("Treat All Printings As a Single Card", "true").toBool());
    ui->cardsToKeepMoreCoppiesOfLineEdit->setText(Config.value("List of Cards to Keep Extra Coppies of", "/mtg/WhiteList.csv").toString());

    onStartOracleCheck();
    on_pbOpenDatabase_clicked();
    on_pbOpenCollection_clicked();
    on_pbOpenOutputs_clicked();
}

PCMWindow::~PCMWindow()
{
    //Saving Settings
    QSettings Config;
    Config.setValue("Oracle Database Location", ui->cardDatabaseLocationLineEdit->text());
    Config.setValue("Collection Output", ui->collectionOutputLineEdit->text());
    Config.setValue("Collection Input", ui->collectionSourceLineEdit->text());
    Config.setValue("Deckbox Price Database", ui->deckBoxPriceInputLineEdit->text());
    Config.setValue("Full Card List for Deckbox Import", ui->fullCardListLocationLineEdit->text());
    Config.setValue("Image Storage Location", ui->imageLocationLineEdit->text());
    Config.setValue("Pucatrade Trades Output", ui->tradeOutputLineEdit->text());
    Config.setValue("Trade Minimum Value", ui->tradeValueThresholdDoubleSpinBox->value());
    Config.setValue("Quantity To Keep", ui->quantityToKeepSpinBox->value());
    Config.setValue("Sounds Location", ui->soundsLocationLineEdit->text());
    Config.setValue("Treat All Printings As a Single Card", ui->IsAPlayerCheckBox->isChecked());
    Config.setValue("List of Cards to Keep Extra Coppies of", ui->cardsToKeepMoreCoppiesOfLineEdit->text());

    fMyTradesOutput.flush();
    if(fMyTradesOutput.device())
    {
        fMyTradesOutput.device()->close();
        delete fMyTradesOutput.device();
    }

    fMyCollectionOutput.flush();
    if(fMyCollectionOutput.device())
    {
        fMyCollectionOutput.device()->close();
        delete fMyCollectionOutput.device();
    }

    delete ui->imageLabel;
    delete pMyTCPServer;
    delete ui;
}

void PCMWindow::NewTCPConnection()
{
    while(pMyTCPServer->hasPendingConnections())
    {
        QTcpSocket * socket = pMyTCPServer->nextPendingConnection();
        connect(socket, SIGNAL(readyRead()), SLOT(ScryGlassRequestReceived()));
        connect(socket, SIGNAL(disconnected()), SLOT(TCPDisconnected()));
        StatusString(QString("New TCP Connection (%1:)").arg(socket->peerAddress().toString()).arg(socket->peerPort()));
    }
}

void PCMWindow::TCPDisconnected()
{
    StatusString("TCP Disconnected");
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    socket->deleteLater();
}

void PCMWindow::ScryGlassRequestReceived()
{
    if(ui->hlCardImageNSetChoice->count() > 1)
         ((SetChoice*)ui->hlCardImageNSetChoice->itemAt(1)->layout()->itemAt(0)->widget())->click();

    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    QByteArray buffer;
    while (socket->bytesAvailable() > 0)
    {
        buffer.append(socket->readAll());
    }
    QList<QByteArray> elements = buffer.split(' ');

    bool bValid = false;
    OracleCard card;

    while(!elements.isEmpty())
    {
        QString item = elements.takeFirst();
        if(qmOracle.size() > 100 && item.compare("GET",Qt::CaseInsensitive) == 0)
        {
            //yay it's a card request
            if(elements.isEmpty()) continue;

            QStringList parts = QString(elements.takeFirst()).split(QRegExp("[&=]+"));
            for(int i = 0; i + 1 < parts.length(); ++i)
            {
                QString piece = parts[i];
                if(piece == "multiverseid")
                {
                    bool bOk;
                    unsigned int multiverseID = parts[i+1].toInt(&bOk);
                    if(bOk && (multiverseID != LastCardMultiverse || !cardRepeatWindow))
                    {
                        LastCardMultiverse = multiverseID;
                        StartCardRepeatWindow(250); // Prevent accepting a scan of this card for 250ms
                        ++i; //the next bit was the MultiverseID, so we skip it later
                        StatusString(QString("Processing Multiverse ID: %1").arg(multiverseID));
                        card = qmOracle.value(multiverseID);
                        QString sImagePath = card.getImagePath();
                        QFileInfo ImageFileInfo(sImagePath);
                        if(ImageFileInfo.exists() && ImageFileInfo.isFile())
                        {
                            DisplayImage(sImagePath);
                        }
                        else
                        {
                            ui->imageLabel->clear();
                            ui->imageLabel->setText("Fetching Image...");
                            StatusString("Fetching Image");
                            imageFetchManager->get(QNetworkRequest(QUrl(card.getImageURL())));
                            lRequestedImages.append(card);
                        }
                        bValid = true;
                    }
                }
            }
        }
    }

    if(socket->isWritable())
    {
        socket->write(QString("HTTP/1.1 200 OK\n/*\
Date: %1\n\
Server: Apache/2.2.14 (Win32)\n\
Last-Modified: %1\n\
Connection: Keep-Alive\n\
Content-Length: 0\n\n\n*/").arg(QDateTime::currentDateTime().toString(Qt::ISODate)).toLocal8Bit());
    }

    socket->flush();

    if(bValid)
    {
        //having started attempting to display the image, lets try and figure out if this card has multiple printings
        QList<quint64> lPrintingIDs = qmmMultiInverse.values(card.sNameEn);
        if(lPrintingIDs.length() == 1)
            return HandleSingleCard(card);
        else if(lPrintingIDs.length() > 1)
            return HandleMultipleCards(card, lPrintingIDs);
        else
            return;
    }
    else if(!cardRepeatWindow)
    {
        ui->imageLabel->setText("Invalid request");
        StatusString("Unrecognised/duplicated (Delver) request", true);
    }
}

void PCMWindow::cardRepeatWindowFinish()
{
    cardRepeatWindow = false;
}

void PCMWindow::StartCardRepeatWindow(uint msecs)
{
    QTimer::singleShot(msecs, this, SLOT(cardRepeatWindowFinish()));
    cardRepeatWindow = true;
}

bool PCMWindow::Needed(OracleCard card, int *iRegCount, int *iFoilCnt)
{
    int iRegularCount = 0;
    int iFoilCount = 0;

    InventoryCard invRegularCard = qmMyRegularCollectorInventory.value(card.iMultiverseID, InventoryCard(0));
    InventoryCard invFoilCard    = qmMyFoilCollectorInventory   .value(card.iMultiverseID, InventoryCard(0));

    if(ui->IsAPlayerCheckBox)
    {
        //players don't care about printings, just about owning the card
        QList<quint64> lPrintingIDs = qmmMultiInverse.values(card.sNameEn);
        for(auto&& CardID: lPrintingIDs)
        {
            InventoryCard tmpRegularCard = qmMyRegularCollectorInventory.value(CardID, InventoryCard(0));
            InventoryCard tmpFoilCard    = qmMyFoilCollectorInventory   .value(CardID, InventoryCard(0));

            iRegularCount += tmpRegularCard.iMyCount;
            iFoilCount += tmpFoilCard.iMyCount;
        }
    }
    else
    {
        //collectors care about each individual printing seperately
        iRegularCount += invRegularCard.iMyCount;
        iFoilCount += invFoilCard.iMyCount;
    }

    if(iRegCount) *iRegCount = iRegularCount;
    if(iFoilCnt)   *iFoilCnt = iFoilCount;

    ui->lcdCollectionQuantity->display(isFoil() ? iFoilCount : iRegularCount);

    return (iFoilCount + iRegularCount < ui->quantityToKeepSpinBox->value()) //we don't have enough of any variant
            ||(isFoil() && iFoilCount < ui->quantityToKeepSpinBox->value()); //this is a foil, and we don't have enough foils
}

void PCMWindow::HandleSingleCard(OracleCard card)
{
    quint64 multiverseID = card.iMultiverseID;

    InventoryCard invRegularCard = qmMyRegularCollectorInventory.value(multiverseID, InventoryCard(0));
    InventoryCard invFoilCard    = qmMyFoilCollectorInventory   .value(multiverseID, InventoryCard(0));

    int iRegularCount = 0;
    int iFoilCount = 0;

//    InventoryCard priceCard = qmMyRegularPriceGuide.value(multiverseID, InventoryCard(0));

    if(Needed(card, &iRegularCount, &iFoilCount))
    {
        //insufficient cards not in inventory
        ui->cardAction->setText("KEEP!");
        StatusString(QString("Keep: %1").arg(invRegularCard.sMyName));
        mySound.setMedia(qKeep);
        mySound.play();
        if(isFoil())
        {
            invFoilCard.iMyCount++;
            qmMyFoilCollectorInventory.insert(multiverseID, invFoilCard);
        }
        else
        {
            invRegularCard.iMyCount++;
            qmMyRegularCollectorInventory.insert(multiverseID, invRegularCard);
        }

        fMyCollectionOutput << card.deckBoxInventoryLine(isFoil());
        fMyCollectionOutput.flush();
        return;
    }
    else if((card.dValue > ui->tradeValueThresholdDoubleSpinBox->value()) | isFoil())
    {
        ui->cardAction->setText("Trade");
        StatusString(QString("Trade: %1 ($%2)").arg(invRegularCard.sMyName).arg((isFoil() ? invFoilCard : invRegularCard).dMyMarketPrice));
        mySound.setMedia(qCoins);
        mySound.play();
        fMyTradesOutput << card.pucaInventoryLine(isFoil());
        fMyTradesOutput.flush();
    }
    else if(card.dValue < 0.001)
    {
        ui->cardAction->setText("No Price Data");
        StatusString(QString("No Price Data for: %1!").arg(invRegularCard.sMyName), true);
        mySound.setMedia(qWeird);
        mySound.play();
    }
    else
    {
        trash(invRegularCard.sMyName);
    }
}

void PCMWindow::trash(QString card)
{
    ui->cardAction->setText("Trash");
    StatusString(QString("Trash: %1").arg(card));
    mySound.setMedia(qTrash);
    mySound.play();
}

// Compare two Multiverse IDs.
bool quint64GreaterThan(const quint64 &v1, const quint64 &v2)
{
    return v1 > v2;
}
bool oracleCardGreaterThan(const OracleCard &v1, const OracleCard &v2)
{
    return v1.mySet->LastSelected > v2.mySet->LastSelected;
    return v1.iMultiverseID > v2.iMultiverseID;
}
void PCMWindow::HandleMultipleCards(OracleCard card, QList<quint64> lCardIDs)
{
    ui->cardAction->setText("Multiple Versions");
    ui->lcdCollectionQuantity->display(-1);
    //first we figure out if the price range falls within acceptable bounds for trashing (which we never do to foils and cards we want)
    if(!isFoil() && !Needed(card))
    {
        bool trashable = true;
        OracleCard priceCard;
        for(auto&& CardID: lCardIDs)
        {
            priceCard = qmOracle.value(CardID, OracleCard());

            if(priceCard.dValue < 0.001)
            {
                trashable = false;
                break; // we have no price data for this version, so we can't confidently trash it ever
            }
            if(priceCard.dValue > ui->tradeValueThresholdDoubleSpinBox->value())
            {
                trashable = false;
                break; // we have a valuable variant, so we can't confidently trash it ever
            }
        }

        if(trashable)
            return trash(priceCard.sNameEn);
    }

    //As we haven't been able to trash the card, now we must determine what set it is from
    QGridLayout *MySetSelectorLayout = new QGridLayout();

    //we want the list to sort most recent selected set first, then most recently printed.
    QList<OracleCard> OracleCards;
    for(auto&& CardID: lCardIDs)
    {
        OracleCard priceCard = qmOracle.value(CardID);
        OracleCards.append(priceCard);
    }
    qSort(OracleCards.begin(), OracleCards.end(), oracleCardGreaterThan);

    int count = 0;
    for(auto&& priceCard: OracleCards)
    {
        SetChoice *thisChoice = new SetChoice(priceCard, MySetSelectorLayout->widget());
        thisChoice->setParent(MySetSelectorLayout->widget());
        MySetSelectorLayout->addWidget((QWidget*)thisChoice, count % 10, count / 10);
        count++;
        connect(thisChoice, SIGNAL(clicked(bool)), this, SLOT(setSelected()));
    }

    ui->hlCardImageNSetChoice->addLayout(MySetSelectorLayout->layout());
}

void PCMWindow::setSelected()
{
    SetChoice *set = (SetChoice*)this->sender();
    OracleCard card = set->MyCard;
    card.mySet->LastSelected = QDateTime::currentDateTime(); //sort this to the top now.
    HandleSingleCard(card);

    CleanSetSelection();
}

void PCMWindow::CleanSetSelection()
{

    //set has been selected, clean up and remove the option to select a set
    QLayoutItem *item;
    //while ((item = ((QLayout*)set->parent())->takeAt(0)) != 0)
    while ((item = ui->hlCardImageNSetChoice->itemAt(1)->layout()->takeAt(0)) != 0)
    {
        item->widget()->deleteLater();
    }
    item = ui->hlCardImageNSetChoice->takeAt(1);
    item->layout()->deleteLater();
}

void PCMWindow::DisplayImage(QString sImagePath)
{
    QImage qImage(sImagePath);
    QPixmap image(QPixmap::fromImage(qImage));
    image = image.scaled(ui->imageLabel->size(),
                         Qt::AspectRatioMode::KeepAspectRatio,
                         Qt::SmoothTransformation);
    ui->imageLabel->setPixmap(image);
}

void PCMWindow::ImageFetchFinished(QNetworkReply* reply)
{
   //Check for errors first
   QImage* img2 = new QImage();
   img2->loadFromData(reply->readAll());

   QNetworkRequest thisRequest = reply->request();
   for(auto&& requestedCard: lRequestedImages)
   {
       if(QUrl(requestedCard.getImageURL()).toDisplayString() == thisRequest.url().toDisplayString())
       {
           if(img2->isNull())
           {
               ui->imageLabel->setText("Image failed to fetch");
               StatusString(QString("Failed to load image: %1 - %2").arg(requestedCard.getImageURL()).arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()), true);
           }
           else
           {
               img2->save(requestedCard.getImagePath(), "JPG", 99);
               DisplayImage(requestedCard.getImagePath());
           }
           lRequestedImages.removeOne(requestedCard);
           return;
       }
   }

   StatusString(QString("Unknown http response, not sure who made this request: %1").arg(thisRequest.url().toDisplayString()));
}

void PCMWindow::on_pbOpenCollection_clicked()
{
    qmMyRegularCollectorInventory.clear();
    qmMyFoilCollectorInventory.clear();
    qmMyRegularPriceGuide.clear();
    qmMyFoilPriceGuide.clear();

    StatusString("Attempting to load white list");
    LoadInventory(&qmMyRegularCollectorInventory, &qmMyFoilCollectorInventory, ui->cardsToKeepMoreCoppiesOfLineEdit->text(), true, true); //inventory as white list (second true is to invert the numbers, use negative stock to indicate we need more
    StatusString("Attempting to load previous run inventory in Deckbox format");
    LoadInventory(&qmMyRegularCollectorInventory, &qmMyFoilCollectorInventory, ui->collectionOutputLineEdit->text(), true); //inventory from previous runs
    StatusString("Attempting to load exported Deckbox Inventory");
    LoadInventory(&qmMyRegularCollectorInventory, &qmMyFoilCollectorInventory, ui->collectionSourceLineEdit->text(), true); //inventory that has been exported from Deckbox
    StatusString("Attempting to load exported Deckbox price list input");
    LoadInventory(&qmMyRegularPriceGuide, &qmMyFoilPriceGuide, ui->deckBoxPriceInputLineEdit->text(), false);
}

void PCMWindow::LoadInventory(QMap<quint64, InventoryCard>* qmRegularInventory,
                              QMap<quint64, InventoryCard>* qmFoilInventory,
                              QString sFileSource,
                              bool AddNotMax,
                              bool InvertValue)
{
    QFile fInput(sFileSource);
    QMap<quint64, InventoryCard>* qmRightInventory;
    if(fInput.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&fInput);
        QString line = in.readLine();
        InventoryCard::InitOrder(line);

        int countShort = 0;
        int countExtras = 0;

        while (!in.atEnd())
        {
            line = in.readLine();

            if(line.length() < 5)
            {
                countShort++;
                continue;
            }

            if(line.contains("Extras: "))
            {
                countExtras++;
                continue;
            }

            InventoryCard card(line);
            if(InvertValue)
                card.iMyCount = 0 - card.iMyCount;

            qmRightInventory = card.bMyFoil ? qmFoilInventory : qmRegularInventory;
            QList<quint64> viMultiverseIDs = qmmMultiInverse.values(card.sMyName);
            if(viMultiverseIDs.length() == 0)
            {
                StatusString(QString("No Multiverse ID for card from string \"%1\"").arg(line), true);
                continue; //we can't handle things that have no multiverse ID
            }

            for(auto&& iMultiverseID: viMultiverseIDs)
            {
                if(qmRightInventory->contains(iMultiverseID))
                {
                    if(AddNotMax)
                        card.iMyCount = qmRightInventory->value(iMultiverseID, InventoryCard(0)).iMyCount + card.iMyCount;
                    else
                        card.dMyMarketPrice = std::max(qmRightInventory->value(iMultiverseID, InventoryCard(0)).dMyMarketPrice, card.dMyMarketPrice);

                    qmRightInventory->insert(iMultiverseID, card);
                }
                else
                {
                    qmRightInventory->insert(iMultiverseID, card);
                }
            }
        }

        StatusString(QString("Skipped %1 tokens and %2 short lines").arg(countExtras).arg(countShort));
        StatusString(QString("Read %1 regular cards").arg(qmRegularInventory->size()));
        StatusString(QString("Read %1 foil cards").arg(qmFoilInventory->size()));
    }
    else
    {
        StatusString(QString("Collection or Pricelist not loaded, does the file exist? (%1)")
                     .arg(sFileSource), true);
    }
}


void PCMWindow::onStartOracleCheck()
{
    QString sOracleVersionLocation("http://mtgjson.com/json/version.json"); //as a quoted string, eg: "3.8"

    oracleVersionFetchManager->get(QNetworkRequest(QUrl(sOracleVersionLocation)));
    StatusString(QString("Fetching Oracle Version from %1").arg(sOracleVersionLocation));
}

void PCMWindow::recordOracleVersion()
{
    QSettings Config;
    Config.setValue("Oracle Version", sMyOracleVersion);
}

int cmpVersion(const char *v1, const char *v2)
{
    int oct_v1[3], oct_v2[3];
    memset(&oct_v1, 0, sizeof(int) * 3);
    memset(&oct_v2, 0, sizeof(int) * 3);
    sscanf(v1, "%d.%d.%d", &oct_v1[0], &oct_v1[1], &oct_v1[2]);
    sscanf(v2, "%d.%d.%d", &oct_v2[0], &oct_v2[1], &oct_v2[2]);

    for (int i = 0; i < 3; i++) {
        if (oct_v1[i] > oct_v2[i])
            return 1;
        else if (oct_v1[i] < oct_v2[i])
            return -1;
    }

    return 0;
}

void PCMWindow::OracleVersionFetchFinished(QNetworkReply* reply)
{
    QString sReply = reply->readAll().replace("\"","");

    QSettings Config;
    QString sVersionLastSaved = Config.value("Oracle Version", "0.0").toString();

    StatusString(QString("Server version is %1, our saved version is %2")
                 .arg(sReply)
                 .arg(sVersionLastSaved));

    sMyOracleVersion = sReply;
    bOracleVersionRetrieved = true;

    bNewOracleRetrieved = (cmpVersion(sReply.toLatin1(), sVersionLastSaved.toLatin1()) > 0);

    if(bNewOracleRetrieved)
    {
        recordOracleVersion();
        on_FetchOracle_clicked();
    }
}

void PCMWindow::on_FetchOracle_clicked()
{
    QString sOracleLocation("http://mtgjson.com/json/AllSetsArray.json");

    oracleFetchManager->get(QNetworkRequest(QUrl(sOracleLocation)));
    StatusString("Fetching new Oracle");
}

void PCMWindow::OracleFetchFinished(QNetworkReply *reply)
{
    QFile fOracle(ui->cardDatabaseLocationLineEdit->text());
    if(fOracle.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&fOracle);
        out << reply->readAll();
        out.flush();

        StatusString("New Oracle retrieved and saved");

        bNewOracleRetrieved = true;

        if(bOracleVersionRetrieved)
            recordOracleVersion();

        on_pbOpenDatabase_clicked();
    }
    else
    {
        StatusString(QString("Failed to save new Oracle to file: %1").arg(ui->cardDatabaseLocationLineEdit->text()), true);
    }
}

void PCMWindow::on_pbOpenDatabase_clicked()
{
    qmmMultiInverse.clear();
    qmOracle.clear();

    OracleCard::sImagePath = ui->imageLocationLineEdit->text();
    OracleSet::sImagePath = ui->imageLocationLineEdit->text();

    QFile fInput(ui->cardDatabaseLocationLineEdit->text());
    if(fInput.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        unsigned int iNoMID = 0, iNoMCID = 0;

        QJsonDocument jdoc = QJsonDocument::fromJson(fInput.readAll(), &jError);

        if(jError.error != QJsonParseError::NoError)
            ui->imageLabel->setText(QObject::tr("Not a json document"));

//        QString tmp = jError.errorString();

        QJsonArray jSets = jdoc.array();

        for(auto&& set: jSets)
        {
            QJsonObject jSet = set.toObject();
            QJsonArray jCards = jSet["cards"].toArray();

            QString sSetCode;
            sSetCode = jSet["code"].toString();

            OracleSet *ThisSet = new OracleSet;
            ThisSet->sMySet = jSet["name"].toString();
            ThisSet->sSetCode = sSetCode;
            ThisSet->LastSelected = QDateTime::currentDateTime();


            for(auto&& cardIter: jCards)
            {
                QJsonObject jCard = cardIter.toObject();
                OracleCard card;

                if(jCard["multiverseId"].isNull())
                    iNoMID++;
                unsigned int iID = jCard["multiverseId"].toInt();
                card.iMultiverseID = iID;

                if(iID == 0) //some sets, like colectors edition don't have multiverse IDs, we can't use them.
                    continue;

                QString sName = jCard["name"].toString();
                card.sNameEn = sName;

                if(jCard["names"].isArray())
                {
                    QJsonArray names = jCard["names"].toArray();
                    if(names.size() == 2)
                    {
                        if(jCard["layout"].toString().contains("split") | jCard["layout"].toString().contains("aftermath")) //BFG
                        {
                            card.sNameEn = names.at(0).toString() + " // " + names.at(1).toString();
                        }
                    }
                    if(names.size() > 3)
                        continue;
                }

                if(jCard["variations"].isArray())
                    card.iArtVersions = jCard["variations"].toArray().size() + 1;
                else
                    card.iArtVersions = 1;

                card.sNameEn = card.sNameEn.replace("ö", "o");
                if(card.sNameEn.at(0) == '"')
                {
                    card.sNameEn = card.sNameEn.remove(0, 1);
                    card.sNameEn = card.sNameEn.remove(card.sNameEn.length() - 1, 1);
                }

                if(jCard["number"].isNull())
                    iNoMCID++;
                else
                    card.sSequenceNumber = jCard["number"].toString();

                if(card.sSequenceNumber.contains("b") | jCard["mciNumber"].toString().contains("b"))
                    continue; //second half of a card, not worth having around (messes up DeckBox output)

                if(!jCard["side"].isNull() && !jCard["side"].toString().contains("a"))
                    continue; // second or third side of a flip, split, or transform card.

                card.cRarity = jCard["rarity"].toString().at(0).toLatin1();
                card.mySet = ThisSet;

                double value = 0;
                if( jCard["prices"].isObject() )
                {
                    QJsonObject prices = jCard["prices"].toObject();
                    if(prices["paper"].isObject())
                    {
                        QJsonObject paperPrices = prices["paper"].toObject();
                        QDate date = QDateTime::currentDateTime().date();
                        QDate limit = date.addDays(-1000);
                        date = date.addDays(-1);
                        for( ; date > limit; )
                        {
                            QString sDate = QString("%1-%2-%3")
                                    .arg(date.year(), 4,10,QLatin1Char('0'))
                                    .arg(date.month(),2,10,QLatin1Char('0'))
                                    .arg(date.day(),  2,10,QLatin1Char('0'));
                            if(paperPrices[sDate].isDouble())
                            {
                                value = paperPrices[sDate].toDouble();
                                date = date.addYears(-10); // End the loop
                            }
                            date = date.addDays(-1);
                        }
                    }
                }
                card.dValue = value;

                qmmMultiInverse.insert(card.sNameEn, iID); //note this is now a Multi Map, this insert will never replace
                qmOracle.insert(iID, card);
            }
        }
    }
}

void PCMWindow::on_pbFullCardListDB_clicked()
{
    unsigned int i = 999999;
    QTextStream fFullCardListOutputDeckBox;
    QTextStream fFullCardListOutputPuca;
    QFile *fullCardListFileDeckBox;
    QFile *fullCardListFilePuca;
    for(auto&& pair: qmOracle.toStdMap())
    {
        if(i++ > 90000) //hard coded pagination, which is commented out
        {
            QString location = ui->fullCardListLocationLineEdit->text();
            fFullCardListOutputDeckBox.flush();
            fullCardListFileDeckBox = new QFile(location.insert(location.lastIndexOf('.'),".deckbox"));// + append++); //this little append bit is the pagination being commented out
            fullCardListFileDeckBox->open(QIODevice::WriteOnly | QIODevice::Text);
            fFullCardListOutputDeckBox.setDevice(fullCardListFileDeckBox);
            fFullCardListOutputDeckBox << OracleCard::DeckBoxHeader;

            location = ui->fullCardListLocationLineEdit->text();
            fullCardListFilePuca = new QFile(location.insert(location.lastIndexOf('.'),".puca"));// + append++); //this little append bit is the pagination being commented out
            fullCardListFilePuca->open(QIODevice::WriteOnly | QIODevice::Text);
            fFullCardListOutputPuca.setDevice(fullCardListFilePuca);
            fFullCardListOutputPuca << OracleCard::PucaHeader;
            i = 0;
        }
        OracleCard card = pair.second;
        if(card.sSequenceNumber.contains("b"))
            continue; //Deckbox doesn't want the second half of split cards
        if(card.sNameEn.contains(QString("token"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we would generate tokens
        if(card.sNameEn.contains(QString("Big Furry Monster"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we generate this name
        if(card.mySet->sMySet.contains(QString("Tempest Remastered"), Qt::CaseInsensitive))
            continue; //Not real cards
        if(card.mySet->sMySet.contains(QString("Vintage Masters"), Qt::CaseInsensitive))
            continue; //Not real cards
        if(card.mySet->sMySet.contains(QString("Vanguard"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we would Avatars
        if(card.mySet->sMySet.contains(QString("Masters Edition"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we would Avatars
        fFullCardListOutputDeckBox << card.deckBoxInventoryLine(false);
        fFullCardListOutputDeckBox << card.deckBoxInventoryLine(true);

        if((card.sNameEn == "Plains")
                | (card.sNameEn == "Island")
                | (card.sNameEn == "Swamp")
                | (card.sNameEn == "Mountain")
                | (card.sNameEn == "Forest")
                | (card.iArtVersions > 1))
            continue; //Puca doesn't like cards with multiple arts, at least not easily
        fFullCardListOutputPuca << card.pucaInventoryLine(false);
        fFullCardListOutputPuca << card.pucaInventoryLine(true);
    }

    fFullCardListOutputDeckBox.flush();
    fFullCardListOutputDeckBox.device()->close();
    fFullCardListOutputDeckBox.device()->deleteLater();
    fFullCardListOutputPuca.flush();
    fFullCardListOutputPuca.device()->close();
    fFullCardListOutputPuca.device()->deleteLater();
}

void PCMWindow::on_pbOpenOutputs_clicked()
{
    if(fMyCollectionOutput.device())
    {
        fMyCollectionOutput.flush();
        fMyCollectionOutput.device()->close();
        delete fMyCollectionOutput.device();
    }

    if(fMyTradesOutput.device())
    {
        fMyTradesOutput.flush();
        fMyTradesOutput.device()->close();
        delete fMyTradesOutput.device();
    }

    QFileInfo tradesFileInfo(ui->tradeOutputLineEdit->text());
    bool writeHeader = !tradesFileInfo.exists();
    QFile *tradeFile = new QFile(ui->tradeOutputLineEdit->text());
    if(tradeFile->open(QIODevice::Text | QIODevice::Append))
    {
        fMyTradesOutput.setDevice(tradeFile);
        if(writeHeader)
            fMyTradesOutput << OracleCard::PucaHeader;
        fMyTradesOutput.flush();
    }
    else
    {
        StatusString(QString("Could not open output file to store trades: %1")
                     .arg(ui->tradeOutputLineEdit->text()), true);
    }

    QFileInfo collectionFileInfo(ui->collectionOutputLineEdit->text());
    writeHeader = !collectionFileInfo.exists();
    QFile *collectionFile = new QFile(ui->collectionOutputLineEdit->text());
    if(collectionFile->open(QIODevice::Text | QIODevice::Append))
    {
        fMyCollectionOutput.setDevice(collectionFile);
        if(writeHeader)
            fMyCollectionOutput << OracleCard::DeckBoxHeader;
        fMyCollectionOutput.flush();
    }
    else
    {
        StatusString(QString("Could not open output file to store collection: %1")
                     .arg(ui->collectionOutputLineEdit->text()), true);
    }
}

void PCMWindow::StatusString(QString sMessage, bool bError)
{
    ui->teStatus->append(QString("<font color=\"%1\">%2</font>\r\n")
                             .arg(bError ? "red" : "black")
                             .arg(sMessage));

    if(bError)
        ui->statusBar->showMessage(sMessage);
}

void PCMWindow::on_soundsLocationLineEdit_textChanged(const QString &arg1)
{
    if(arg1.lastIndexOf('/') == arg1.length() - 1)
    {
        qTrash = QUrl::fromLocalFile(arg1 + "trashcan.wav");
        qKeep = QUrl::fromLocalFile(arg1 + "lock.wav");
        qCoins = QUrl::fromLocalFile(arg1 + "coins.wav");
        qWeird = QUrl::fromLocalFile(arg1 + "weird.wav");
    }
    else
    {
        //ui->
    }
}

bool PCMWindow::isFoil()
{
    return ui->cbScanningFoils->isChecked();
}

