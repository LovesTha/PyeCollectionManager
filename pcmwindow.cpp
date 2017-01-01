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
    ui->soundsLocationLineEdit->setText(Config.value("Sounds Location", "/mtg/sounds/").toString());

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
                InventoryCard priceCard = qmMyRegularPriceGuide.value(multiverseID, InventoryCard(0));
                bool isFoil = ui->cbScanningFoils->isChecked();
                ui->lcdCollectionQuantity->display((isFoil ? invFoilCard : invRegularCard).iMyCount);

                if((invFoilCard.iMyCount + invRegularCard.iMyCount < ui->quantityToKeepSpinBox->value()) //we don't have enough of any variant
                  ||(isFoil && invFoilCard.iMyCount < ui->quantityToKeepSpinBox->value())) //this is a foil, and we don't have enough foils
                {
                    //card not in inventory
                    ui->cardAction->setText("KEEP!");
                    StatusString(QString("Keep: %1").arg(invRegularCard.sMyName));
                    mySound.setMedia(qKeep);
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
                    mySound.setMedia(qCoins);
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
                    mySound.setMedia(qWeird);
                    mySound.play();
                }
                else
                {
                    ui->cardAction->setText("trash");
                    StatusString(QString("Trash: %1").arg(invRegularCard.sMyName));
                    mySound.setMedia(qTrash);
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

void PCMWindow::on_pbOpenCollection_clicked()
{
    qmMyRegularInventory.clear();
    qmMyFoilInventory.clear();
    qmMyRegularPriceGuide.clear();
    qmMyFoilPriceGuide.clear();

    StatusString("Attempting to load previous run inventory in Deckbox format");
    LoadInventory(&qmMyRegularInventory, &qmMyFoilInventory, ui->collectionOutputLineEdit->text(), true); //inventory from previous runs
    StatusString("Attempting to load exported Deckbox Inventory");
    LoadInventory(&qmMyRegularInventory, &qmMyFoilInventory, ui->collectionSourceLineEdit->text(), true); //inventory that has been exported from Deckbox
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
                qmRightInventory->insert(iMultiverseID, card);
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

            QString sMCISID, sGSID;
            if(jSet["magicCardsInfoCode"].isNull())
            {
                iNoSID++;
                sMCISID = jSet["code"].toString().toLower();
            }
            else
            {
                sMCISID = jSet["magicCardsInfoCode"].toString();
            }

            if(jSet["gathererCode"].isNull())
                sGSID = jSet["code"].toString();
            else
                sGSID = jSet["gathererCode"].toString();

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

                card.cRarity = jCard["rarity"].toString().at(0).toLatin1();
                card.sMySet = jSet["name"].toString();
                card.sMCISID = sMCISID;
                card.sGSID = sGSID;

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
            fMyCollectionOutput << "Count,Tradelist Count,Name,Edition,Card Number,Condition,Language,Foil,Signed,Artist Proof,Altered Art,Misprint,Promo,Textless,My Price\n";
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
