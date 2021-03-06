/* Copyright 2013-2020 Yikun Liu <cos.lyk@gmail.com>
 *
 * This program is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see http://www.gnu.org/licenses/.
 */

#include "jsapiObject.h"
#include "accessManager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSettings>
#include "dialogs.h"

JSAPIObject::JSAPIObject(const QString &id, QObject *parent) : QObject(parent), m_id(id)
{
}


void JSAPIObject::get_post_content(const QString& url, const QByteArray &contentType, const QByteArray& postData, const QJSValue& callbackFunc)
{
    Q_ASSERT(NetworkAccessManager::instance() != nullptr);

    QNetworkRequest request = QNetworkRequest(url);
    QNetworkReply *reply;
    if (postData.isEmpty())
    {
        reply = NetworkAccessManager::instance()->get(request);
    }
    else
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
        reply = NetworkAccessManager::instance()->post(request, postData);
    }

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        reply->deleteLater();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // redirection
        if (status == 301 || status == 302)
        {
            QByteArray final_url = reply->rawHeader(QByteArrayLiteral("Location"));
            get_post_content(QString::fromUtf8(final_url), contentType, postData, callbackFunc);
            return;
        }

        // Error
        if (reply->error() != QNetworkReply::NoError)
        {
            QString errStr = QStringLiteral("Network Error: %1\n%2\n").arg(QString::number(status), reply->errorString());
            Dialogs::instance()->messageDialog(tr("Error"), errStr);
            return;
        }

        // Call callback function
        QJSValue func = callbackFunc;
        QJSValue retVal = func.call({ QString::fromUtf8(reply->readAll()) });
        if (retVal.isError())
            emit jsError(retVal);
    });
}

void JSAPIObject::get_content(const QString& url, const QJSValue& callbackFunc)
{
    get_post_content(url, QByteArray(), QByteArray(), callbackFunc);
}

void JSAPIObject::post_content(const QString &url, const QByteArray &contentType, const QByteArray &postData, const QJSValue &callbackFunc)
{
    get_post_content(url, contentType, postData, callbackFunc);
}

// Dialogs
void JSAPIObject::information(const QString &msg)
{
    Q_ASSERT(Dialogs::instance() != nullptr);
    Dialogs::instance()->messageDialog(tr("Information"), msg);
}

void JSAPIObject::warning(const QString& msg)
{
    Q_ASSERT(Dialogs::instance() != nullptr);
    Dialogs::instance()->messageDialog(tr("Warning"), msg);
}

void JSAPIObject::get_text(const QString& msg, const QString& defaultValue, const QJSValue& callbackFunc)
{
    Q_ASSERT(Dialogs::instance() != nullptr);
    Dialogs::instance()->textInputDialog(msg, [=](const QString& text) {
        QJSValue retVal = QJSValue(callbackFunc).call({ text });
        if (retVal.isError()) {
            emit jsError(retVal);
        }
    }, defaultValue);
}

// Show result
void JSAPIObject::show_result(const QVariant& result)
{
    emit showResultRequested(result);
}

// Configurations
QVariant JSAPIObject::get_configuration(const QString &name)
{
    QString key = QStringLiteral("plugin-%1/%2").arg(m_id, name);
    return QSettings().value(key);
}

void JSAPIObject::set_configuration(const QString &name, const QVariant &value)
{
    QString key = QStringLiteral("plugin-%1/%2").arg(m_id, name);
    QSettings().setValue(key, value);
}
