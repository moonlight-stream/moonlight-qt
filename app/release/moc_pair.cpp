/****************************************************************************
** Meta object code from reading C++ file 'pair.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../cli/pair.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'pair.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7CliPair8LauncherE_t {};
} // unnamed namespace

template <> constexpr inline auto CliPair::Launcher::qt_create_metaobjectdata<qt_meta_tag_ZN7CliPair8LauncherE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CliPair::Launcher",
        "searchingComputer",
        "",
        "pairing",
        "pcName",
        "pin",
        "failed",
        "text",
        "success",
        "onComputerFound",
        "NvComputer*",
        "computer",
        "onPairingCompleted",
        "error",
        "onTimeout",
        "execute",
        "ComputerManager*",
        "manager",
        "isExecuted"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'searchingComputer'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'pairing'
        QtMocHelpers::SignalData<void(QString, QString)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 }, { QMetaType::QString, 5 },
        }}),
        // Signal 'failed'
        QtMocHelpers::SignalData<void(QString)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Signal 'success'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onComputerFound'
        QtMocHelpers::SlotData<void(NvComputer *)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Slot 'onPairingCompleted'
        QtMocHelpers::SlotData<void(NvComputer *, QString)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 10, 11 }, { QMetaType::QString, 13 },
        }}),
        // Slot 'onTimeout'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Method 'execute'
        QtMocHelpers::MethodData<void(ComputerManager *)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Method 'isExecuted'
        QtMocHelpers::MethodData<bool() const>(18, 2, QMC::AccessPublic, QMetaType::Bool),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Launcher, qt_meta_tag_ZN7CliPair8LauncherE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject CliPair::Launcher::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7CliPair8LauncherE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7CliPair8LauncherE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7CliPair8LauncherE_t>.metaTypes,
    nullptr
} };

void CliPair::Launcher::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Launcher *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->searchingComputer(); break;
        case 1: _t->pairing((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->failed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->success(); break;
        case 4: _t->onComputerFound((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1]))); break;
        case 5: _t->onPairingCompleted((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->onTimeout(); break;
        case 7: _t->execute((*reinterpret_cast< std::add_pointer_t<ComputerManager*>>(_a[1]))); break;
        case 8: { bool _r = _t->isExecuted();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)()>(_a, &Launcher::searchingComputer, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)(QString , QString )>(_a, &Launcher::pairing, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)(QString )>(_a, &Launcher::failed, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)()>(_a, &Launcher::success, 3))
            return;
    }
}

const QMetaObject *CliPair::Launcher::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CliPair::Launcher::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7CliPair8LauncherE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CliPair::Launcher::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void CliPair::Launcher::searchingComputer()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void CliPair::Launcher::pairing(QString _t1, QString _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void CliPair::Launcher::failed(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void CliPair::Launcher::success()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
