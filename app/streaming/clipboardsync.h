#pragma once

#include <QSslCertificate>
#include <QThread>
#include <QUrl>

class ClipboardSync : public QThread
{
public:
    ClipboardSync(QUrl host, QSslCertificate certificate, QByteArray token, bool text, bool files);
    ~ClipboardSync() override;
    void stop();

protected:
    void run() override;

private:
    QUrl m_Host;
    QSslCertificate m_Certificate;
    QByteArray m_Token;
    bool m_Text;
    bool m_Files;
    unsigned long m_LocalSequence;
};
