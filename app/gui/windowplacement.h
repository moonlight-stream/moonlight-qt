#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QTimer>
#include <QWindow>

class QScreen;

class WindowPlacement : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit WindowPlacement(QObject* parent = nullptr);

    QWindow* window() const;
    void setWindow(QWindow* window);

    bool isEnabled() const;
    void setEnabled(bool enabled);

    Q_INVOKABLE void restore();
    Q_INVOKABLE void flush();

signals:
    void windowChanged();
    void enabledChanged();

private:
    static QScreen* findSavedScreen(const QString& name, const QRect& geometry);
    static QRect constrainGeometry(const QRect& geometry, const QRect& availableGeometry);

    void scheduleSave();
    void saveNow();

    QPointer<QWindow> m_Window;
    QTimer m_SaveTimer;
    bool m_Enabled = false;
    bool m_Restoring = false;
};
