#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QSettings>
#include <QColor>

class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool gameModeEnabled READ isGameModeEnabled WRITE setGameModeEnabled NOTIFY gameModeEnabledChanged)
    Q_PROPERTY(int favoriteComputer READ getFavoriteComputer WRITE setFavoriteComputer NOTIFY favoriteComputerChanged)
    Q_PROPERTY(QString primaryColor READ getPrimaryColor WRITE setPrimaryColor NOTIFY primaryColorChanged)
    Q_PROPERTY(QString secondaryColor READ getSecondaryColor WRITE setSecondaryColor NOTIFY secondaryColorChanged)
    Q_PROPERTY(float appImageHeight READ appImageHeight NOTIFY appImageHeightChanged)
    Q_PROPERTY(float appImageWidth READ appImageWidth NOTIFY appImageWidthChanged)
    Q_PROPERTY(float appImageSize READ getAppImageSize WRITE setAppImageSize NOTIFY appImageSizeChanged)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
    Q_PROPERTY(float appNameFontSize READ getAppNameFontSize WRITE setAppNameFontSize NOTIFY appNameFontSizeChanged)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    Q_INVOKABLE bool isGameModeEnabled() const;
    Q_INVOKABLE void setGameModeEnabled(bool enabled);

    Q_INVOKABLE int getFavoriteComputer() const;
    Q_INVOKABLE void setFavoriteComputer(int computerId);
    Q_INVOKABLE bool isComputerFavorite(int computerId) const;

    Q_INVOKABLE QString getPrimaryColor() const;
    Q_INVOKABLE void setPrimaryColor(const QString &color);

    Q_INVOKABLE QString getSecondaryColor() const;
    Q_INVOKABLE void setSecondaryColor(const QString &color);

    Q_INVOKABLE float getAppImageSize() const;
    Q_INVOKABLE void setAppImageSize(float size);

    float appImageHeight() const;
    float appImageWidth() const;

    int spacing() const;
    Q_INVOKABLE void setSpacing(int newSpacing);

    Q_INVOKABLE float getAppNameFontSize() const;
    Q_INVOKABLE void setAppNameFontSize(float size);

signals:
    void gameModeEnabledChanged();
    void favoriteComputerChanged();
    void primaryColorChanged();
    void secondaryColorChanged();
    void appImageHeightChanged();
    void appImageWidthChanged();
    void appImageSizeChanged();
    void spacingChanged();
    void appNameFontSizeChanged();

private:
    void loadState();
    void saveState();
    void updateImageSize();

    QSettings m_settings;
    bool m_gameModeEnabled;
    int m_favoriteComputer;
    QString m_primaryColor;
    QString m_secondaryColor;
    float m_appImageSize;
    float m_appImageWidth;
    float m_appImageHeight;
    int m_spacing;
    float m_appNameFontSize;
};

#endif // THEMEMANAGER_H
