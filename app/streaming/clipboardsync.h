#pragma once

#include "clipboarduploadprogress.h"
#include <QSslCertificate>
#include <QThread>
#include <QUrl>
#include <atomic>
#include <cstdint>
#include <memory>

class ClipboardSync : public QThread
{
public:
    ClipboardSync(QUrl host, QSslCertificate certificate, QByteArray token, bool text, bool files,
                  bool resumable = false, bool multitask = false);
    ~ClipboardSync() override;
    void stop();
    // Called from the stream window's input loop.
    bool requestPaste();
    void cancelPaste(bool discardUpload = false);
    int takeReadyPastes();
    void acknowledgePastesSent(bool delivered = true);
    bool pastePending() const;
    QList<ClipboardUploadProgress> fileUploadTasks() const;
    void cancelFileTask(quint64 taskId);
    bool pasteFileTask(quint64 taskId);
    ClipboardUploadProgress fileUploadProgress() const;
    int fileUploadPercent() const { return fileUploadProgress().percent(); }

protected:
    void run() override;

private:
    struct FileQueue;
    std::unique_ptr<FileQueue> m_FileQueue;
    QUrl m_Host;
    QSslCertificate m_Certificate;
    QByteArray m_Token;
    bool m_Text;
    bool m_Files;
    bool m_Resumable;
    bool m_Multitask;
    unsigned long m_LocalSequence;
    static constexpr uint64_t NoPaste = UINT64_MAX;
    static constexpr uint64_t Ready = NoPaste - 1;
    std::atomic<uint64_t> m_ConfirmedSequence{NoPaste};
    std::atomic<uint64_t> m_PasteSequence{NoPaste};
    std::atomic<uint64_t> m_IncomingSequence{NoPaste};
    std::atomic<uint64_t> m_DeferredFileSequence{NoPaste};
    int m_PasteCount = 0;
    std::atomic<qint64> m_PasteActivity{0};
};
