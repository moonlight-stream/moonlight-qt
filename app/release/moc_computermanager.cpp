/****************************************************************************
** Meta object code from reading C++ file 'computermanager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../backend/computermanager.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'computermanager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18DelayedFlushThreadE_t {};
} // unnamed namespace

template <> constexpr inline auto DelayedFlushThread::qt_create_metaobjectdata<qt_meta_tag_ZN18DelayedFlushThreadE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DelayedFlushThread"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DelayedFlushThread, qt_meta_tag_ZN18DelayedFlushThreadE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DelayedFlushThread::staticMetaObject = { {
    QMetaObject::SuperData::link<QThread::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DelayedFlushThreadE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DelayedFlushThreadE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18DelayedFlushThreadE_t>.metaTypes,
    nullptr
} };

void DelayedFlushThread::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DelayedFlushThread *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *DelayedFlushThread::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DelayedFlushThread::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DelayedFlushThreadE_t>.strings))
        return static_cast<void*>(this);
    return QThread::qt_metacast(_clname);
}

int DelayedFlushThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN19MdnsPendingComputerE_t {};
} // unnamed namespace

template <> constexpr inline auto MdnsPendingComputer::qt_create_metaobjectdata<qt_meta_tag_ZN19MdnsPendingComputerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MdnsPendingComputer",
        "resolvedHost",
        "",
        "MdnsPendingComputer*",
        "QList<QHostAddress>&",
        "handleResolvedTimeout",
        "handleResolvedAddress",
        "QHostAddress",
        "address"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'resolvedHost'
        QtMocHelpers::SignalData<void(MdnsPendingComputer *, QVector<QHostAddress> &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 }, { 0x80000000 | 4, 2 },
        }}),
        // Slot 'handleResolvedTimeout'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleResolvedAddress'
        QtMocHelpers::SlotData<void(const QHostAddress &)>(6, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MdnsPendingComputer, qt_meta_tag_ZN19MdnsPendingComputerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MdnsPendingComputer::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19MdnsPendingComputerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19MdnsPendingComputerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19MdnsPendingComputerE_t>.metaTypes,
    nullptr
} };

void MdnsPendingComputer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MdnsPendingComputer *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->resolvedHost((*reinterpret_cast< std::add_pointer_t<MdnsPendingComputer*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QList<QHostAddress>&>>(_a[2]))); break;
        case 1: _t->handleResolvedTimeout(); break;
        case 2: _t->handleResolvedAddress((*reinterpret_cast< std::add_pointer_t<QHostAddress>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< MdnsPendingComputer* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MdnsPendingComputer::*)(MdnsPendingComputer * , QVector<QHostAddress> & )>(_a, &MdnsPendingComputer::resolvedHost, 0))
            return;
    }
}

const QMetaObject *MdnsPendingComputer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MdnsPendingComputer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19MdnsPendingComputerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int MdnsPendingComputer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void MdnsPendingComputer::resolvedHost(MdnsPendingComputer * _t1, QVector<QHostAddress> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}
namespace {
struct qt_meta_tag_ZN15ComputerManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto ComputerManager::qt_create_metaobjectdata<qt_meta_tag_ZN15ComputerManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ComputerManager",
        "computerStateChanged",
        "",
        "NvComputer*",
        "computer",
        "pairingCompleted",
        "error",
        "computerAddCompleted",
        "QVariant",
        "success",
        "detectedPortBlocking",
        "quitAppCompleted",
        "handleAboutToQuit",
        "handleComputerStateChanged",
        "handleMdnsServiceResolved",
        "MdnsPendingComputer*",
        "QList<QHostAddress>&",
        "addresses",
        "startPolling",
        "stopPollingAsync",
        "addNewHostManually",
        "address"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'computerStateChanged'
        QtMocHelpers::SignalData<void(NvComputer *)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'pairingCompleted'
        QtMocHelpers::SignalData<void(NvComputer *, QString)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'computerAddCompleted'
        QtMocHelpers::SignalData<void(QVariant, QVariant)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 9 }, { 0x80000000 | 8, 10 },
        }}),
        // Signal 'quitAppCompleted'
        QtMocHelpers::SignalData<void(QVariant)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 6 },
        }}),
        // Slot 'handleAboutToQuit'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleComputerStateChanged'
        QtMocHelpers::SlotData<void(NvComputer *)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'handleMdnsServiceResolved'
        QtMocHelpers::SlotData<void(MdnsPendingComputer *, QVector<QHostAddress> &)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 15, 4 }, { 0x80000000 | 16, 17 },
        }}),
        // Method 'startPolling'
        QtMocHelpers::MethodData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'stopPollingAsync'
        QtMocHelpers::MethodData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'addNewHostManually'
        QtMocHelpers::MethodData<void(QString)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ComputerManager, qt_meta_tag_ZN15ComputerManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ComputerManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ComputerManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ComputerManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15ComputerManagerE_t>.metaTypes,
    nullptr
} };

void ComputerManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ComputerManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->computerStateChanged((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1]))); break;
        case 1: _t->pairingCompleted((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->computerAddCompleted((*reinterpret_cast< std::add_pointer_t<QVariant>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QVariant>>(_a[2]))); break;
        case 3: _t->quitAppCompleted((*reinterpret_cast< std::add_pointer_t<QVariant>>(_a[1]))); break;
        case 4: _t->handleAboutToQuit(); break;
        case 5: _t->handleComputerStateChanged((*reinterpret_cast< std::add_pointer_t<NvComputer*>>(_a[1]))); break;
        case 6: _t->handleMdnsServiceResolved((*reinterpret_cast< std::add_pointer_t<MdnsPendingComputer*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QList<QHostAddress>&>>(_a[2]))); break;
        case 7: _t->startPolling(); break;
        case 8: _t->stopPollingAsync(); break;
        case 9: _t->addNewHostManually((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< MdnsPendingComputer* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ComputerManager::*)(NvComputer * )>(_a, &ComputerManager::computerStateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ComputerManager::*)(NvComputer * , QString )>(_a, &ComputerManager::pairingCompleted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ComputerManager::*)(QVariant , QVariant )>(_a, &ComputerManager::computerAddCompleted, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ComputerManager::*)(QVariant )>(_a, &ComputerManager::quitAppCompleted, 3))
            return;
    }
}

const QMetaObject *ComputerManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ComputerManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ComputerManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ComputerManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void ComputerManager::computerStateChanged(NvComputer * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ComputerManager::pairingCompleted(NvComputer * _t1, QString _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void ComputerManager::computerAddCompleted(QVariant _t1, QVariant _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void ComputerManager::quitAppCompleted(QVariant _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
