cmake_minimum_required(VERSION 3.6)
project(vgo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cài vào đúng thư mục fcitx5 của distro (/usr/lib/.../fcitx5, /usr/share/fcitx5)
# — nơi fcitx5 chắc chắn quét, cùng chỗ Lotus. Tránh /usr/local (có thể không nạp).
set(FCITX_INSTALL_USE_FCITX_SYS_PATHS ON CACHE BOOL "Dùng thư mục cài của fcitx5 distro")

find_package(ECM 1.0.0 REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Utils REQUIRED)

# Addon shared library (engine thuần + lớp bọc fcitx5), in-process.
add_library(vgo SHARED
    src/engine.cpp
    fcitx5/vgoime.cpp
)
set_target_properties(vgo PROPERTIES PREFIX "lib")
target_include_directories(vgo PRIVATE src)
target_link_libraries(vgo Fcitx5::Core Fcitx5::Utils)

install(TARGETS vgo LIBRARY DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")
install(FILES "${CMAKE_SOURCE_DIR}/fcitx5/vgo-addon.conf" RENAME vgo.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES "${CMAKE_SOURCE_DIR}/fcitx5/vgo.conf"
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
# Từ điển base (nhận diện tiếng Anh) -> /usr/share/vgo/en-base.txt
install(FILES "${CMAKE_SOURCE_DIR}/data/en-base.txt"
        DESTINATION "${FCITX_INSTALL_DATADIR}/vgo")
install(FILES "${CMAKE_SOURCE_DIR}/fcitx5/vgo.svg" RENAME fcitx-vgo.svg
        DESTINATION "${FCITX_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
# Cập nhật icon-theme cache để fcitx5 thấy icon mới (nếu không, hiện icon mặc định).
install(CODE "execute_process(COMMAND gtk-update-icon-cache -f -q \"${FCITX_INSTALL_DATADIR}/icons/hicolor\" ERROR_QUIET)")
