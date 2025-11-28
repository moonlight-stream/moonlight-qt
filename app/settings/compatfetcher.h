/**
 * @file app/settings/compatfetcher.h
 * @brief Fetches GameStream compatibility information.
 */

#pragma once

#include <QObject>
#include <QNetworkAccessManager>

/**
 * @brief Fetches GameStream server compatibility information from remote sources.
 */
class CompatFetcher : public QObject
{
    Q_OBJECT

public:
    explicit CompatFetcher(QObject *parent = nullptr);

    void start();

    static bool isGfeVersionSupported(QString gfeVersion);

private slots:
    void handleCompatInfoFetched(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_Nam;
};
