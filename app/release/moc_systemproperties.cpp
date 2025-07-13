/****************************************************************************
** Meta object code from reading C++ file 'systemproperties.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../backend/systemproperties.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'systemproperties.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16SystemPropertiesE_t {};
} // unnamed namespace

template <> constexpr inline auto SystemProperties::qt_create_metaobjectdata<qt_meta_tag_ZN16SystemPropertiesE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SystemProperties",
        "unmappedGamepadsChanged",
        "",
        "refreshDisplays",
        "getNativeResolution",
        "displayIndex",
        "getSafeAreaResolution",
        "getRefreshRate",
        "hasHardwareAcceleration",
        "rendererAlwaysFullScreen",
        "isRunningWayland",
        "isRunningXWayland",
        "isWow64",
        "friendlyNativeArchName",
        "hasDesktopEnvironment",
        "hasBrowser",
        "hasDiscordIntegration",
        "unmappedGamepads",
        "maximumResolution",
        "versionString",
        "supportsHdr",
        "usesMaterial3Theme"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'unmappedGamepadsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'refreshDisplays'
        QtMocHelpers::MethodData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'getNativeResolution'
        QtMocHelpers::MethodData<QRect(int)>(4, 2, QMC::AccessPublic, QMetaType::QRect, {{
            { QMetaType::Int, 5 },
        }}),
        // Method 'getSafeAreaResolution'
        QtMocHelpers::MethodData<QRect(int)>(6, 2, QMC::AccessPublic, QMetaType::QRect, {{
            { QMetaType::Int, 5 },
        }}),
        // Method 'getRefreshRate'
        QtMocHelpers::MethodData<int(int)>(7, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 5 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'hasHardwareAcceleration'
        QtMocHelpers::PropertyData<bool>(8, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'rendererAlwaysFullScreen'
        QtMocHelpers::PropertyData<bool>(9, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'isRunningWayland'
        QtMocHelpers::PropertyData<bool>(10, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'isRunningXWayland'
        QtMocHelpers::PropertyData<bool>(11, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'isWow64'
        QtMocHelpers::PropertyData<bool>(12, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'friendlyNativeArchName'
        QtMocHelpers::PropertyData<QString>(13, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'hasDesktopEnvironment'
        QtMocHelpers::PropertyData<bool>(14, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'hasBrowser'
        QtMocHelpers::PropertyData<bool>(15, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'hasDiscordIntegration'
        QtMocHelpers::PropertyData<bool>(16, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'unmappedGamepads'
        QtMocHelpers::PropertyData<QString>(17, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable, 0),
        // property 'maximumResolution'
        QtMocHelpers::PropertyData<QSize>(18, QMetaType::QSize, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'versionString'
        QtMocHelpers::PropertyData<QString>(19, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'supportsHdr'
        QtMocHelpers::PropertyData<bool>(20, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'usesMaterial3Theme'
        QtMocHelpers::PropertyData<bool>(21, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SystemProperties, qt_meta_tag_ZN16SystemPropertiesE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SystemProperties::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SystemPropertiesE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SystemPropertiesE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16SystemPropertiesE_t>.metaTypes,
    nullptr
} };

void SystemProperties::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SystemProperties *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->unmappedGamepadsChanged(); break;
        case 1: _t->refreshDisplays(); break;
        case 2: { QRect _r = _t->getNativeResolution((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QRect*>(_a[0]) = std::move(_r); }  break;
        case 3: { QRect _r = _t->getSafeAreaResolution((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QRect*>(_a[0]) = std::move(_r); }  break;
        case 4: { int _r = _t->getRefreshRate((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SystemProperties::*)()>(_a, &SystemProperties::unmappedGamepadsChanged, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<bool*>(_v) = _t->hasHardwareAcceleration; break;
        case 1: *reinterpret_cast<bool*>(_v) = _t->rendererAlwaysFullScreen; break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->isRunningWayland; break;
        case 3: *reinterpret_cast<bool*>(_v) = _t->isRunningXWayland; break;
        case 4: *reinterpret_cast<bool*>(_v) = _t->isWow64; break;
        case 5: *reinterpret_cast<QString*>(_v) = _t->friendlyNativeArchName; break;
        case 6: *reinterpret_cast<bool*>(_v) = _t->hasDesktopEnvironment; break;
        case 7: *reinterpret_cast<bool*>(_v) = _t->hasBrowser; break;
        case 8: *reinterpret_cast<bool*>(_v) = _t->hasDiscordIntegration; break;
        case 9: *reinterpret_cast<QString*>(_v) = _t->unmappedGamepads; break;
        case 10: *reinterpret_cast<QSize*>(_v) = _t->maximumResolution; break;
        case 11: *reinterpret_cast<QString*>(_v) = _t->versionString; break;
        case 12: *reinterpret_cast<bool*>(_v) = _t->supportsHdr; break;
        case 13: *reinterpret_cast<bool*>(_v) = _t->usesMaterial3Theme; break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 9:
            if (QtMocHelpers::setProperty(_t->unmappedGamepads, *reinterpret_cast<QString*>(_v)))
                Q_EMIT _t->unmappedGamepadsChanged();
            break;
        default: break;
        }
    }
}

const QMetaObject *SystemProperties::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SystemProperties::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SystemPropertiesE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SystemProperties::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void SystemProperties::unmappedGamepadsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
