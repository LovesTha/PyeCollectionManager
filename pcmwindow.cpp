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
                QString sCardName = myOracle.CardNameFromMultiverseID(multiverseID);
                OracleCard card = myOracle.CardFromMultiverseID(multiverseID);
                QString stmp = QString("/data/Oracle/pics/%1/%2.full.jpg").arg(card.sSet).arg(multiverseID);
                QImage qImage(stmp);
                QPixmap image(QPixmap::fromImage(qImage));
                image = image.scaled(ui->imageLabel->size(), Qt::AspectRatioMode::KeepAspectRatio, Qt::SmoothTransformation);
                ui->imageLabel->setPixmap(image);
                //double dTmp = qmOracle.value(multiverseID).dValue;
                int iQuantity = qmInventory.value(sCardName, -1);
                ui->lcdCollectionQuantity->display(iQuantity);

                if((qmInventory.value(sCardName, -1) < 1)
                        | ui->cbCollectionForce->isChecked())
                {
                    //card not in inventory
                    ui->cardAction->setText("KEEP!");
                    mySound.setMedia(QUrl::fromLocalFile("/home/gareth/PyeCollectionManager/lock.wav"));
                    mySound.play();
                    //Count,Tradelist Count,Name,Edition,Card Number,Condition,Language,Foil,Signed,Artist Proof,Altered Art,Misprint,Promo,Textless,My Price
                    //fMyCollectionOutput << "1,0,\"" << qmOracle.value(multiverseID).sNameEn << "\"," << qmTheSetCode.value(qmOracle.value(multiverseID).sSet) << "," << qmOracle.value(multiverseID).iSetID << ",Near Mint,English,,,,,,,,\n";
                    fMyCollectionOutput << "1,\"" << card.sNameEn << "\"," << myOracle.SetNameFromSetCode(card.sSet) << ",Near Mint,";
                    if(ui->cbScanningFoils->isChecked())
                        fMyCollectionOutput << "Foil";
                    fMyCollectionOutput << "\n";
                    fMyCollectionOutput.flush();
                    qmInventory.insert(sCardName, 1);
                }
                else if(card.dValue > ui->sbTradeThreshold->value())
                {
                    ui->cardAction->setText("TRADE!");
                    mySound.setMedia(QUrl::fromLocalFile("/home/gareth/PyeCollectionManager/coins.wav"));
                    mySound.play();
                    fMyTradesOutput << "1,\"" << card.sNameEn << "\"," << myOracle.SetNameFromSetCode(card.sSet) << ",Near Mint,English,";
                    if(ui->cbScanningFoils->isChecked())
                        fMyTradesOutput << "Foil";
                    fMyTradesOutput << "\n";
                    fMyTradesOutput.flush();
                }
                else if(card.dValue < 0.001)
                {
                    ui->cardAction->setText("No Price Data");
                    mySound.setMedia(QUrl::fromLocalFile("/home/gareth/PyeCollectionManager/weird.wav"));
                    mySound.play();

                }
                else
                {
                    ui->cardAction->setText("trash");
                    mySound.setMedia(QUrl::fromLocalFile("/home/gareth/PyeCollectionManager/trashcan.wav"));
                    mySound.play();

                }
            }
            else
            {
                ui->imageLabel->setText("Invalid request");
            }
        }
    }
}

QVector<int> *InventoryCard::pviTheFieldIndexes = 0;
bool InventoryCard::InitOrderEstablished = false;
QMap<QString, unsigned int> InventoryCard::qmTheStringIndex;

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

InventoryCard::InventoryCard(QString sInitLine)
{
    QStringList elements = sInitLine.split(",");
    if(elements.size() < 15)
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
        dMyMarketPrice =  elements.at(pviTheFieldIndexes->at(16)).toFloat();
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
        ui->statusBar->showMessage("Collection not loaded, does the file exist?");
    }
}

void PCMWindow::on_pbOpenDatabase_clicked()
{
    ui->statusBar->showMessage(myOracle.OpenFile(ui->cardDatabaseLocationLineEdit->text()));
}


void PCMWindow::on_pbOpenOutputs_clicked()
{
    QFile *tradeFile = new QFile(ui->tradeOutputLineEdit->text());
    bool tradesExist = false, collectionExists = false;
    if(tradeFile->exists())
    {
        tradesExist = true;
    }
    if(tradeFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        fMyTradesOutput.setDevice(tradeFile);
        fMyTradesOutput.setCodec("UTF-8");
        if(!tradesExist)
            fMyTradesOutput << "Count,Name,Edition,Condition,Language,Foil\n";
    }
    else
    {
        ui->statusBar->showMessage("Trades output file could not be opened, please check you have permission to write to that file/folder.");
    }

    QFile *collectionFile = new QFile(ui->collectionOutputLineEdit->text());
    if(collectionFile->exists())
    {
        collectionExists = true;
    }
    if(collectionFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        fMyCollectionOutput.setDevice(collectionFile);
        fMyCollectionOutput.setCodec("UTF-8");
        if(!collectionExists)
            fMyCollectionOutput << "Count,Name,Edition,Condition,Foil\n";
    }
    else
    {
        ui->statusBar->showMessage("Collection output file could not be opened, please check you have permission to write to that file/folder.");
    }
}
