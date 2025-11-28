/**
 * @file app/backend/autoupdatechecker.h
 * @brief Checks for application updates automatically.
 */

#pragma once

#include <QObject>
#include <QNetworkAccessManager>

/**
 * @brief Automatically checks for application updates and notifies when new versions are available.
 */
class AutoUpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit AutoUpdateChecker(QObject *parent = nullptr);

    Q_INVOKABLE void start();

signals:
    void onUpdateAvailable(QString newVersion, QString url);

private slots:
    void handleUpdateCheckRequestFinished(QNetworkReply* reply);

private:
    void parseStringToVersionQuad(QString& string, QVector<int>& version);

    int compareVersion(QVector<int>& version1, QVector<int>& version2);

    QString getPlatform();

    QVector<int> m_CurrentVersionQuad;
    QNetworkAccessManager* m_Nam;
};
