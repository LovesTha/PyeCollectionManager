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

    setWindowTitle("Pye Collection Manager - 0.1-Alpha-r2");

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
        StatusString(QString("New TCP Connection (%1)").arg(socket->peerAddress().toString()));
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
                    lRequestedImages.append(card);
                }

                //having started attempting to display the image, lets try and figure out if this card has multiple printings
                QList<quint64> lPrintingIDs = qmmMultiInverse.values(card.sNameEn);
                if(lPrintingIDs.length() == 1)
                    return HandleSingleCard(card);
                else
                    return HandleMultipleCards(card, lPrintingIDs);
            }
            else
            {
                ui->imageLabel->setText("Invalid request");
                StatusString("Unrecognised request", true);
            }
        }
    }
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

    InventoryCard priceCard = qmMyRegularPriceGuide.value(multiverseID, InventoryCard(0));

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
    else if((priceCard.dMyMarketPrice > ui->tradeValueThresholdDoubleSpinBox->value()) | isFoil())
    {
        ui->cardAction->setText("Trade");
        StatusString(QString("Trade: %1 ($%2)").arg(invRegularCard.sMyName).arg((isFoil() ? invFoilCard : invRegularCard).dMyMarketPrice));
        mySound.setMedia(qCoins);
        mySound.play();
        fMyTradesOutput << "1,\"" << card.sNameEn << "\",\"" << card.mySet->sMySet << "\",Near Mint,English,";
        if(isFoil())
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
        trash(invRegularCard);
    }
}

void PCMWindow::trash(InventoryCard card)
{
    ui->cardAction->setText("Trash");
    StatusString(QString("Trash: %1").arg(card.sMyName));
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
        InventoryCard priceCard(-1);
        for(auto&& CardID: lCardIDs)
        {
            priceCard = qmMyRegularPriceGuide.value(CardID, InventoryCard(0));

            if(priceCard.dMyMarketPrice < 0.001)
            {
                trashable = false;
                break; // we have no price data for this version, so we can't confidently trash it ever
            }
            if(priceCard.dMyMarketPrice > ui->tradeValueThresholdDoubleSpinBox->value())
            {
                trashable = false;
                break; // we have a valuable variant, so we can't confidently trash it ever
            }
        }

        if(trashable)
            return trash(priceCard);
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
               StatusString(QString("Failed to load image: %1").arg(requestedCard.getImageURL()), true);
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
        while (!in.atEnd())
        {
            line = in.readLine();
            if(line.length() < 5)
                continue;
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

        StatusString(QString("Read %1 regular cards").arg(qmRegularInventory->size()));
        StatusString(QString("Read %1 foil cards").arg(qmFoilInventory->size()));
    }
    else
    {
        StatusString(QString("Collection or Pricelist not loaded, does the file exist? (%1)")
                     .arg(sFileSource), true);
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

            OracleSet *ThisSet = new OracleSet;
            ThisSet->sMySet = jSet["name"].toString();
            ThisSet->sMCISID = sMCISID;
            ThisSet->sGSID = sGSID;
            ThisSet->LastSelected = QDateTime::currentDateTime();


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
                card.mySet = ThisSet;

                qmmMultiInverse.insert(sName, iID); //note this is now a Multi Map, this insert will never replace
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
        if(card.mySet->sMySet.contains(QString("Tempest Remastered"), Qt::CaseInsensitive))
            continue; //Not real cards
        if(card.mySet->sMySet.contains(QString("Vintage Masters"), Qt::CaseInsensitive))
            continue; //Not real cards
        if(card.mySet->sMySet.contains(QString("Vanguard"), Qt::CaseInsensitive))
            continue; //Deckbox doesn't like how we would Avatars
        if(card.mySet->sMySet.contains(QString("Masters Edition"), Qt::CaseInsensitive))
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

bool PCMWindow::isFoil()
{
    return ui->cbScanningFoils->isChecked();
}
