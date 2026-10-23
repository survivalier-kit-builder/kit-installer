/****************************************************************************
** Meta object code from reading C++ file 'installerwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/installer/installerwindow.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'installerwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_InstallerWindow_t {
    uint offsetsAndSizes[16];
    char stringdata0[16];
    char stringdata1[7];
    char stringdata2[1];
    char stringdata3[7];
    char stringdata4[9];
    char stringdata5[9];
    char stringdata6[14];
    char stringdata7[4];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_InstallerWindow_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_InstallerWindow_t qt_meta_stringdata_InstallerWindow = {
    {
        QT_MOC_LITERAL(0, 15),  // "InstallerWindow"
        QT_MOC_LITERAL(16, 6),  // "onNext"
        QT_MOC_LITERAL(23, 0),  // ""
        QT_MOC_LITERAL(24, 6),  // "onBack"
        QT_MOC_LITERAL(31, 8),  // "onCancel"
        QT_MOC_LITERAL(40, 8),  // "onBrowse"
        QT_MOC_LITERAL(49, 13),  // "onLanguageRow"
        QT_MOC_LITERAL(63, 3)   // "row"
    },
    "InstallerWindow",
    "onNext",
    "",
    "onBack",
    "onCancel",
    "onBrowse",
    "onLanguageRow",
    "row"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_InstallerWindow[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   44,    2, 0x08,    1 /* Private */,
       3,    0,   45,    2, 0x08,    2 /* Private */,
       4,    0,   46,    2, 0x08,    3 /* Private */,
       5,    0,   47,    2, 0x08,    4 /* Private */,
       6,    1,   48,    2, 0x08,    5 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    7,

       0        // eod
};

Q_CONSTINIT const QMetaObject InstallerWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_InstallerWindow.offsetsAndSizes,
    qt_meta_data_InstallerWindow,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_InstallerWindow_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<InstallerWindow, std::true_type>,
        // method 'onNext'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onBack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCancel'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onBrowse'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLanguageRow'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void InstallerWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<InstallerWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onNext(); break;
        case 1: _t->onBack(); break;
        case 2: _t->onCancel(); break;
        case 3: _t->onBrowse(); break;
        case 4: _t->onLanguageRow((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *InstallerWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *InstallerWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_InstallerWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int InstallerWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
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
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
