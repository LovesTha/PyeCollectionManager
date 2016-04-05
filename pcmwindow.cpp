#include "pcmwindow.h"
#include "ui_pcmwindow.h"
#include <QTcpSocket>

PCMWindow::PCMWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PCMWindow)
{
    ui->setupUi(this);

    pqlMyImage = new QLabel(this);
    ui->MainTab->layout()->addWidget(pqlMyImage);

    pMyTCPServer = new QTcpServer(this);
    pMyTCPServer->setMaxPendingConnections(99);
    if(!pMyTCPServer->listen(QHostAddress::Any, 8080))
    {
        pqlMyImage->setText("Couldn't open port 8080, try restarting this application after you have changed something");
    }
    connect(pMyTCPServer, SIGNAL(newConnection()), SLOT(NewTCPConnection()));
}

PCMWindow::~PCMWindow()
{
    delete pqlMyImage;
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
            if(bOk) pqlMyImage->setText(QString("Multiverse ID of: %1").arg(multiverseID));
            else pqlMyImage->setText("Invalid request");
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

    QStringList elements = sInitLine.split("\t");
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
    QStringList elements = sInitLine.split("\t");
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
