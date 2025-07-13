/****************************************************************************
** Meta object code from reading C++ file 'sdlgamepadkeynavigation.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../gui/sdlgamepadkeynavigation.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'sdlgamepadkeynavigation.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN23SdlGamepadKeyNavigationE_t {};
} // unnamed namespace

template <> constexpr inline auto SdlGamepadKeyNavigation::qt_create_metaobjectdata<qt_meta_tag_ZN23SdlGamepadKeyNavigationE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SdlGamepadKeyNavigation",
        "onPollingTimerFired",
        "",
        "enable",
        "disable",
        "notifyWindowFocus",
        "hasFocus",
        "setUiNavMode",
        "settingsMode",
        "getConnectedGamepads"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onPollingTimerFired'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Method 'enable'
        QtMocHelpers::MethodData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'disable'
        QtMocHelpers::MethodData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'notifyWindowFocus'
        QtMocHelpers::MethodData<void(bool)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 6 },
        }}),
        // Method 'setUiNavMode'
        QtMocHelpers::MethodData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Method 'getConnectedGamepads'
        QtMocHelpers::MethodData<int()>(9, 2, QMC::AccessPublic, QMetaType::Int),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SdlGamepadKeyNavigation, qt_meta_tag_ZN23SdlGamepadKeyNavigationE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SdlGamepadKeyNavigation::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SdlGamepadKeyNavigationE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SdlGamepadKeyNavigationE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN23SdlGamepadKeyNavigationE_t>.metaTypes,
    nullptr
} };

void SdlGamepadKeyNavigation::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SdlGamepadKeyNavigation *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onPollingTimerFired(); break;
        case 1: _t->enable(); break;
        case 2: _t->disable(); break;
        case 3: _t->notifyWindowFocus((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->setUiNavMode((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: { int _r = _t->getConnectedGamepads();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
}

const QMetaObject *SdlGamepadKeyNavigation::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SdlGamepadKeyNavigation::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23SdlGamepadKeyNavigationE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SdlGamepadKeyNavigation::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}
QT_WARNING_POP
