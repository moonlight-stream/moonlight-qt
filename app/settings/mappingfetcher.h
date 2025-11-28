/**
 * @file app/settings/mappingfetcher.h
 * @brief Fetches SDL gamepad mappings from remote sources.
 */

#pragma once

#include <QObject>
#include <QNetworkAccessManager>

/**
 * @brief Fetches SDL gamepad mapping data from online sources.
 */
class MappingFetcher : public QObject
{
    Q_OBJECT

public:
    explicit MappingFetcher(QObject *parent = nullptr);

    void start();

private slots:
    void handleMappingListFetched(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_Nam;
};
