#include "pcmwindow.h"
#include "ui_pcmwindow.h"
#include <QTcpSocket>
#include <QFile>

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
                QString stmp = QString("/data/Oracle/pics/%1/%2.full.jpg").arg(qmOracle.value(multiverseID).sSet).arg(multiverseID);
                QImage qImage(stmp);
                QPixmap image(QPixmap::fromImage(qImage));
                image = image.scaled(ui->imageLabel->size(), Qt::AspectRatioMode::KeepAspectRatio, Qt::SmoothTransformation);
                ui->imageLabel->setPixmap(image);
                //double dTmp = qmOracle.value(multiverseID).dValue;
                int iQuantity = qmInventory.value(qmMultiverse.value(multiverseID), -1);
                ui->lcdCollectionQuantity->display(iQuantity);

                if(qmInventory.value(qmMultiverse.value(multiverseID), -1) < 1)
                {
                    //card not in inventory
                    ui->cardAction->setText("KEEP!");
                    qmInventory.insert(qmMultiverse.value(multiverseID), 1);
                }
                else if(qmOracle.value(multiverseID).dValue > 0.1)
                {
                    ui->cardAction->setText("TRADE!");
                }
                else if(qmOracle.value(multiverseID).dValue < 0.001)
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

QVector<unsigned int> *InventoryCard::pviTheFieldIndexes = 0;
bool InventoryCard::InitOrderEstablished = false;
QMap<QString, unsigned int> InventoryCard::qmTheStringIndex;

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
    if(elements.size() != 17)
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
    QFile fInput(ui->collectionSourceLineEdit->text());
    if(fInput.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&fInput);
        QString line = in.readLine();
        InventoryCard::InitOrder(line);
        while (!in.atEnd())
        {
            line = in.readLine();
            InventoryCard card(line);
            if(qmInventory.contains(card.sMyName))
            {
                qmInventory.insert(card.sMyName, qmInventory.value(card.sMyName) + card.iMyCount);
            }
            else
            {
                qmInventory.insert(card.sMyName, card.iMyCount);
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
    QFile fInput(ui->cardDatabaseLocationLineEdit->text());
    if(fInput.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        reader.setDevice(&fInput);

        if(reader.readNextStartElement())
        {
            QString sTmp = reader.name().toString();
            if(reader.name() == "mtg_carddatabase") ReadOracle();
            else reader.raiseError(QObject::tr("Not a Gatherer database rip"));
        }

        if(reader.error())
        {
            int PleaseReportErrors = 8;
        }
    }
}

void PCMWindow::ReadOracle()
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
}

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

    QFile *collectionFile = new QFile(ui->tradeOutputLineEdit->text());
    if(collectionFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        fMyCollectionOutput.setDevice(collectionFile);
    }
    else
    {
        int PleaseHandleNotOpeningFile = 9;
    }
}
