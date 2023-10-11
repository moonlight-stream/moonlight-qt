#include "hotkeymodel.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>

HotkeyModel::HotkeyModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void HotkeyModel::initialize(StreamingPreferences* prefs)
{
    m_pPrefs = prefs;

    if (false) {
        // Debug reset hotkeys
        qDebug() << "initialize: Reset hotkeys(" << m_pPrefs->hotkeys.count() << ")=" << m_pPrefs->hotkeys;
        m_pPrefs->hotkeys.clear();
        m_pPrefs->save();
        emit hotkeysChanged();
    }
    qDebug() << "initialize: hotkeys(" << m_pPrefs->hotkeys.count() << ")=" << m_pPrefs->hotkeys;
}

int HotkeyModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;
    return m_pPrefs->hotkeys.count();
}

QVariant HotkeyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    auto hotkeyNumber = QString::number(index.row());
    auto hotkeyInfo = m_pPrefs->hotkeys.value(hotkeyNumber).value<HotkeyInfo>();

    switch (role)
    {
    case AppNameRole:
        return hotkeyInfo.appName;
    case ComputerNameRole:
        return hotkeyInfo.computerName;
    case HotkeyNumberRole:
        return hotkeyNumber;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> HotkeyModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names[AppNameRole] = "appName";
    names[ComputerNameRole] = "computerName";
    names[HotkeyNumberRole] = "hotkeyNumber";
    return names;
}

int HotkeyModel::hotkeyNumber(QString computerName, QString appName) {
    int hotkeyNumber = -1;
    auto hotkeys = m_pPrefs->hotkeys.keys();
    for (auto& key : hotkeys) {
        auto hotkeyInfo = m_pPrefs->hotkeys[key].value<HotkeyInfo>();
        if (hotkeyInfo.computerName == computerName && hotkeyInfo.appName == appName) {
            hotkeyNumber = key.toInt();
            break;
        }
    }
    return hotkeyNumber;
}

void HotkeyModel::hotkeyPut(int hotkeyNumber, QString computerName, QString appName) {
    auto hotkeyInfo = HotkeyInfo(computerName, appName);
    auto key = QString::number(hotkeyNumber);
    auto value = QVariant::fromValue(hotkeyInfo);
    m_pPrefs->hotkeys[key] = value;
    m_pPrefs->save();
    emit hotkeysChanged();
}

void HotkeyModel::hotkeyRemove(int hotkeyNumber) {
    m_pPrefs->hotkeys.remove(QString::number(hotkeyNumber));
    m_pPrefs->save();
    emit hotkeysChanged();
}
