#pragma once

#include <QSslCertificate>
#include <QElapsedTimer>
#include <QThread>
#include <QUrl>
#include <atomic>
#include <cstdint>

class ClipboardSync : public QThread
{
public:
    ClipboardSync(QUrl host, QSslCertificate certificate, QByteArray token, bool text, bool files);
    ~ClipboardSync() override;
    void stop();
    // Called from the stream window's input loop.
    bool requestPaste();
    void cancelPaste();
    int takeReadyPastes();
    bool pastePending() const { return m_PasteCount != 0; }

protected:
    void run() override;

private:
    QUrl m_Host;
    QSslCertificate m_Certificate;
    QByteArray m_Token;
    bool m_Text;
    bool m_Files;
    unsigned long m_LocalSequence;
    static constexpr uint64_t NoPaste = UINT64_MAX;
    static constexpr uint64_t Ready = NoPaste - 1;
    std::atomic<uint64_t> m_ConfirmedSequence{NoPaste};
    std::atomic<uint64_t> m_PasteSequence{NoPaste};
    int m_PasteCount = 0;
    QElapsedTimer m_PasteTimer;
};
