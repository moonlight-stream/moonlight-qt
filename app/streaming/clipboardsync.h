#pragma once

#include "clipboarduploadprogress.h"
#include <QSslCertificate>
#include <QThread>
#include <QUrl>
#include <atomic>
#include <cstdint>
#include <mutex>

class ClipboardSync : public QThread
{
public:
    ClipboardSync(QUrl host, QSslCertificate certificate, QByteArray token, bool text, bool files, bool resumable = false);
    ~ClipboardSync() override;
    void stop();
    // Called from the stream window's input loop.
    bool requestPaste();
    void cancelPaste(bool discardUpload = false);
    int takeReadyPastes();
    bool pastePending() const { return m_PasteCount != 0; }
    ClipboardUploadProgress fileUploadProgress() const;
    int fileUploadPercent() const { return fileUploadProgress().percent(); }

protected:
    void run() override;

private:
    QUrl m_Host;
    QSslCertificate m_Certificate;
    QByteArray m_Token;
    bool m_Text;
    bool m_Files;
    bool m_Resumable;
    unsigned long m_LocalSequence;
    static constexpr uint64_t NoPaste = UINT64_MAX;
    static constexpr uint64_t Ready = NoPaste - 1;
    std::atomic<uint64_t> m_ConfirmedSequence{NoPaste};
    std::atomic<uint64_t> m_PasteSequence{NoPaste};
    std::atomic<uint64_t> m_BatchSequence{NoPaste};
    mutable std::mutex m_UploadMutex;
    ClipboardUploadProgress m_Upload;
    std::atomic<uint64_t> m_CancelGeneration{0};
    std::atomic<uint64_t> m_DiscardGeneration{UINT64_MAX};
    int m_PasteCount = 0;
    std::atomic<qint64> m_PasteActivity{0};
};
