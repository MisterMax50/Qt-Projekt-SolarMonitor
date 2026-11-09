# SolarMonitor – Monitoring einer PV-Flotte
# Qt 6 (getestet mit Desktop Qt 6.12 MinGW 64-bit), C++17, CMake

cmake_minimum_required(VERSION 3.19)

project(SolarMonitor_Gruppe_XY
    VERSION 2.0
    DESCRIPTION "Monitoring einer PV-Flotte rund um den Bodensee"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Concurrent Network LinguistTools)

# Setzt AUTOMOC/AUTOUIC/AUTORCC und plattformübliche Installationspfade
qt_standard_project_setup()

set(PROJEKT_HEADER
    src/model/anlage.h
    src/model/datenbasis.h
    src/model/pruefmeldung.h
    src/model/rolle.h
    src/model/stoerung.h
    src/model/verlauf.h
    src/model/wechselrichter.h
    src/model/wetterstation.h
    src/repo/csv_parser.h
    src/repo/routen_dienst.h
    src/repo/verlauf_repository.h
    src/eval/ertrags_auswertung.h
    src/eval/flotten_analyse.h
    src/eval/schwellwerte.h
    src/eval/tourenplanung.h
    src/util/anzeige.h
    src/util/geo.h
    src/util/gewaesser.h
    src/util/gewaesser_daten.h
    src/util/sprache.h
    src/util/stil.h
    src/util/zeit.h
    src/ui/aenderungen.h
    src/ui/anlage_info.h
    src/ui/anlage_model.h
    src/ui/app_kontext.h
    src/ui/badge_delegate.h
    src/ui/balken_diagramm.h
    src/ui/dashboard_widget.h
    src/ui/detail_dialog.h
    src/ui/edit_dialog.h
    src/ui/filter_proxy.h
    src/ui/kachel_lader.h
    src/ui/karte_view.h
    src/ui/login_seite.h
    src/ui/mainwindow.h
    src/ui/kennzahl_kachel.h
    src/ui/intro_animation.h
    src/ui/datenordner_seite.h
    src/ui/pdf_bericht.h
    src/ui/pruefprotokoll_widget.h
    src/ui/serviceplanung.h
    src/ui/statistik_widget.h
    src/ui/stoerung_dialog.h
    src/ui/stoerung_model.h
    src/ui/tabellen_hilfe.h
    src/ui/verlauf_plot.h
)

set(PROJEKT_QUELLEN
    src/main.cpp
    src/model/datenbasis.cpp
    src/model/rolle.cpp
    src/repo/csv_parser.cpp
    src/repo/routen_dienst.cpp
    src/repo/verlauf_repository.cpp
    src/eval/ertrags_auswertung.cpp
    src/eval/flotten_analyse.cpp
    src/eval/tourenplanung.cpp
    src/util/anzeige.cpp
    src/util/gewaesser.cpp
    src/util/sprache.cpp
    src/util/stil.cpp
    src/ui/aenderungen.cpp
    src/ui/anlage_info.cpp
    src/ui/anlage_model.cpp
    src/ui/badge_delegate.cpp
    src/ui/balken_diagramm.cpp
    src/ui/dashboard_widget.cpp
    src/ui/detail_dialog.cpp
    src/ui/edit_dialog.cpp
    src/ui/kachel_lader.cpp
    src/ui/karte_view.cpp
    src/ui/login_seite.cpp
    src/ui/mainwindow.cpp
    src/ui/kennzahl_kachel.cpp
    src/ui/intro_animation.cpp
    src/ui/datenordner_seite.cpp
    src/ui/pdf_bericht.cpp
    src/ui/pruefprotokoll_widget.cpp
    src/ui/serviceplanung.cpp
    src/ui/statistik_widget.cpp
    src/ui/stoerung_dialog.cpp
    src/ui/stoerung_model.cpp
    src/ui/verlauf_plot.cpp
)

# Kurzer interner Zielname: Unter Windows ist der Pfad einer Datei auf 260 Zeichen
# begrenzt (MAX_PATH), und CMake/MinGW hängen den Zielnamen mehrfach an den
# Build-Pfad an. Die erzeugte Exe heißt trotzdem SolarMonitor_Gruppe_XY.exe.
set(ZIEL SolarMonitor)

qt_add_executable(${ZIEL}
    ${PROJEKT_QUELLEN}
    ${PROJEKT_HEADER}
)

target_include_directories(${ZIEL} PRIVATE src)

# Programmsymbol der .exe (Explorer, Startmenü). Im laufenden Programm wird
# dasselbe Logo per Code gezeichnet (Stil::appIcon).
if(WIN32)
    target_sources(${ZIEL} PRIVATE resources/SolarMonitor.rc)
endif()

target_link_libraries(${ZIEL} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
)

target_compile_definitions(${ZIEL} PRIVATE
    # Warnungen bei veralteten Qt-APIs anzeigen
    QT_DEPRECATED_WARNINGS
    # Projektordner: Die Daten werden direkt aus <Projekt>/data gelesen,
    # nicht aus dem Build-Ordner (siehe findeDatenordner in main.cpp)
    SOLARMONITOR_PROJEKTORDNER="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${ZIEL} PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(${ZIEL} PRIVATE /W4 /utf-8)
endif()

# Kein Konsolenfenster unter Windows, App-Bundle unter macOS
set_target_properties(${ZIEL} PROPERTIES
    OUTPUT_NAME SolarMonitor_Gruppe_XY
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

# ---------- Übersetzungen ----------
# Quellsprache im Code ist Deutsch. lrelease erzeugt beim Bauen die .qm-
# Dateien und bettet sie als Ressource unter :/i18n/ ein, es müssen also
# keine Dateien neben die Exe kopiert werden.
# Die .ts-Dateien aktualisieren: Ziel "update_translations" bauen.
qt_add_translations(${ZIEL}
    TS_FILES
        i18n/SolarMonitor_de.ts
        i18n/SolarMonitor_en.ts
        i18n/SolarMonitor_fr.ts
        i18n/SolarMonitor_zh.ts
    RESOURCE_PREFIX /i18n
)

install(TARGETS ${ZIEL}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
