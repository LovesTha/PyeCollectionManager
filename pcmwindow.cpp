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

}

PCMWindow::~PCMWindow()
{
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
    }
}

void PCMWindow::TCPDisconnected()
{
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
        if(item.compare("GET",Qt::CaseInsensitive) == 0)
        {
            //yay it's a card request
            if(elements.isEmpty()) continue;

            QList<QByteArray> request = elements.takeFirst().split('=');
            bool bOk;
            unsigned int multiverseID = request.takeLast().toInt(&bOk);
            if(bOk)
            {
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
                    manager->get(QNetworkRequest(QUrl(card.getImageURL())));
                    sMyImageRequested = sImagePath;
                }

                //double dTmp = qmOracle.value(multiverseID).dValue;
                int iQuantity = qmMyInventory.value(multiverseID, -1);
                ui->lcdCollectionQuantity->display(iQuantity);

                if(qmMyInventory.value(multiverseID, -1) < 1)
                {
                    //card not in inventory
                    ui->cardAction->setText("KEEP!");
                    qmMyInventory.insert(multiverseID, 1);

                    fMyCollectionOutput << card.deckBoxInventoryLine(false);
                    fMyCollectionOutput.flush();
                }
                else if(qmMyPriceGuide.value(multiverseID, -1) > 0.1)
                {
                    ui->cardAction->setText("TRADE!");
                }
                else if(qmMyPriceGuide.value(multiverseID, -1) < 0.001)
                {
                    ui->cardAction->setText("No Price Data");
                }
                else
                {
                    ui->cardAction->setText("trash");
                }
            }
            else
            {
                ui->imageLabel->setText("Invalid request");
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
       ui->imageLabel->setText("Image failed to fetch");

   img2->save("/tmp/img.jpg", "JPG", 99);

   img2->save(sMyImageRequested, "JPG", 99);
   DisplayImage(sMyImageRequested);
}

QVector<unsigned int> *InventoryCard::pviTheFieldIndexes = 0;
bool InventoryCard::InitOrderEstablished = false;
QMap<QString, unsigned int> InventoryCard::qmTheStringIndex;
QString OracleCard::sImagePath = "/tmp/";

void InventoryCard::InitOrder(QString sInitLine)
{
    if(pviTheFieldIndexes == 0) pviTheFieldIndexes = new QVector<unsigned int>;
    for(int i = 0; i < 17; ++i) pviTheFieldIndexes->append(i);

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

InventoryCard::InventoryCard(QString sInitLine)
{
    QStringList elements = sInitLine.split(",");
    if(elements.size() < 17)
        return;

    if(!InitOrderEstablished)
        InitOrder(""); //will create the default order;

    QString sTmp;

    sTmp = elements.at(pviTheFieldIndexes->at(0));
    iMyCount = sTmp.toInt();

    sTmp = elements.at(pviTheFieldIndexes->at(1));
    iMyTradelistCount = sTmp.toInt();

    sTmp = elements.at(pviTheFieldIndexes->at(4));
    iMyCardNumber = sTmp.toInt();

    sTmp = elements.at(pviTheFieldIndexes->at(16));
    if(sTmp.size() > 1 && sTmp.at(0) == '$')
        sTmp = sTmp.remove(0, 1);
    dMyMarketPrice = sTmp.toFloat();

    sTmp = elements.at(pviTheFieldIndexes->at(14));
    dMySalePrice = sTmp.toFloat();

    sMyName      = elements.at(pviTheFieldIndexes->at(2));
    sMyEdition   = elements.at(pviTheFieldIndexes->at(3));
    sMyCondition = elements.at(pviTheFieldIndexes->at(5));
    sMyLanguage  = elements.at(pviTheFieldIndexes->at(6));
    sMyRarity    = elements.at(pviTheFieldIndexes->at(15));

    bMyFoil        = !elements.at(pviTheFieldIndexes->at(7)).isEmpty();
    bMySigned      = !elements.at(pviTheFieldIndexes->at(8)).isEmpty();
    bMyArtistProof = !elements.at(pviTheFieldIndexes->at(9)).isEmpty();
    bMyAlteredArt  = !elements.at(pviTheFieldIndexes->at(10)).isEmpty();
    bMyMisprint    = !elements.at(pviTheFieldIndexes->at(11)).isEmpty();
    bMyPromo       = !elements.at(pviTheFieldIndexes->at(12)).isEmpty();
    bMyTextless    = !elements.at(pviTheFieldIndexes->at(13)).isEmpty();
}

void PCMWindow::on_pbOpenCollection_clicked()
{
    LoadInventory(&qmMyInventory, ui->collectionSourceLineEdit->text(), true);
    LoadInventory(&qmMyPriceGuide, ui->deckBoxPriceInputLineEdit->text(), false);
}

void PCMWindow::LoadInventory(QMap<quint64, double>* qmInventory, QString sFileSource, bool AddNotMax)
{
    QFile fInput(sFileSource);
    if(fInput.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&fInput);
        QString line = in.readLine();
        InventoryCard::InitOrder(line);
        while (!in.atEnd())
        {
            line = in.readLine();
            InventoryCard card(line);
            quint64 iMultiverseID = qmMultiInverse.value(card.sMyName, -1);
            if(iMultiverseID == -1)
                continue; //we can't handle things that have no multiverse ID
            if(qmInventory->contains(iMultiverseID))
            {
                double val;

                if(AddNotMax)
                    val = qmInventory->value(iMultiverseID) + card.iMyCount;
                else
                    val = std::max(qmInventory->value(iMultiverseID), card.dMyMarketPrice);

                qmInventory->insert(iMultiverseID, val);
            }
            else
            {
                qmInventory->insert(iMultiverseID, AddNotMax ? card.iMyCount : card.dMyMarketPrice);
            }
        }
    }
    else
    {
        int PleaseHandleNotOpeningFile = 9;
    }
}

void PCMWindow::on_pbOpenDatabase_clicked()
{
    OracleCard::sImagePath = ui->imageLocationLineEdit->text();
    unsigned int iNoMID = 0, iNoMCID = 0, iNoSID = 0;

    QFile fInput(ui->cardDatabaseLocationLineEdit->text());
    if(fInput.open(QIODevice::ReadOnly | QIODevice::Text))
    {
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
    QFile *tradeFile = new QFile(ui->tradeOutputLineEdit->text());
    if(tradeFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        fMyTradesOutput.setDevice(tradeFile);
    }
    else
    {
        int PleaseHandleNotOpeningFile = 9;
    }

    QFileInfo collectionFileInfo(ui->collectionOutputLineEdit->text());
    bool writeHeader = !collectionFileInfo.exists();
    QFile *collectionFile = new QFile(ui->collectionOutputLineEdit->text());
    if(collectionFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        fMyCollectionOutput.setDevice(collectionFile);
        if(writeHeader)
            fMyCollectionOutput << "Count,Tradelist Count,Name,Edition,Card Number,Condition,Language,Foil,Signed,Artist Proof,Altered Art,Misprint,Promo,Textless,My Price,\n";
        fMyCollectionOutput.flush();
    }
    else
    {
        int PleaseHandleNotOpeningFile = 9;
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

