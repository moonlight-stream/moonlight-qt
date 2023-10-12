#include "hotkeymodel.h"

#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>

HotkeyModel::HotkeyModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void HotkeyModel::initialize(ComputerManager* computerManager, HotkeyManager* hotkeyManager)
{
    m_ComputerManager = computerManager;

    m_HotkeyManager = hotkeyManager;
}

int HotkeyModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;
    return m_HotkeyManager->count();
}

QVariant HotkeyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // presented index 0,1,2,3,4,5,6,7,8,9
    // keyboard number 1,2,3,4,5,6,7,8,9,0
    auto presentedIndex = index.row();
    if (presentedIndex >= 10) {
        qDebug() << "Maximum number of hotkeys reached";
        return QVariant();
    }
    auto hotkeyNumber = presentedIndex == 9 ? 0 : presentedIndex + 1;
    auto hotkeyInfo = m_HotkeyManager->get(hotkeyNumber);
    if (hotkeyInfo == nullptr) {
        return QVariant();
    }

    switch (role)
    {
    case AppNameRole:
        return hotkeyInfo->property("appName");
    case ComputerNameRole:
        return hotkeyInfo->property("computerName");
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
