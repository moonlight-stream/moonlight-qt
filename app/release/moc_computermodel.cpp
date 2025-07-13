/****************************************************************************
** Meta object code from reading C++ file 'computermodel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../gui/computermodel.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'computermodel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13ComputerModelE_t {};
} // unnamed namespace

template <> constexpr inline auto ComputerModel::qt_create_metaobjectdata<qt_meta_tag_ZN13ComputerModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ComputerModel",
        "pairingCompleted",
        "",
        "QVariant",
        "error",
        "connectionTestCompleted",
        "result",
        "blockedPorts",
        "handleComputerStateChanged",
        "NvComputer*",
        "computer",
        "handlePairingCompleted",
        "initialize",
        "ComputerManager*",
        "computerManager",
        "deleteComputer",
        "computerIndex",
        "generatePinString",
        "pairComputer",
        "pin",
        "testConnectionForComputer",
        "wakeComputer",
        "renameComputer",
        "name",
        "createSessionForCurrentGame",
        "Session*"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'pairingCompleted'
        QtMocHelpers::SignalData<void(QVariant)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'connectionTestCompleted'
        QtMocHelpers::SignalData<void(int, QString)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { QMetaType::QString, 7 },
        }}),
        // Slot 'handleComputerStateChanged'
        QtMocHelpers::SlotData<void(NvComputer *)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Slot 'handlePairingCompleted'
        QtMocHelpers::SlotData<void(NvComputer *, QString)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 9, 10 }, { QMetaType::QString, 4 },
        }}),
        // Method 'initialize'
        QtMocHelpers::MethodData<void(ComputerManager *)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Method 'deleteComputer'
        QtMocHelpers::MethodData<void(int)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Method 'generatePinString'
        QtMocHelpers::MethodData<QString()>(17, 2, QMC::AccessPublic, QMetaType::QString),
        // Method 'pairComputer'
        QtMocHelpers::MethodData<void(int, QString)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 16 }, { QMetaType::QString, 19 },
        }}),
        // Method 'testConnectionForComputer'
        QtMocHelpers::MethodData<void(int)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Method 'wakeComputer'
        QtMocHelpers::MethodData<void(int)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Method 'renameComputer'
        QtMocHelpers::MethodData<void(int, QString)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 16 }, { QMetaType::QString, 23 },
        }}),
        // Method 'createSessionForCurrentGame'
        QtMocHelpers::MethodData<Session *(int)>(24, 2, QMC::AccessPublic, 0x80000000 | 25, {{
            { QMetaType::Int, 16 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ComputerModel, qt_meta_tag_ZN13ComputerModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ComputerModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractListModel::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ComputerModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ComputerModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13ComputerModelE_t>.metaTypes,
    nullptr
} };

void ComputerModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ComputerModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->pairingCompleted((*reinterpret_cast< std::add_pointer_t<QVariant>>(_a[1]))); break;
        case 1: _t->connectionTestCompleted((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->handleComputerStateChanged((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1]))); break;
        case 3: _t->handlePairingCompleted((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->initialize((*reinterpret_cast< std::add_pointer_t<ComputerManager*>>(_a[1]))); break;
        case 5: _t->deleteComputer((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: { QString _r = _t->generatePinString();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 7: _t->pairComputer((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->testConnectionForComputer((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->wakeComputer((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 10: _t->renameComputer((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: { Session* _r = _t->createSessionForCurrentGame((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< Session**>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< ComputerManager* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ComputerModel::*)(QVariant )>(_a, &ComputerModel::pairingCompleted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ComputerModel::*)(int , QString )>(_a, &ComputerModel::connectionTestCompleted, 1))
            return;
    }
}

const QMetaObject *ComputerModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ComputerModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ComputerModelE_t>.strings))
        return static_cast<void*>(this);
    return QAbstractListModel::qt_metacast(_clname);
}

int ComputerModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractListModel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void ComputerModel::pairingCompleted(QVariant _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ComputerModel::connectionTestCompleted(int _t1, QString _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}
QT_WARNING_POP
