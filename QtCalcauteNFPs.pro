QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    actions/s_action.cpp \
    main.cpp \
    nest/clipper/clipper.cpp \
    nest/nesttools.cpp \
    nest/nfpplacer.cpp \
    shapes/s_box.cpp \
    shapes/s_document.cpp \
    shapes/s_math.cpp \
    shapes/s_point.cpp \
    shapes/s_polyline.cpp \
    shapes/s_shape.cpp \
    shapes/s_shapecontainer.cpp \
    view/kwctrlview.cpp \
    view/myctrlview.cpp \
    widget.cpp

HEADERS += \
    actions/s_action.h \
    nest/clipper/clipper.hpp \
    nest/nfpplacer.h \
    s_common.hpp \
    nest/nesttools.h \
    shapes/s_box.hpp \
    shapes/s_def.h \
    shapes/s_document.h \
    shapes/s_math.h \
    shapes/s_point.hpp \
    shapes/s_polyline.hpp \
    shapes/s_shape.h \
    shapes/s_shapecontainer.h \
    shapes/utiltool.h \
    test.h \
    view/kwctrlview.h \
    view/myctrlview.h \
    widget.h

FORMS += \
    widget.ui

LIBS+= -lopengl32 -lglu32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
