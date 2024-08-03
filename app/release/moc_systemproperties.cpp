/****************************************************************************
** Meta object code from reading C++ file 'systemproperties.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.7.0)
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
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.7.0. It"
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

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSSystemPropertiesENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSSystemPropertiesENDCLASS = QtMocHelpers::stringData(
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
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSSystemPropertiesENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
      14,   55, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   44,    2, 0x06,   15 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,   45,    2, 0x02,   16 /* Public */,
       4,    1,   46,    2, 0x02,   17 /* Public */,
       6,    1,   49,    2, 0x02,   19 /* Public */,
       7,    1,   52,    2, 0x02,   21 /* Public */,

 // signals: parameters
    QMetaType::Void,

 // methods: parameters
    QMetaType::Void,
    QMetaType::QRect, QMetaType::Int,    5,
    QMetaType::QRect, QMetaType::Int,    5,
    QMetaType::Int, QMetaType::Int,    5,

 // properties: name, type, flags
       8, QMetaType::Bool, 0x00015401, uint(-1), 0,
       9, QMetaType::Bool, 0x00015401, uint(-1), 0,
      10, QMetaType::Bool, 0x00015401, uint(-1), 0,
      11, QMetaType::Bool, 0x00015401, uint(-1), 0,
      12, QMetaType::Bool, 0x00015401, uint(-1), 0,
      13, QMetaType::QString, 0x00015401, uint(-1), 0,
      14, QMetaType::Bool, 0x00015401, uint(-1), 0,
      15, QMetaType::Bool, 0x00015401, uint(-1), 0,
      16, QMetaType::Bool, 0x00015401, uint(-1), 0,
      17, QMetaType::QString, 0x00015003, uint(0), 0,
      18, QMetaType::QSize, 0x00015401, uint(-1), 0,
      19, QMetaType::QString, 0x00015401, uint(-1), 0,
      20, QMetaType::Bool, 0x00015401, uint(-1), 0,
      21, QMetaType::Bool, 0x00015401, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject SystemProperties::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSSystemPropertiesENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSSystemPropertiesENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSSystemPropertiesENDCLASS_t,
        // property 'hasHardwareAcceleration'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'rendererAlwaysFullScreen'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isRunningWayland'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isRunningXWayland'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isWow64'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'friendlyNativeArchName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'hasDesktopEnvironment'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'hasBrowser'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'hasDiscordIntegration'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'unmappedGamepads'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'maximumResolution'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'versionString'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'supportsHdr'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'usesMaterial3Theme'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<SystemProperties, std::true_type>,
        // method 'unmappedGamepadsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshDisplays'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'getNativeResolution'
        QtPrivate::TypeAndForceComplete<QRect, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getSafeAreaResolution'
        QtPrivate::TypeAndForceComplete<QRect, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getRefreshRate'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void SystemProperties::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SystemProperties *>(_o);
        (void)_t;
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
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (SystemProperties::*)();
            if (_t _q_method = &SystemProperties::unmappedGamepadsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    } else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<SystemProperties *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->hasHardwareAcceleration; break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->rendererAlwaysFullScreen; break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->isRunningWayland; break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->isRunningXWayland; break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->isWow64; break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->friendlyNativeArchName; break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->hasDesktopEnvironment; break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->hasBrowser; break;
        case 8: *reinterpret_cast< bool*>(_v) = _t->hasDiscordIntegration; break;
        case 9: *reinterpret_cast< QString*>(_v) = _t->unmappedGamepads; break;
        case 10: *reinterpret_cast< QSize*>(_v) = _t->maximumResolution; break;
        case 11: *reinterpret_cast< QString*>(_v) = _t->versionString; break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->supportsHdr; break;
        case 13: *reinterpret_cast< bool*>(_v) = _t->usesMaterial3Theme; break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<SystemProperties *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 9:
            if (_t->unmappedGamepads != *reinterpret_cast< QString*>(_v)) {
                _t->unmappedGamepads = *reinterpret_cast< QString*>(_v);
                Q_EMIT _t->unmappedGamepadsChanged();
            }
            break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *SystemProperties::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SystemProperties::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSSystemPropertiesENDCLASS.stringdata0))
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
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
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
