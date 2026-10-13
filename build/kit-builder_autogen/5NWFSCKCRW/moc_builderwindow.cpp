/****************************************************************************
** Meta object code from reading C++ file 'builderwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/builder/builderwindow.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'builderwindow.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_BuilderWindow_t {
    uint offsetsAndSizes[28];
    char stringdata0[14];
    char stringdata1[8];
    char stringdata2[1];
    char stringdata3[9];
    char stringdata4[12];
    char stringdata5[11];
    char stringdata6[14];
    char stringdata7[15];
    char stringdata8[12];
    char stringdata9[11];
    char stringdata10[11];
    char stringdata11[12];
    char stringdata12[14];
    char stringdata13[6];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_BuilderWindow_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_BuilderWindow_t qt_meta_stringdata_BuilderWindow = {
    {
        QT_MOC_LITERAL(0, 13),  // "BuilderWindow"
        QT_MOC_LITERAL(14, 7),  // "pickExe"
        QT_MOC_LITERAL(22, 0),  // ""
        QT_MOC_LITERAL(23, 8),  // "pickIcon"
        QT_MOC_LITERAL(32, 11),  // "pickLicense"
        QT_MOC_LITERAL(44, 10),  // "pickOutput"
        QT_MOC_LITERAL(55, 13),  // "updatePreview"
        QT_MOC_LITERAL(69, 14),  // "refreshGpgKeys"
        QT_MOC_LITERAL(84, 11),  // "updateGpgUi"
        QT_MOC_LITERAL(96, 10),  // "newProject"
        QT_MOC_LITERAL(107, 10),  // "openDialog"
        QT_MOC_LITERAL(118, 11),  // "saveProject"
        QT_MOC_LITERAL(130, 13),  // "saveProjectAs"
        QT_MOC_LITERAL(144, 5)   // "build"
    },
    "BuilderWindow",
    "pickExe",
    "",
    "pickIcon",
    "pickLicense",
    "pickOutput",
    "updatePreview",
    "refreshGpgKeys",
    "updateGpgUi",
    "newProject",
    "openDialog",
    "saveProject",
    "saveProjectAs",
    "build"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_BuilderWindow[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   86,    2, 0x08,    1 /* Private */,
       3,    0,   87,    2, 0x08,    2 /* Private */,
       4,    0,   88,    2, 0x08,    3 /* Private */,
       5,    0,   89,    2, 0x08,    4 /* Private */,
       6,    0,   90,    2, 0x08,    5 /* Private */,
       7,    0,   91,    2, 0x08,    6 /* Private */,
       8,    0,   92,    2, 0x08,    7 /* Private */,
       9,    0,   93,    2, 0x08,    8 /* Private */,
      10,    0,   94,    2, 0x08,    9 /* Private */,
      11,    0,   95,    2, 0x08,   10 /* Private */,
      12,    0,   96,    2, 0x08,   11 /* Private */,
      13,    0,   97,    2, 0x08,   12 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject BuilderWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_BuilderWindow.offsetsAndSizes,
    qt_meta_data_BuilderWindow,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_BuilderWindow_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<BuilderWindow, std::true_type>,
        // method 'pickExe'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'pickIcon'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'pickLicense'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'pickOutput'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updatePreview'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshGpgKeys'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateGpgUi'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'newProject'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveProject'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'saveProjectAs'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'build'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void BuilderWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<BuilderWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->pickExe(); break;
        case 1: _t->pickIcon(); break;
        case 2: _t->pickLicense(); break;
        case 3: _t->pickOutput(); break;
        case 4: _t->updatePreview(); break;
        case 5: _t->refreshGpgKeys(); break;
        case 6: _t->updateGpgUi(); break;
        case 7: _t->newProject(); break;
        case 8: _t->openDialog(); break;
        case 9: { bool _r = _t->saveProject();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 10: { bool _r = _t->saveProjectAs();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 11: _t->build(); break;
        default: ;
        }
    }
}

const QMetaObject *BuilderWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BuilderWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_BuilderWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int BuilderWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
