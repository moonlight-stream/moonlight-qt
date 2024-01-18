#include "ThemeManager.h"

// Constants for default image sizes
const float APP_IMAGE_DEFAULT_WIDTH = 200;
const float APP_IMAGE_DEFAULT_HEIGHT = 267;

// Constructor for ThemeManager
ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent),
    m_gameModeEnabled(false),
    m_favoriteComputer(-1),
    m_appImageSize(1.0f), // Default scale factor for image size
    m_appImageWidth(APP_IMAGE_DEFAULT_WIDTH * m_appImageSize), // Initialize image width
    m_appImageHeight(APP_IMAGE_DEFAULT_HEIGHT * m_appImageSize), // Initialize image height
    m_appNameFontSize(36) { // Default font size for app name
    loadState(); // Load state from persistent storage
    updateImageSize(); // Update image size based on the current scale factor
}

// Getters and setters for primary and secondary colors
QString ThemeManager::getPrimaryColor() const { return m_primaryColor; }
void ThemeManager::setPrimaryColor(const QString &color) {
    if (m_primaryColor != color) {
        m_primaryColor = color;
        saveState(); // Save the updated state
        emit primaryColorChanged(); // Emit signal indicating change
    }
}

QString ThemeManager::getSecondaryColor() const { return m_secondaryColor; }
void ThemeManager::setSecondaryColor(const QString &color) {
    if (m_secondaryColor != color) {
        m_secondaryColor = color;
        saveState();
        emit secondaryColorChanged();
    }
}

// Getters and setters for application image size
float ThemeManager::getAppImageSize() const { return m_appImageSize; }
void ThemeManager::setAppImageSize(float size) {
    if (m_appImageSize != size) {
        m_appImageSize = size;
        updateImageSize(); // Update image dimensions
        saveState();
        emit appImageSizeChanged();
    }
}

// Update image width and height based on scale factor
void ThemeManager::updateImageSize() {
    m_appImageWidth = APP_IMAGE_DEFAULT_WIDTH * m_appImageSize;
    m_appImageHeight = APP_IMAGE_DEFAULT_HEIGHT * m_appImageSize;
    emit appImageWidthChanged();
    emit appImageHeightChanged();
}

// Getters for image width and height
float ThemeManager::appImageWidth() const { return m_appImageWidth; }
float ThemeManager::appImageHeight() const { return m_appImageHeight; }

// Getter and setter for game mode enabled state
bool ThemeManager::isGameModeEnabled() const { return m_gameModeEnabled; }
void ThemeManager::setGameModeEnabled(bool enabled) {
    if (m_gameModeEnabled != enabled) {
        m_gameModeEnabled = enabled;
        saveState();
        emit gameModeEnabledChanged();
    }
}

// Getter and setter for favorite computer
int ThemeManager::getFavoriteComputer() const { return m_favoriteComputer; }
void ThemeManager::setFavoriteComputer(int computerId) {
    if (m_favoriteComputer != computerId) {
        m_favoriteComputer = computerId;
        saveState();
        emit favoriteComputerChanged();
    }
}

// Check if a computer is marked as favorite
bool ThemeManager::isComputerFavorite(int computerId) const {
    return m_favoriteComputer == computerId;
}

// Getter and setter for spacing
int ThemeManager::spacing() const { return m_spacing; }
void ThemeManager::setSpacing(int newSpacing) {
    if (m_spacing != newSpacing) {
        m_spacing = newSpacing;
        emit spacingChanged();
        saveState();
    }
}

// Getter and setter for app name font size
float ThemeManager::getAppNameFontSize() const { return m_appNameFontSize; }
void ThemeManager::setAppNameFontSize(float size) {
    if (m_appNameFontSize != size) {
        m_appNameFontSize = size;
        emit appNameFontSizeChanged();
        saveState();
    }
}

// Load and save state methods
void ThemeManager::loadState() {
    // Load various settings from persistent storage
    m_gameModeEnabled = m_settings.value("gameModeEnabled", false).toBool();
    m_favoriteComputer = m_settings.value("favoriteComputer", -1).toInt();
    m_primaryColor = m_settings.value("primaryColor", "#000000").toString();
    m_secondaryColor = m_settings.value("secondaryColor", "#FFFFFF").toString();
    m_appImageSize = m_settings.value("appImageSize", 1.0f).toFloat();
    m_spacing = m_settings.value("spacing", 40).toInt();
    m_appNameFontSize = m_settings.value("appNameFontSize", 36).toFloat();
    updateImageSize();
}

void ThemeManager::saveState() {
    // Save various settings to persistent storage
    m_settings.setValue("gameModeEnabled", m_gameModeEnabled);
    m_settings.setValue("favoriteComputer", m_favoriteComputer);
    m_settings.setValue("primaryColor", m_primaryColor);
    m_settings.setValue("secondaryColor", m_secondaryColor);
    m_settings.setValue("appImageSize", m_appImageSize);
    m_settings.setValue("spacing", m_spacing);
    m_settings.setValue("appNameFontSize", m_appNameFontSize);
}
