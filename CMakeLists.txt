cmake_minimum_required(VERSION 3.16)
project(KitInstaller VERSION 2.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_ENABLE_EXPORTS ON)   # noms de fonctions lisibles dans les rapports de plantage
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
option(KIT_SANITIZE "Compiler avec AddressSanitizer/UBSan (débogage)" OFF)
if(KIT_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# --- Runtime AppImage (type 2, licence MIT) embarqué dans kit-builder -------------
# Téléchargé une seule fois depuis GitHub au configure, puis intégré au binaire :
# l'utilisateur final n'a besoin d'aucun outil externe.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(RUNTIME_ARCH aarch64)
else()
    set(RUNTIME_ARCH x86_64)
endif()
set(RUNTIME_FILE ${CMAKE_CURRENT_SOURCE_DIR}/resources/runtime)
if(NOT EXISTS ${RUNTIME_FILE})
    message(STATUS "Téléchargement du runtime AppImage (${RUNTIME_ARCH})...")
    file(DOWNLOAD
        https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-${RUNTIME_ARCH}
        ${RUNTIME_FILE} STATUS dl_status TLS_VERIFY ON)
    list(GET dl_status 0 dl_code)
    if(NOT dl_code EQUAL 0)
        file(REMOVE ${RUNTIME_FILE})
        message(FATAL_ERROR "Échec du téléchargement du runtime. Placez manuellement le fichier "
            "runtime-${RUNTIME_ARCH} (AppImage/type2-runtime) dans resources/runtime")
    endif()
endif()

# --- Langues : TOUTES les langues de languages/*.lang sont intégrées à l'installeur (le développeur n'en choisit pas)
file(GLOB LANG_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/languages/*.lang)
set(LANG_QRC_ENTRIES "")
foreach(f ${LANG_FILES})
    get_filename_component(lang_name ${f} NAME)
    if(NOT lang_name MATCHES "^_")          # _template.lang = modèle, pas une langue
        string(APPEND LANG_QRC_ENTRIES "    <file alias=\"lang/${lang_name}\">${f}</file>\n")
    endif()
endforeach()
configure_file(resources/installer.qrc.in ${CMAKE_BINARY_DIR}/installer.qrc @ONLY)
configure_file(resources/builder.qrc.in ${CMAKE_BINARY_DIR}/builder.qrc @ONLY)

# --- Cœur en C : manifeste, copie, .desktop, désinstalleur, écrivain SquashFS/AppImage
add_library(kitcore STATIC src/core/kit_core.c src/core/kit_appimage.c)
target_include_directories(kitcore PUBLIC src/core)
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_compile_definitions(kitcore PRIVATE KIT_HAVE_ZLIB)
    target_link_libraries(kitcore PUBLIC ZLIB::ZLIB)
else()
    message(WARNING "zlib introuvable : les AppImage générées ne seront pas compressées.")
endif()

# 1) Kit Builder : interface + mode console (--cli)
add_executable(kit-builder
    src/builder/main.cpp src/builder/builderwindow.cpp src/builder/builderwindow.h
    src/builder/packager.cpp src/builder/packager.h
    src/builder/gpgsign.cpp src/builder/gpgsign.h
    src/builder/project.cpp src/builder/project.h
    src/common/sidepanel.cpp src/common/sidepanel.h
    src/common/diagnostics.cpp src/common/diagnostics.h
    src/common/languages.cpp src/common/languages.h
    src/common/chrome.cpp src/common/chrome.h
    ${CMAKE_BINARY_DIR}/builder.qrc)
target_include_directories(kit-builder PRIVATE src/common)
target_compile_definitions(kit-builder PRIVATE KIT_VERSION="${PROJECT_VERSION}")
target_link_libraries(kit-builder PRIVATE kitcore Qt${QT_VERSION_MAJOR}::Widgets)

# 2) Kit Installer : le moteur embarqué dans l'AppImage généré
add_executable(kit-installer
    src/installer/main.cpp src/installer/installerwindow.cpp src/installer/installerwindow.h
    src/installer/uninstallerwindow.cpp src/installer/uninstallerwindow.h
    src/installer/gpgkeywindow.cpp src/installer/gpgkeywindow.h
    src/common/sidepanel.cpp src/common/sidepanel.h
    src/common/diagnostics.cpp src/common/diagnostics.h
    src/common/languages.cpp src/common/languages.h
    src/common/chrome.cpp src/common/chrome.h
    ${CMAKE_BINARY_DIR}/installer.qrc)
target_include_directories(kit-installer PRIVATE src/common)
target_compile_definitions(kit-installer PRIVATE KIT_VERSION="${PROJECT_VERSION}")
target_link_libraries(kit-installer PRIVATE kitcore Qt${QT_VERSION_MAJOR}::Widgets)

# Dossier « languages » à côté de kit-builder : on peut y déposer de nouveaux fichiers .lang sans recompiler
add_custom_command(TARGET kit-builder POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/languages $<TARGET_FILE_DIR:kit-builder>/languages)

install(TARGETS kit-builder kit-installer RUNTIME DESTINATION bin)
install(FILES packaging/kit-builder.desktop DESTINATION share/applications)
install(FILES packaging/kit-gen.xml DESTINATION share/mime/packages)      # puis : update-mime-database /usr/share/mime
install(FILES resources/logo.png DESTINATION share/icons/hicolor/256x256/mimetypes RENAME application-x-kit-gen.png)
install(FILES resources/logo.png DESTINATION share/icons/hicolor/256x256/apps RENAME kit-builder.png)
install(DIRECTORY languages/ DESTINATION share/kit-installer/languages)
