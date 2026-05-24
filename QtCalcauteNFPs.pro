QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

msvc {
    QMAKE_CFLAGS += /utf-8
    QMAKE_CXXFLAGS += /utf-8
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    nest/clipper/clipper.cpp \
    nest/nester.cpp \
    nest/nester_bottom_left.cpp \
    nest/nester_evaluation.cpp \
    nest/nester_greedy.cpp \
    nest/nester_simulated_annealing.cpp \
    nest/nfp_placer.cpp \
    nest/nfp_placer_minkowski.cpp \
    nest/nfp_placer_moving.cpp \
    nest/nfp_placer_vector.cpp \
    view/kwctrlview.cpp \
    view/segmentedtabbar.cpp \
    view/myctrlview.cpp \
    view/nestcanvasview.cpp \
    view/nestwindow.cpp \
    widget.cpp

HEADERS += \
    nest/clipper/clipper.hpp \
    nest/nester.h \
    nest/nest_scene.h \
    nest/nester_common.h \
    nest/nfp_placer_common.h \
    nest/nfp_placer.h \
    s_common.hpp \
    shapes/s_box.hpp \
    shapes/s_def.h \
    shapes/s_math.h \
    shapes/s_point.hpp \
    shapes/s_polyline.hpp \
    shapes/s_shape.h \
    shapes/utiltool.h \
    test.h \
    view/kwctrlview.h \
    view/segmentedtabbar.h \
    view/myctrlview.h \
    view/nestcanvasview.h \
    view/nestwindow.h \
    widget.h

FORMS += \
    widget.ui

RESOURCES += \
    resources/icons.qrc

LIBS+= -lopengl32 -lglu32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
