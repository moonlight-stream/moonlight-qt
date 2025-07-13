/****************************************************************************
** Meta object code from reading C++ file 'startstream.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../cli/startstream.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'startstream.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14CliStartStream8LauncherE_t {};
} // unnamed namespace

template <> constexpr inline auto CliStartStream::Launcher::qt_create_metaobjectdata<qt_meta_tag_ZN14CliStartStream8LauncherE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CliStartStream::Launcher",
        "searchingComputer",
        "",
        "searchingApp",
        "sessionCreated",
        "appName",
        "Session*",
        "session",
        "failed",
        "text",
        "appQuitRequired",
        "onComputerFound",
        "NvComputer*",
        "computer",
        "onComputerUpdated",
        "onTimeout",
        "onQuitAppCompleted",
        "QVariant",
        "error",
        "execute",
        "ComputerManager*",
        "manager",
        "quitRunningApp",
        "isExecuted"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'searchingComputer'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchingApp'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sessionCreated'
        QtMocHelpers::SignalData<void(QString, Session *)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 }, { 0x80000000 | 6, 7 },
        }}),
        // Signal 'failed'
        QtMocHelpers::SignalData<void(QString)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Signal 'appQuitRequired'
        QtMocHelpers::SignalData<void(QString)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Slot 'onComputerFound'
        QtMocHelpers::SlotData<void(NvComputer *)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Slot 'onComputerUpdated'
        QtMocHelpers::SlotData<void(NvComputer *)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Slot 'onTimeout'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onQuitAppCompleted'
        QtMocHelpers::SlotData<void(QVariant)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 17, 18 },
        }}),
        // Method 'execute'
        QtMocHelpers::MethodData<void(ComputerManager *)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 20, 21 },
        }}),
        // Method 'quitRunningApp'
        QtMocHelpers::MethodData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'isExecuted'
        QtMocHelpers::MethodData<bool() const>(23, 2, QMC::AccessPublic, QMetaType::Bool),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Launcher, qt_meta_tag_ZN14CliStartStream8LauncherE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject CliStartStream::Launcher::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14CliStartStream8LauncherE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14CliStartStream8LauncherE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14CliStartStream8LauncherE_t>.metaTypes,
    nullptr
} };

void CliStartStream::Launcher::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Launcher *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->searchingComputer(); break;
        case 1: _t->searchingApp(); break;
        case 2: _t->sessionCreated((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<Session*>>(_a[2]))); break;
        case 3: _t->failed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->appQuitRequired((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->onComputerFound((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1]))); break;
        case 6: _t->onComputerUpdated((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1]))); break;
        case 7: _t->onTimeout(); break;
        case 8: _t->onQuitAppCompleted((*reinterpret_cast< std::add_pointer_t<QVariant>>(_a[1]))); break;
        case 9: _t->execute((*reinterpret_cast< std::add_pointer_t<ComputerManager*>>(_a[1]))); break;
        case 10: _t->quitRunningApp(); break;
        case 11: { bool _r = _t->isExecuted();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)()>(_a, &Launcher::searchingComputer, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)()>(_a, &Launcher::searchingApp, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)(QString , Session * )>(_a, &Launcher::sessionCreated, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)(QString )>(_a, &Launcher::failed, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Launcher::*)(QString )>(_a, &Launcher::appQuitRequired, 4))
            return;
    }
}

const QMetaObject *CliStartStream::Launcher::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CliStartStream::Launcher::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14CliStartStream8LauncherE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CliStartStream::Launcher::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void CliStartStream::Launcher::searchingComputer()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void CliStartStream::Launcher::searchingApp()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void CliStartStream::Launcher::sessionCreated(QString _t1, Session * _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void CliStartStream::Launcher::failed(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void CliStartStream::Launcher::appQuitRequired(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
