# Map library.

TARGET = map
TEMPLATE = lib
CONFIG += staticlib warn_on

ROOT_DIR = ..

INCLUDEPATH *= $$ROOT_DIR/3party/protobuf/src $$ROOT_DIR/3party/freetype/include $$ROOT_DIR/3party/jansson/src

include($$ROOT_DIR/common.pri)

HEADERS += \
    api_mark_point.hpp \
    benchmark_tools.hpp \
    bookmark.hpp \
    bookmark_manager.hpp \
    chart_generator.hpp \
    displacement_mode_manager.hpp \
    feature_vec_model.hpp \
    framework.hpp \
    ge0_parser.hpp \
    geourl_process.hpp \
    gps_track.hpp \
    gps_track_collection.hpp \
    gps_track_filter.hpp \
    gps_track_storage.hpp \
    gps_tracker.hpp \
    mwm_url.hpp \
    place_page_info.hpp \
    reachable_by_taxi_checker.hpp \
    track.hpp \
    traffic_manager.hpp \
    user_mark.hpp \
    user_mark_container.hpp \

SOURCES += \
    ../api/src/c/api-client.c \
    address_finder.cpp \
    api_mark_point.cpp \
    benchmark_tools.cpp \
    bookmark.cpp \
    bookmark_manager.cpp \
    chart_generator.cpp \
    displacement_mode_manager.cpp \
    feature_vec_model.cpp \
    framework.cpp \
    ge0_parser.cpp \
    geourl_process.cpp \
    gps_track.cpp \
    gps_track_collection.cpp \
    gps_track_filter.cpp \
    gps_track_storage.cpp \
    gps_tracker.cpp \
    mwm_url.cpp \
    place_page_info.cpp \
    reachable_by_taxi_checker.cpp \
    track.cpp \
    traffic_manager.cpp \
    user_mark.cpp \
    user_mark_container.cpp \

!iphone*:!tizen*:!android* {
  QT += opengl
}
