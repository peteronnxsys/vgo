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

# Repo này có thể chỉ chứa Telex (src/engine.cpp), chỉ VNI (src/vni.cpp) hoặc cả hai:
# có file nào thì build bộ gõ đó. Gộp 2 gói = chép chung một thư mục là xong.
# Ẩn symbol (chỉ export factory của fcitx): libvgo + libvgo-vni cùng chứa lõi viet.cpp,
# nạp chung một tiến trình fcitx5 thì không được gọi nhầm hàm của nhau.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(ICON_DIR "${FCITX_INSTALL_DATADIR}/icons/hicolor/scalable/apps")

if(EXISTS "${CMAKE_SOURCE_DIR}/src/engine.cpp")
    # vgo (Telex): engine thuần + lớp bọc fcitx5, in-process.
    add_library(vgo SHARED
        src/engine.cpp
        src/viet.cpp
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
    # Từ điển base (Anh) + danh sách ép-Việt + bigram + âm tiết Việt -> /usr/share/vgo/
    install(FILES "${CMAKE_SOURCE_DIR}/data/en-base.txt"
                  "${CMAKE_SOURCE_DIR}/data/vn-base.txt"
                  "${CMAKE_SOURCE_DIR}/data/bigram-base.txt"
                  "${CMAKE_SOURCE_DIR}/data/vn-syllables.txt"
            DESTINATION "${FCITX_INSTALL_DATADIR}/vgo")
    install(FILES "${CMAKE_SOURCE_DIR}/fcitx5/vgo.svg" RENAME fcitx-vgo.svg
            DESTINATION "${ICON_DIR}")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/vni.cpp")
    # vgo-vni: module VNI riêng (không từ điển / tự học), cài + gỡ độc lập với vgo.
    add_library(vgo-vni SHARED
        src/vni.cpp
        src/viet.cpp
        fcitx5/vniime.cpp
    )
    set_target_properties(vgo-vni PROPERTIES PREFIX "lib")
    target_include_directories(vgo-vni PRIVATE src)
    target_link_libraries(vgo-vni Fcitx5::Core Fcitx5::Utils)

    install(TARGETS vgo-vni LIBRARY DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")
    install(FILES "${CMAKE_SOURCE_DIR}/fcitx5/vgo-vni-addon.conf" RENAME vgo-vni.conf
            DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
    install(FILES "${CMAKE_SOURCE_DIR}/fcitx5/vgo-vni.conf"
            DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
    install(FILES "${CMAKE_SOURCE_DIR}/fcitx5/vgo.svg" RENAME fcitx-vgo-vni.svg
            DESTINATION "${ICON_DIR}")
endif()

# Cập nhật icon-theme cache để fcitx5 thấy icon mới (nếu không, hiện icon mặc định).
install(CODE "execute_process(COMMAND gtk-update-icon-cache -f -q \"${FCITX_INSTALL_DATADIR}/icons/hicolor\" ERROR_QUIET)")
