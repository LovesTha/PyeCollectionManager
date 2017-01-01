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

PCMWindow::PCMWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PCMWindow)
{
    ui->setupUi(this);

    pMyTCPServer = new QTcpServer(this);
    pMyTCPServer->setMaxPendingConnections(99);
    if(!pMyTCPServer->listen(QHostAddress::Any, 8080))
    {
        ui->imageLabel->setText("Couldn't open port 8080, try restarting this application after you have changed something");
    }
    connect(pMyTCPServer, SIGNAL(newConnection()), SLOT(NewTCPConnection()));

    manager = new QNetworkAccessManager();
    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(ImageFetchFinished(QNetworkReply*)));

    QCoreApplication::setOrganizationName("Pye");
    QCoreApplication::setOrganizationDomain("cerberos.id.au");
    QCoreApplication::setApplicationName("Collection Manager");

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

    on_pbOpenDatabase_clicked();
    on_pbOpenOutputs_clicked();
    on_pbOpenCollection_clicked();
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
        connect(socket, SIGNAL(readyRead()), SLOT(TCPSocketReadReady()));
        connect(socket, SIGNAL(disconnected()), SLOT(TCPDisconnected()));
        StatusString(QString("New TCP Connection (%1)").arg(socket->peerAddress().toString()));
    }
}

void PCMWindow::TCPDisconnected()
{
    StatusString("TCP Disconnected");
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    socket->deleteLater();
}

void PCMWindow::TCPSocketReadReady()
{
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    QByteArray buffer;
    while (socket->bytesAvailable() > 0)
    {
        buffer.append(socket->readAll());
    }
    QList<QByteArray> elements = buffer.split(' ');

    while(!elements.isEmpty())
    {
        QString item = elements.takeFirst();
        if(qmOracle.size() > 100 && item.compare("GET",Qt::CaseInsensitive) == 0)
        {
            //yay it's a card request
            if(elements.isEmpty()) continue;

            QList<QByteArray> request = elements.takeFirst().split('=');
            bool bOk;
            unsigned int multiverseID = request.takeLast().toInt(&bOk);
            if(bOk)
            {
                StatusString(QString("Processing Multiverse ID: %1").arg(multiverseID));
                OracleCard card = qmOracle.value(multiverseID);
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
                    manager->get(QNetworkRequest(QUrl(card.getImageURL())));
                    sMyImageRequested = sImagePath;
                }

                InventoryCard invRegularCard = qmMyRegularInventory.value(multiverseID, InventoryCard(0));
                InventoryCard invFoilCard    = qmMyFoilInventory   .value(multiverseID, InventoryCard(0));
                InventoryCard priceCard = qmMyRegularPriceGuide.value(multiverseID, InventoryCard(-1));
                bool isFoil = ui->cbScanningFoils->isChecked();
                ui->lcdCollectionQuantity->display((isFoil ? invFoilCard : invRegularCard).iMyCount);

                if((invFoilCard.iMyCount + invRegularCard.iMyCount < ui->quantityToKeepSpinBox->value()) //we don't have enough of any variant
                  ||(isFoil && invFoilCard.iMyCount < ui->quantityToKeepSpinBox->value())) //this is a foil, and we don't have enough foils
                {
                    //card not in inventory
                    ui->cardAction->setText("KEEP!");
                    StatusString(QString("Keep: %1").arg(invRegularCard.sMyName));
                    mySound.setMedia(QUrl::fromLocalFile("/home/gareth/PyeCollectionManager/lock.wav"));
                    mySound.play();
                    if(isFoil)
                    {
                        invFoilCard.iMyCount++;
                        qmMyFoilInventory.insert(multiverseID, invFoilCard);
                    }
                    else
                    {
                        invRegularCard.iMyCount++;
                        qmMyRegularInventory.insert(multiverseID, invRegularCard);
                    }

                    fMyCollectionOutput << card.deckBoxInventoryLine(isFoil);
                    fMyCollectionOutput.flush();
                }
                else if((priceCard.dMyMarketPrice > ui->tradeValueThresholdDoubleSpinBox->value()) | isFoil)
                {
                    ui->cardAction->setText("TRADE!");
                    StatusString(QString("Trade: %1 ($%2)").arg(invRegularCard.sMyName).arg((isFoil ? invFoilCard : invRegularCard).dMyMarketPrice));
                    mySound.setMedia(QUrl::fromLocalFile("../PyeCollectionManager/coins.wav"));
                    mySound.play();
                    fMyTradesOutput << "1,\"" << card.sNameEn << "\",\"" << card.sMySet << "\",Near Mint,English,";
                    if(isFoil)
                        fMyTradesOutput << "Foil";
                    fMyTradesOutput << "\n";
                    fMyTradesOutput.flush();
                }
                else if(priceCard.dMyMarketPrice < 0.001)
                {
                    ui->cardAction->setText("No Price Data");
                    StatusString(QString("No Price Data for: %1!").arg(invRegularCard.sMyName), true);
                    mySound.setMedia(QUrl::fromLocalFile("../PyeCollectionManager/weird.wav"));
                    mySound.play();
                }
                else
                {
                    ui->cardAction->setText("trash");
                    StatusString(QString("Trash: %1").arg(invRegularCard.sMyName));
                    mySound.setMedia(QUrl::fromLocalFile("../PyeCollectionManager/trashcan.wav"));
                    mySound.play();
                }
            }
            else
            {
                ui->imageLabel->setText("Invalid request");                
                StatusString("Unrecognised request", true);
            }
        }
    }
}

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

    QString sSetLocal = this->sMySet;
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
    sSetLocal == sSetLocal.replace(QRegExp(" \\([0-9][0-9][0-9][0-9]\\)"), "");
    if(sSetLocal.at(0) != '\"') //only if it doesn't start with a " already
        sSetLocal = sSetLocal.replace(QRegExp("\""), "\"\""); //some names include " in them, deckbox wants those replaced with a pair of ""
    sSetLocal = QString("\"") + sSetLocal + "\"";

    sInventory = sInventory.arg(sName).arg(sSetLocal).arg(this->sSequenceNumber).arg(Foil ? "Foil" : ""); //arg 4 is Foil or blank
    return sInventory;
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

   if(img2->isNull())
   {
       ui->imageLabel->setText("Image failed to fetch");
       StatusString(QString("Failed to load image: %1").arg(sMyImageRequested), true);
   }

   img2->save(sMyImageRequested, "JPG", 99);
   DisplayImage(sMyImageRequested);
}

QVector<int> *InventoryCard::pviTheFieldIndexes = 0;
bool InventoryCard::InitOrderEstablished = false;
QMap<QString, unsigned int> InventoryCard::qmTheStringIndex;
QString OracleCard::sImagePath = "/tmp/";

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

    QString sTmp;

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

void PCMWindow::on_pbOpenCollection_clicked()
{
    qmMyRegularInventory.clear();
    qmMyFoilInventory.clear();
    qmMyRegularPriceGuide.clear();
    qmMyFoilPriceGuide.clear();

    StatusString("Attempting to load exported Deckbox Inventory");
    LoadInventory(&qmMyRegularInventory, &qmMyFoilInventory, ui->collectionSourceLineEdit->text(), true); //inventory that has been exported from Deckbox
    StatusString("Attempting to load previous run inventory in Deckbox format");
    LoadInventory(&qmMyRegularInventory, &qmMyFoilInventory, ui->collectionOutputLineEdit->text(), true); //inventory from previous runs
    StatusString("Attempting to load exported Deckbox price list input");
    LoadInventory(&qmMyRegularPriceGuide, &qmMyFoilPriceGuide, ui->deckBoxPriceInputLineEdit->text(), false);
}

void PCMWindow::LoadInventory(QMap<quint64, InventoryCard>* qmRegularInventory,
                              QMap<quint64, InventoryCard>* qmFoilInventory,
                              QString sFileSource,
                              bool AddNotMax)
{
    QFile fInput(sFileSource);
    QMap<quint64, InventoryCard>* qmRightInventory;
    if(fInput.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&fInput);
        QString line = in.readLine();
        InventoryCard::InitOrder(line);
        while (!in.atEnd())
        {
            line = in.readLine();
            InventoryCard card(line);
            qmRightInventory = card.bMyFoil ? qmFoilInventory : qmRegularInventory;
            quint64 iMultiverseID = qmMultiInverse.value(card.sMyName, 0);
            if(iMultiverseID == 0)
                continue; //we can't handle things that have no multiverse ID
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
                qmRightInventory->insert(iMultiverseID, InventoryCard(AddNotMax ? card.iMyCount : card.dMyMarketPrice));
            }
        }

        StatusString(QString("Read %1 regular cards").arg(qmRegularInventory->size()));
        StatusString(QString("Read %1 foil cards").arg(qmFoilInventory->size()));
    }
    else
    {
        StatusString(QString("Collection or Pricelist not loaded, does the file exist?")
                     .arg(ui->tradeOutputLineEdit->text()), true);
    }
}

void PCMWindow::on_pbOpenDatabase_clicked()
{
    qmMultiverse.clear();
    qmMultiInverse.clear();
    qmOracle.clear();

    OracleCard::sImagePath = ui->imageLocationLineEdit->text();

    QFile fInput(ui->cardDatabaseLocationLineEdit->text());
    if(fInput.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        unsigned int iNoMID = 0, iNoMCID = 0, iNoSID = 0;

        QJsonDocument jdoc = QJsonDocument::fromJson(fInput.readAll(), &jError);

        if(jError.error != QJsonParseError::NoError)
            ui->imageLabel->setText(QObject::tr("Not a json document"));

        QJsonArray jSets = jdoc.array();

        for(auto&& set: jSets)
        {
            QJsonObject jSet = set.toObject();
            QJsonArray jCards = jSet["cards"].toArray();

            QString sMCISID;
            if(jSet["magicCardsInfoCode"].isNull())
            {
                iNoSID++;
                sMCISID = jSet["code"].toString().toLower();
            }
            else
            {
                sMCISID = jSet["magicCardsInfoCode"].toString();
            }

            for(auto&& cardIter: jCards)
            {
                QJsonObject jCard = cardIter.toObject();
                OracleCard card;

                if(jCard["multiverseid"].isNull())
                    iNoMID++;
                unsigned int iID = jCard["multiverseid"].toInt();
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
                        if(jCard["layout"].toString().contains("split")) //BFG
                        {
                            card.sNameEn = names.at(0).toString() + " // " + names.at(1).toString();
                        }
                    }
                    if(names.size() > 3)
                        continue;
                }

                if(jCard["number"].isNull())
                    iNoMCID++;
                else
                    card.sSequenceNumber = jCard["number"].toString();

                if(card.sSequenceNumber.contains("b") | jCard["mciNumber"].toString().contains("b"))
                    continue; //second half of a card, not worth having around (messes up DeckBox output)

                //if(card.sNameEn.contains("Kenzo"))
                    //int iasf = 999;

                card.sMySet = jSet["name"].toString();
                card.sID  = sMCISID;

                qmMultiverse.insert(iID, sName);
                qmMultiInverse.insert(sName, iID);
                qmOracle.insert(iID, card);
            }
        }
    }
}

void PCMWindow::on_pbFullCardListDB_clicked()
{
    int append = 'a';
    unsigned int i = 999999;
    QTextStream fFullCardListOutput;
    QFile *fullCardListFile;
    for(auto&& pair: qmOracle.toStdMap())
    {
        if(i++ > 90000) //hard coded pagination
        {
            fFullCardListOutput.flush();
            fullCardListFile = new QFile(ui->fullCardListLocationLineEdit->text() + append++);
            fullCardListFile->open(QIODevice::WriteOnly | QIODevice::Text);
            fFullCardListOutput.setDevice(fullCardListFile);
            fFullCardListOutput << "Count,Tradelist Count,Name,Edition,Card Number,Condition,Language,Foil,Signed,Artist Proof,Altered Art,Misprint,Promo,Textless,My Price,\n";
            i = 0;
        }
        OracleCard card = pair.second;
        if(card.sSequenceNumber.contains("b"))
            continue; //Deckbox doesn't want the second hard of split cards
        if(card.sNameEn.contains(QString("token"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we would generate tokens
        if(card.sNameEn.contains(QString("Big Furry Monster"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we generate this name
        if(card.sMySet.contains(QString("Tempest Remastered"), Qt::CaseInsensitive))
            continue; //Not real cards
        if(card.sMySet.contains(QString("Vintage Masters"), Qt::CaseInsensitive))
            continue; //Not real cards
        if(card.sMySet.contains(QString("Vanguard"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we would Avatars
        if(card.sMySet.contains(QString("Masters Edition"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we would Avatars
        fFullCardListOutput << card.deckBoxInventoryLine(false);
        fFullCardListOutput << card.deckBoxInventoryLine(true);
    }

    fFullCardListOutput.flush();
    fFullCardListOutput.device()->close();
    fFullCardListOutput.device()->deleteLater();
}

/*void PCMWindow::ReadOracle()
{
    while(reader.readNextStartElement())
    {
        if(reader.name() == "sets") ReadSets();
        else if(reader.name() == "cards") ReadCards();
        else reader.skipCurrentElement();
    }
}

void PCMWindow::ReadSets()
{
    while(reader.readNextStartElement())
    {
        if(reader.name() == "set") ReadSet();
        else reader.skipCurrentElement();
    }
}

void PCMWindow::ReadSet()
{
    QString sName, sCode;
    while(reader.readNextStartElement())
    {
        if(reader.name() == "name")
            sName = reader.readElementText();
        else if(reader.name() == "code")
            sCode = reader.readElementText();
        else reader.skipCurrentElement();
    }

    qmTheSetCode.insert(sCode, sName);
}

void PCMWindow::ReadCards()
{
    while(reader.readNextStartElement())
    {
        QString sTmp = reader.name().toString();
        if(reader.name() == "card") ReadCard();
        else reader.skipCurrentElement();
    }
}

void PCMWindow::ReadCard()
{
    QString sName, sSet, sNameDe;
    quint64 iID, iNumber;
    double dValue = -1;
    while(reader.readNextStartElement())
    {
        QString stmp = reader.name().toString();
        if(reader.name() == "id")
            iID     = reader.readElementText().toInt();
        else if(reader.name() == "set")
            sSet    = reader.readElementText();
        else if(reader.name() == "name")
            sName   = reader.readElementText();
        else if(reader.name() == "number")
            iNumber = reader.readElementText().toInt();
        else if(reader.name() == "name_DE")
            sNameDe = reader.readElementText();
        else if(reader.name() == "pricing_mid")
            dValue  = reader.readElementText().toDouble();
        else reader.skipCurrentElement();
    }

    OracleCard card;
    card.iMultiverseID = iID;
    card.sNameDe = sNameDe;
    card.sNameEn = sName;
    card.sSet = sSet;
    card.dValue = dValue;

    qmMultiverse.insert(iID, sName);
    qmOracle.insert(iID, card);
}*/

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

    QFile *tradeFile = new QFile(ui->tradeOutputLineEdit->text());
    if(tradeFile->open(QIODevice::Text | QIODevice::Append))
    {
        fMyTradesOutput.setDevice(tradeFile);
    }
    else
    {
        StatusString(QString("Could not open output file to store trades: %1")
                     .arg(ui->tradeOutputLineEdit->text()), true);
    }

    QFileInfo collectionFileInfo(ui->collectionOutputLineEdit->text());
    bool writeHeader = !collectionFileInfo.exists();
    QFile *collectionFile = new QFile(ui->collectionOutputLineEdit->text());
    if(collectionFile->open(QIODevice::Text | QIODevice::Append))
    {
        fMyCollectionOutput.setDevice(collectionFile);
        if(writeHeader)
            fMyCollectionOutput << "Count,Tradelist Count,Name,Edition,Card Number,Condition,Language,Foil,Signed,Artist Proof,Altered Art,Misprint,Promo,Textless,My Price,\n";
        fMyCollectionOutput.flush();
    }
    else
    {
        StatusString(QString("Could not open output file to store collection: %1")
                     .arg(ui->collectionOutputLineEdit->text()), true);
    }
}

QString OracleCard::getImagePath() const
{
    QDir path(QString("%1/%2").arg(sImagePath).arg(sID));
    path.mkpath(".");
    return QString("%3/%1/%2.jpg").arg(sID).arg(sSequenceNumber).arg(sImagePath);
}

QString OracleCard::getImageURL() const
{
    return QString("http://gatherer.wizards.com/Handlers/Image.ashx?multiverseid=%1&type=card").arg(iMultiverseID);
}

void PCMWindow::StatusString(QString sMessage, bool bError)
{
    ui->teStatus->append(QString("<font color=\"%1\">%2</font>\r\n")
                             .arg(bError ? "red" : "black")
                             .arg(sMessage));

    if(bError)
        ui->statusBar->showMessage(sMessage);
}
