/*
 * harbour-watchlist - Sailfish OS Version
 * Copyright © 2020 Andreas Wüst (andreas.wuest.freelancer@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "euroinvestormarketdatabackend.h"

#include <QDebug>
#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QJsonDocument>

EuroinvestorMarketDataBackend::EuroinvestorMarketDataBackend(QNetworkAccessManager *manager, const QString &applicationName, const QString applicationVersion, QObject *parent) : QObject(parent) {
    qDebug() << "Initializing Euroinvestor Market Data Backend...";
    this->manager = manager;
    this->applicationName = applicationName;
    this->applicationVersion = applicationVersion;

    // Index
    // DE
    marketDataId2ExtRefId["INDEX_DAX"] = "11876";
    marketDataId2ExtRefId["INDEX_MDAX"] = "12036";
    marketDataId2ExtRefId["INDEX_SDAX"] = "12100";
    marketDataId2ExtRefId["INDEX_TECDAX"] = "12101";
    // US
    marketDataId2ExtRefId["INDEX_S&P500"] = "15326";
    marketDataId2ExtRefId["INDEX_NASDAQ"] = "74288";
    marketDataId2ExtRefId["INDEX_DOWJONES"] = "9703";
    // France
    marketDataId2ExtRefId["INDEX_CN20"] = "78560";
    marketDataId2ExtRefId["INDEX_CAC40"] = "73594";
    marketDataId2ExtRefId["INDEX_SFB120"] = "70498";
    // Other
    marketDataId2ExtRefId["INDEX_OMXS30"] = "78541";
    marketDataId2ExtRefId["INDEX_OSEBX"] = "69309";
    marketDataId2ExtRefId["INDEX_OMC_C25"] = "64283";

    // Commodities
    marketDataId2ExtRefId["COM_GOLD"] = "8352";

    // Currencies
    marketDataId2ExtRefId["CUR_SEK_DKK"] = "36399";
    marketDataId2ExtRefId["CUR_EUR_USD"] = "36278";
    marketDataId2ExtRefId["CUR_GBP_DKK"] = "36274";
}

EuroinvestorMarketDataBackend::~EuroinvestorMarketDataBackend() {
    qDebug() << "Shutting down Euroinvestor Backend...";
    marketDataId2ExtRefId.clear();
}

QNetworkReply *EuroinvestorMarketDataBackend::executeGetRequest(const QUrl &url) {
    qDebug() << "AbstractDataBackend::executeGetRequest " << url;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, TODO_MIME_TYPE_JSON);
    request.setHeader(QNetworkRequest::UserAgentHeader, TODO_USER_AGENT);

    return manager->get(request);
}

QString EuroinvestorMarketDataBackend::getMarketDataExtRefId(const QString &marketDataId) {
    if (marketDataId2ExtRefId.contains(marketDataId)) {
        return marketDataId2ExtRefId[marketDataId];
    }

    return QString::null;
}

void EuroinvestorMarketDataBackend::lookupMarketData(const QString &marketDataIds) {
    qDebug() << "EuroinvestorMarketDataBackend::lookupMarketData";
    QNetworkReply *reply = executeGetRequest(QUrl(API_MARKET_DATA + marketDataIds));

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(handleRequestError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(handleLookupMarketDataFinished()));
}

void EuroinvestorMarketDataBackend::handleLookupMarketDataFinished() {
    qDebug() << "EuroinvestorMarketDataBackend::handleLookupMarketDataFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        return;
    }

    emit marketDataResultAvailable(processMarketDataResult(reply->readAll()));
}

QString EuroinvestorMarketDataBackend::processMarketDataResult(QByteArray marketDataResult) {
    qDebug() << "EuroinvestorMarketDataBackend::processMarketDataResult";
    QJsonDocument jsonDocument = QJsonDocument::fromJson(marketDataResult);
    if (!jsonDocument.isArray()) {
        qDebug() << "not a json array!";
    }

    QJsonArray responseArray = jsonDocument.array();
    QJsonDocument resultDocument;
    QJsonArray resultArray;

    foreach (const QJsonValue &value, responseArray) {
        QJsonObject rootObject = value.toObject();
        QJsonObject exchangeObject = rootObject["exchange"].toObject();

        QJsonObject resultObject;
        resultObject.insert("extRefId", rootObject.value("id"));
        resultObject.insert("name", rootObject.value("name"));
        resultObject.insert("currency", rootObject.value("currency"));
        resultObject.insert("last", rootObject.value("last"));
        resultObject.insert("symbol", rootObject.value("symbol"));
        // resultObject.insert("isin", rootObject.value("isin"));
        resultObject.insert("stockMarketName", exchangeObject.value("name"));
        resultObject.insert("changeAbsolute", rootObject.value("change"));
        resultObject.insert("changeRelative", rootObject.value("changeInPercentage"));
        //resultObject.insert("high", rootObject.value("high"));
        //resultObject.insert("low", rootObject.value("low"));
        //resultObject.insert("ask", rootObject.value("ask"));
        //resultObject.insert("bid", rootObject.value("bid"));
        //resultObject.insert("volume", rootObject.value("volume"));
        //resultObject.insert("numberOfStocks", rootObject.value("numberOfStocks"));

        QJsonValue updatedAt = rootObject.value("updatedAt");
        // TODO move date formatting to a separate method
        QDateTime dateTimeUpdatedAt = QDateTime::fromString(updatedAt.toString(), Qt::ISODate);
        QString updateAtString = dateTimeUpdatedAt.toString("yyyy-MM-dd") + " " + dateTimeUpdatedAt.toString("hh:mm:ss");
        resultObject.insert("quoteTimestamp", updateAtString);

        QDateTime dateTimeNow = QDateTime::currentDateTime();
        QString nowString = dateTimeNow.toString("yyyy-MM-dd") + " " + dateTimeNow.toString("hh:mm:ss");
        resultObject.insert("lastChangeTimestamp", nowString);

        resultArray.push_back(resultObject);
    }

    resultDocument.setArray(resultArray);
    QString dataToString(resultDocument.toJson());

    return dataToString;
}


void EuroinvestorMarketDataBackend::handleRequestError(QNetworkReply::NetworkError error) {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    qWarning() << "EuroinvestorMarketDataBackend::handleRequestError:" << (int)error << reply->errorString() << reply->readAll();

    emit requestError("Return code: " + QString::number((int)error) + " - " + reply->errorString());
}
