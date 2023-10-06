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

    auto row = index.row();
    auto count = m_pPrefs->hotkeys.count();

    Q_ASSERT(-1 < row && row < count && count < 11);

    auto json = m_pPrefs->hotkeys[row];

    QJsonParseError jsonParseErr;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(json.toUtf8(), &jsonParseErr);
    if (jsonParseErr.error != QJsonParseError::NoError)
    {
        qDebug() << "Parse message error:"
                 << jsonParseErr.error
                 << "-> Error string:"
                 << jsonParseErr.errorString()
                 << "Offset symbol number:"
                 << jsonParseErr.offset
                 << "Check please :"
                 << json.mid(jsonParseErr.offset, 50).simplified();
        return QVariant();
    }

    auto hotkey = jsonDoc.object();

    switch (role)
    {
    case AppNameRole:
        return hotkey["appName"].toString();
    case ComputerNameRole:
        return hotkey["computerName"].toString();
    case HotkeyNumberRole:
        return row < 9 ? row + 1 : row == 9 ? 0 : -1;
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

QString hotkeyToJson(QString computerName, QString appName) {
    auto value = QJsonObject();
    value["computerName"] = computerName;
    value["appName"] = appName;
    auto json = QJsonDocument(value).toJson(QJsonDocument::Compact);
    return json;
}

int HotkeyModel::hotkeyIndexGet(QString computerName, QString appName) {
    auto json = hotkeyToJson(computerName, appName);
    return hotkeyIndexGet(json);
}

int HotkeyModel::hotkeyIndexGet(QString json) {
    return m_pPrefs->hotkeys.indexOf(json);
}

int HotkeyModel::hotkeyAdd(QString computerName, QString appName) {
    auto json = hotkeyToJson(computerName, appName);
    auto index = hotkeyIndexGet(json);
    int result = 0;
    if (index < 0) {
        m_pPrefs->hotkeys += json;
        m_pPrefs->save();
        emit hotkeysChanged();
        result = m_pPrefs->hotkeys.length() - 1;
    } else {
        result = -index - 1;
    }
    return result;
}

int HotkeyModel::hotkeyRemove(QString computerName, QString appName) {
    auto json = hotkeyToJson(computerName, appName);
    auto index = hotkeyIndexGet(json);
    int result = 0;
    if (index >= 0) {
        m_pPrefs->hotkeys.removeAt(index);
        m_pPrefs->save();
        emit hotkeysChanged();
        result = index;
    } else {
        result = -1;
    }
    return result;
}
