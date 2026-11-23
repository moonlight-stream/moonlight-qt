#pragma once

#include "filetransferengine.h"
#include <SDL.h>
#include <QSize>
#include <functional>
#include <optional>
#include <list>
#include <qt_windows.h>

// The SDL thread owns the gesture. Shell and HTTPS lookups run outside its event loop.
class StreamFileDrag
{
public:
    using Transfer = std::function<void(bool, const QList<TransferEntry>&, const QString&)>;
    using Error = std::function<void(const QString&)>;
    StreamFileDrag(SDL_Window* window, QSize frame, QUrl host, QSslCertificate certificate, QByteArray token,
                   Transfer transfer, Error error);
    ~StreamFileDrag();
    void updateWindow();
    void cancelGesture();
    bool handleButton(const SDL_MouseButtonEvent& event, bool canStart, Uint8 primaryButton);
    bool waitingForRelease() const;
    bool pending() const;
    void poll();

private:
    class DropTarget;
    struct Gesture {
        QPoint anchor;
        POINT start;
        std::future<FileDragLocation> source;
        std::future<QString> target;
        Uint32 releasedAt = 0;
        bool deferredRelease = false;
    };
    struct Download {
        QList<TransferEntry> entries;
        std::future<QString> target;
    };
    std::optional<QPoint> framePoint(POINT screen) const;
    void detach();
    void probe(POINT screen);
    bool finishGesture(bool cancelRemoteDrag);
    SDL_Window* m_Window;
    QSize m_Frame;
    FileTransferEngine m_Engine;
    Transfer m_Transfer;
    Error m_Error;
    HWND m_Owner = nullptr;
    DropTarget* m_DropTarget = nullptr;
    HRESULT m_Ole;
    std::optional<Gesture> m_Gesture;
    std::list<std::future<FileDragLocation>> m_Uploads;
    std::list<Download> m_Downloads;
};
