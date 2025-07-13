/****************************************************************************
** Meta object code from reading C++ file 'appmodel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../gui/appmodel.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'appmodel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN8AppModelE_t {};
} // unnamed namespace

template <> constexpr inline auto AppModel::qt_create_metaobjectdata<qt_meta_tag_ZN8AppModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AppModel",
        "computerLost",
        "",
        "handleComputerStateChanged",
        "NvComputer*",
        "computer",
        "handleBoxArtLoaded",
        "NvApp",
        "app",
        "image",
        "initialize",
        "ComputerManager*",
        "computerManager",
        "computerIndex",
        "showHiddenGames",
        "createSessionForApp",
        "Session*",
        "appIndex",
        "getDirectLaunchAppIndex",
        "getRunningAppId",
        "getRunningAppName",
        "quitRunningApp",
        "setAppHidden",
        "hidden",
        "setAppDirectLaunch",
        "directLaunch"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'computerLost'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'handleComputerStateChanged'
        QtMocHelpers::SlotData<void(NvComputer *)>(3, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Slot 'handleBoxArtLoaded'
        QtMocHelpers::SlotData<void(NvComputer *, NvApp, QUrl)>(6, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 4, 5 }, { 0x80000000 | 7, 8 }, { QMetaType::QUrl, 9 },
        }}),
        // Method 'initialize'
        QtMocHelpers::MethodData<void(ComputerManager *, int, bool)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 12 }, { QMetaType::Int, 13 }, { QMetaType::Bool, 14 },
        }}),
        // Method 'createSessionForApp'
        QtMocHelpers::MethodData<Session *(int)>(15, 2, QMC::AccessPublic, 0x80000000 | 16, {{
            { QMetaType::Int, 17 },
        }}),
        // Method 'getDirectLaunchAppIndex'
        QtMocHelpers::MethodData<int()>(18, 2, QMC::AccessPublic, QMetaType::Int),
        // Method 'getRunningAppId'
        QtMocHelpers::MethodData<int()>(19, 2, QMC::AccessPublic, QMetaType::Int),
        // Method 'getRunningAppName'
        QtMocHelpers::MethodData<QString()>(20, 2, QMC::AccessPublic, QMetaType::QString),
        // Method 'quitRunningApp'
        QtMocHelpers::MethodData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'setAppHidden'
        QtMocHelpers::MethodData<void(int, bool)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::Bool, 23 },
        }}),
        // Method 'setAppDirectLaunch'
        QtMocHelpers::MethodData<void(int, bool)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::Bool, 25 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AppModel, qt_meta_tag_ZN8AppModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AppModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractListModel::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8AppModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8AppModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8AppModelE_t>.metaTypes,
    nullptr
} };

void AppModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AppModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->computerLost(); break;
        case 1: _t->handleComputerStateChanged((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1]))); break;
        case 2: _t->handleBoxArtLoaded((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<NvApp>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QUrl>>(_a[3]))); break;
        case 3: _t->initialize((*reinterpret_cast< std::add_pointer_t<ComputerManager*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3]))); break;
        case 4: { Session* _r = _t->createSessionForApp((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< Session**>(_a[0]) = std::move(_r); }  break;
        case 5: { int _r = _t->getDirectLaunchAppIndex();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 6: { int _r = _t->getRunningAppId();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 7: { QString _r = _t->getRunningAppName();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 8: _t->quitRunningApp(); break;
        case 9: _t->setAppHidden((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 10: _t->setAppDirectLaunch((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< NvApp >(); break;
            }
            break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< ComputerManager* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AppModel::*)()>(_a, &AppModel::computerLost, 0))
            return;
    }
}

const QMetaObject *AppModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AppModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8AppModelE_t>.strings))
        return static_cast<void*>(this);
    return QAbstractListModel::qt_metacast(_clname);
}

int AppModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractListModel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void AppModel::computerLost()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
