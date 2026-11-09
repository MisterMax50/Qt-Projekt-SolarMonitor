# SolarMonitor – Monitoring einer PV-Flotte

Offline-Desktopanwendung (C++17, Qt 6 Widgets) zur Verwaltung und Auswertung
von rund 1.500 PV-Anlagen: Stammdaten, Störungen, Messwertverläufe,
Energieertrag und Performance Ratio, automatische Fehlererkennung,
Serviceplanung mit Tourenplanung und Karte.

## Bauen und starten

Das Projekt wird mit **CMake** gebaut (`CMakeLists.txt`, CMake ≥ 3.19, Qt ≥ 6.4):

1. Qt Creator → *Datei → Projekt öffnen* → `CMakeLists.txt`
2. Kit: **Desktop Qt 6.11.x/6.12.x MinGW 64-bit**
3. *Erstellen* → *Ausführen*

Ohne Qt Creator:

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=<Qt-Pfad>/6.12.0/mingw_64
cmake --build build
```

Hinweis Windows/MinGW: Dateipfade dürfen höchstens 260 Zeichen lang sein. Das
CMake-Ziel heißt deshalb intern kurz `SolarMonitor` (die Exe trotzdem
`SolarMonitor_Gruppe_XY.exe`). Liegt das Projekt sehr tief im Dateisystem und
erscheint beim Bauen „opening dependency file … No such file or directory“,
das Projekt in einen kürzeren Pfad verschieben (z. B. `C:\Projekte\`).

Das Logo wird im Programm per Code gezeichnet (`Stil::zeichneLogo`). Im Ordner
`resources/` liegen das daraus erzeugte Programmsymbol der Exe
(`SolarMonitor.ico`, eingebunden über `SolarMonitor.rc`) und das Logo als PNG
in 512 px, z. B. für das Titelblatt der Dokumentation.

Benötigte Qt-Module: `Widgets`, `Concurrent`, `Network` und `LinguistTools`
(alle in der Standardinstallation). Die Übersetzungen werden beim Bauen per
`lrelease` erzeugt und unter `:/i18n/` in die Exe eingebettet
(`qt_add_translations`). Die `.ts`-Dateien lassen sich über das Build-Ziel
`update_translations` aktualisieren.

Die Daten werden direkt aus **`data/` im Projektordner** gelesen (neben
`CMakeLists.txt`); in den Build-Ordner muss nichts kopiert werden. Den Pfad
trägt CMake beim Bauen ein (`SOLARMONITOR_PROJEKTORDNER`). Auch
Kartenkacheln und gespeicherte Routen landen dort. Findet das Programm den
Ordner nicht (z. B. weil das Projekt nach dem Bauen verschoben wurde), sucht
es neben der Exe, bis zu vier Ordnerebenen darüber und im
Arbeitsverzeichnis.

**Datenordner ändern:** Im Benutzermenü unter *Datenordner ändern …* lässt
sich jederzeit ein anderer Ordner wählen (Pfad eintippen oder
*Durchsuchen …*). Die Seite prüft sofort, ob `anlagen.csv` und die übrigen
Dateien vorhanden sind. Nach *Übernehmen* lädt das Programm den neuen
Ordner (`src/model/datensatz.*`) im laufenden Fenster und tauscht erst bei
Erfolg Daten, Auswertung und Oberfläche aus – das Fenster bleibt offen, die
Anmeldung bleibt bestehen; die Wahl wird
gespeichert (QSettings) und hat beim nächsten Start Vorrang vor der Suche.
Werden beim Start keine Daten gefunden, erscheint dieselbe Seite direkt nach
der Anmeldung (`src/ui/datenordner_seite.*`).

Kommandozeile: `--daten <ordner>` (Datenordner), `--lang de|en|fr|zh`
(Startsprache), `--kein-intro`.

## Anmeldung (Rollen)

| Rolle      | PIN  | Rechte |
|------------|------|--------|
| betrieb    | 0000 | Ansehen, Auswertungen, Ertragsbericht/Einsatzliste exportieren |
| service    | 5678 | zusätzlich Störungen erfassen und bearbeiten, Störungen exportieren |
| admin      | 1234 | zusätzlich Stammdaten bearbeiten und exportieren |

Nach drei falschen PINs ist die Anmeldung 30 s gesperrt. PINs sind nur als
SHA-256-Hash im Code hinterlegt. *Datei → Benutzer wechseln* meldet neu an.

## Oberfläche

| Seite (Navigation links) | Inhalt |
|---|---|
| Übersicht (Startseite) | Gruppenfeld „Flottenstatus“ mit Kennzahlen als Links zur passenden Seite (Anlagen in Betrieb, offene/kritische Störungen, Median-PR, Ertragsverlust, Datenprüfung), Karte der Flotte gefärbt nach Handlungsbedarf (rot: kritische Störung, gestört oder Fehlerbild; orange: offene Störung oder Wartung), Listen mit offenen kritischen Störungen, größten Ertragsverlusten und Fehlerbildern (Doppelklick öffnet die Anlage). Aktualisiert sich bei jeder Änderung (`src/ui/dashboard_widget.*`) |
| Anlagen | Tabelle (QAbstractTableModel + QSortFilterProxyModel) mit Detailbereich rechts, Suche nach ID/Ort/Wetterstation (Suchfeld in der Kopfzeile), kombinierbare Filter (Status, Typ, Ort, Leistungsbereich, nur mit Fehlerbild, nur mit offenen Störungen), Sortierung u. a. nach Leistung, Inbetriebnahme und PR, Kontextmenü |
| Störungen | Störungsliste mit Filtern, Kontextmenü (Status ändern, Anlage öffnen) |
| Karte | OpenStreetMap-Hintergrund, Anlagen und Wetterstationen, Färbung nach Status/PR/Auffälligkeit/Handlungsbedarf, Zoom/Pan, Klick öffnet die Anlage, geplante Tour |
| Statistik | Leistung je Typ und Ort, Altersverteilung, Störungen je Fehlercode und Schwere, PR je Wetterstation |
| Serviceplanung | Priorisierte Einsatzliste nach geschätztem Verlust in €, Tourenplanung im Umkreis eines Startpunkts mit Autoroute (OSRM) oder Luftlinie (Nearest Neighbour + 2-opt) |
| Datenprüfung | Alle Befunde beim Einlesen mit Datei und Zeilennummer |

Doppelklick auf eine Anlage öffnet den **Detaildialog** mit den Reitern
Stammdaten (inkl. Wechselrichter und Wetterstation), Verlauf (Leistung,
Einstrahlung, Modultemperatur), Ertrag & PR (Kennzahlen, Tageserträge als
Diagramm und Tabelle, erkannte Fehlerbilder) und Störungen.

**Rückgängig/Wiederholen:** Alle Datenänderungen (Stammdaten bearbeiten,
Störung erfassen, Störung bearbeiten, Status per Kontextmenü) laufen als
Befehle über einen `QUndoStack` (`src/ui/aenderungen.*`). *Start →
Rückgängig* (Strg+Z) bzw. *Wiederholen* (Strg+Y) nennt die betroffene
Änderung; *Start → Änderungsverlauf* zeigt alle Änderungen der Sitzung
(`QUndoView`) und springt per Klick zu jedem Stand. Nach jedem Schritt werden
die `*_out.csv` neu geschrieben und die Kennzahlen der Anlage neu berechnet.
Beim Benutzerwechsel wird der Verlauf geleert, damit eine Rolle nicht die
Änderungen einer anderen zurücknimmt.

**PDF-Berichte** (`src/ui/pdf_bericht.*`, alle Rollen):
- *Anlagenbericht* (Detaildialog, Kontextmenü oder *Exportieren* auf der
  Seite *Anlagen*): Stammdaten, Kennzahlen, Tageserträge und Leistungsverlauf als
  Diagramm, erkannte Fehlerbilder, Störungen, Hinweise der Datenprüfung.
- *Flottenbericht* (*Exportieren* auf den Seiten *Übersicht* und *Statistik*): Übersicht, Status,
  Vergleich der Wetterstationen, 15 größte Ertragsverluste, Häufigkeit der
  Fehlerbilder, offene kritische Störungen, Datenqualität.

Umsetzung mit `QPdfWriter` und `QTextDocument` (HTML): Das Dokument bricht
selbst auf A4-Seiten um, Tabellenköpfe werden auf Folgeseiten wiederholt,
Überschriften rutschen nicht allein ans Seitenende. Kopf- und Fußzeile
(Logo, Titel, Datum, Rolle, Seite x von y) werden je Seite mit QPainter
gezeichnet. Die Diagramme sind dieselben Widgets wie in der Oberfläche, in
ein Bild gerendert, immer hell (auch im dunklen Design).

Beim Start läuft im Programmfenster ein kurzes **Logo-Intro** (`src/ui/intro_animation.*`, ca. 3 s):
Die Kachel springt auf, die Sonne geht auf, Strahlen und Messkurve
zeichnen sich, dann blendet der Schriftzug ein. Klick oder Taste
überspringt es; abschalten im Benutzermenü (*Intro beim Start*) oder mit
`--kein-intro`. Gezeichnet mit QPainter, ohne Bild- oder Videodateien.

Das Programm startet im **Vollbild**; im Benutzermenü oben rechts bzw. mit F11
wird auf ein maximiertes Fenster umgeschaltet. Die Wahl wird gespeichert.

Sprache (Deutsch, Englisch, Französisch, Chinesisch – zur Laufzeit) und Design (hell/dunkel) stehen
ebenfalls im Benutzermenü. Sprache, Design, Vollbild und Fenstergröße werden
gespeichert (QSettings).

**Gestaltung nach dem Vorbild von Monitoring-Portalen für Solaranlagen**
(z. B. Hersteller-Portale für Wechselrichter oder Betriebsführungs-Software
für Solarparks):
- **Seitenleiste** links in Dunkelblau mit Logo, Abschnitten *Überwachung*
  und *Auswertung* und farbigen Abzeichen (offene Störungen, Anlagen mit
  Fehlerbild, Fehler beim Einlesen). Unten der Zustand der Flottenanalyse.
- **Kopfzeile** mit Seitentitel und Erklärung, Anlagensuche (Strg+F),
  **Alarmglocke** mit der Zahl offener kritischer Störungen und
  **Benutzermenü** (Benutzer wechseln, Änderungsverlauf, Dunkles Design,
  Vollbild, Sprache, Über, Beenden).
- **Aktionszeile** je Seite: links der Zähler („1499 von 1499 Anlagen“),
  rechts Rückgängig/Wiederholen, die Befehle der Seite (Hauptbefehl
  „Störung erfassen“ in Orange) und ein Menüknopf *Exportieren*.
- **Inhalte als Karten**: weiße Flächen mit runden Ecken auf hellgrauem
  Grund; Filter in einer eigenen Karte über der Tabelle; Detailkarte rechts
  neben der Anlagentabelle (`src/ui/anlage_info.*`).
- **Kennzahlenkacheln** (`src/ui/kennzahl_kachel.*`) auf Übersicht,
  Statistik und im Detaildialog: farbiger Symbolkreis, großer Wert, Notiz.
  Auf der Übersicht führt ein Klick zur passenden Seite.
- Rückmeldungen (gespeichert, exportiert, Analyse fertig) erscheinen als
  kurze **Einblendung** unten rechts statt in einer Statusleiste.
- **Ein einziges Fenster**: Intro, Anmeldung und Programm laufen im selben
  Fenster (QStackedWidget im Hauptfenster). Die Anmeldung
  (`src/ui/login_seite.*`) ist eine Karte auf dunkelblauem Grund mit
  Markenfläche links und Formular rechts; *Benutzer wechseln* zeigt sie
  ebenfalls im Fenster, mit „Zurück“ geht es ohne Wechsel weiter.
- Farben: Dunkelblau (Seitenleiste) und Sonnen-Orange (Akzent, Hauptknöpfe);
  Grün/Orange/Rot nur für den Zustand der Anlagen. Alles steht in einem
  Stylesheet in `src/util/stil.cpp` (hell und dunkel), Grundlage ist der
  Qt-Stil „Fusion“. Die Linien-Icons werden im Code gezeichnet, ohne
  zusätzliche Bilddateien.

## Datenbasis und Indexdateien

```
data/
├── anlagen.csv, wechselrichter.csv, wetterstationen.csv, stoerungen.csv
├── master_verlauf_index.csv          # anlage_id → pfad (relativ zu data/)
└── anlagen_verlauf_batch_XX/
    ├── verlauf_index.csv             # anlage_id → pfad (relativ zum Batch-Ordner)
    └── PXXXXX.json
```

- Beim Start wird nur der **Index** gelesen und geprüft, ob die Dateien
  existieren. Fehlt `master_verlauf_index.csv`, werden die Batch-Indizes
  verwendet; Einträge, die nur in einem Batch-Index stehen, werden ergänzt.
- Eine JSON-Datei wird erst geladen, wenn ihr Verlauf gebraucht wird
  (**Lazy Loading**), im Hintergrund per `QtConcurrent`. Die letzten 20
  Verläufe bleiben in einem LRU-Cache.
- Parallel wertet die **Flottenanalyse** alle Verläufe im Hintergrund aus
  (PR, spezifischer Ertrag, Fehlerbilder, Median-PR je Wetterstation).

Geänderte Daten werden automatisch nach `data/anlagen_out.csv` bzw.
`data/stoerungen_out.csv` geschrieben (`QSaveFile`, die Originale bleiben
unverändert). Weitere Exporte: *Datei → … exportieren*.

## Hintergrundkarte (OpenStreetMap)

Die Karte nutzt Web-Mercator wie OpenStreetMap. Kacheln werden beim ersten
Anzeigen von `tile.openstreetmap.org` geladen (höchstens zwei gleichzeitig,
eigener User-Agent, gemäß OSM-Nutzungsrichtlinie) und dauerhaft in
`data/kartenkacheln/{z}/{x}/{y}.png` gespeichert. Danach funktioniert die
Karte für alle bereits angesehenen Bereiche und Zoomstufen offline.
Ohne Internet und ohne gespeicherte Kacheln erscheint ein Hinweis, die
Anlagen werden weiter angezeigt; ohne Hintergrund wird ein Gradnetz
gezeichnet. Kartendaten: © OpenStreetMap-Mitwirkende (ODbL).

## Tourenplanung mit Autoroute

In der Serviceplanung werden aus allen Einsätzen im gewählten Umkreis um
den Startpunkt die wichtigsten ausgewählt (Priorität, dann Verlust in €).

**Umkreis um den Bodensee herum:** Der Umkreis wird nicht quer über das
Wasser gemessen, sondern als kürzester Landweg um die Seen herum
(`src/util/gewaesser.*`). Die Uferlinie des Bodensees (Obersee/Überlinger
See und Untersee) ist als Polygon einkompiliert (Natural Earth 1:10m,
gemeinfrei) und damit immer offline verfügbar. Ist die Luftlinie frei, gilt
sie; sonst wird über einen Sichtbarkeitsgraphen der Ufer-Eckpunkte mit
Dijkstra der kürzeste Weg um das Wasser gesucht. Beispiel: Meersburg →
Konstanz sind 8 km Luftlinie, um den See herum aber rund 44 km – Konstanz
liegt daher bei 25 km Umkreis nicht mehr in der Tour. Die Karte zeigt statt
eines Kreises die so erreichbare Fläche (Marching Squares auf einem Raster,
Seen ausgespart). Der Uferumriss ist bewusst grob (≈ 1 km); Anlagen direkt
am Ufer, die rechnerisch im See liegen, werden auf das nächste Ufer gesetzt.

Die Reihenfolge wird zuerst offline per Nearest Neighbour + 2-opt geplant,
ebenfalls mit den Entfernungen um den See herum (gestrichelte Linie auf der
Karte). Ist „Autoroute“ aktiv, wird zusätzlich der OSRM-Dienst
(`trip`, OpenStreetMap-Straßennetz) abgefragt: Er liefert die schnellste
Rundreise, Etappenlängen, Fahrzeiten und den Straßenverlauf, der auf der
Karte eingezeichnet wird. Autofähren (z. B. Meersburg–Konstanz) werden mit
`exclude=ferry` ausgeschlossen; unterstützt ein Server diese Option nicht,
wird ohne Ausschluss erneut angefragt. Antworten werden in `data/routen_cache/`
gespeichert und stehen danach offline zur Verfügung. Ohne Internet bleibt
die Luftlinien-Tour aktiv. Der Server lässt sich über die
Umgebungsvariable `SOLARMONITOR_OSRM_URL` ändern (Standard:
`https://router.project-osrm.org`, öffentlicher Demoserver – nur für geringe
Nutzung gedacht).

## Fachliche Berechnung

- E [kWh] = 0,25 h / 1000 · Σ P [W], H [kWh/m²] analog aus G [W/m²]
- Y = E / Ppeak, PR = E′ / (H′ · Ppeak)
- **Datenlücken:** E und H summieren alle vorhandenen Werte; für die PR
  werden nur Intervalle verwendet, in denen Leistung *und* Einstrahlung
  vorliegen, damit eine Lücke die PR nicht verfälscht.
- Erkannte Fehlerbilder: keine Einspeisung, Ausfall (G > 300 W/m², P ≈ 0),
  WR meldet Störung, Leistungsbegrenzung des Wechselrichters (P ≥ 97 % der
  AC-Grenze über ≥ 45 min), Verschattung morgens/abends (Wirkungsgrad
  P/(G·Ppeak) < 65 % des Mittagswerts), Minderleistung (PR < 85 % des
  Stationsmedians), Datenlücken > 5 %. Alle Schwellwerte stehen in
  `src/eval/schwellwerte.h`.
- Ertragsverlust = H · Ppeak · Median-PR der Station − E (≥ 0), in € mit der
  Einspeisevergütung bewertet.

## Projektstruktur

```
src/
├── main.cpp          Start: Datenordner suchen, Laden, Hauptfenster
├── model/            Datenklassen, Datenbasis (CSV laden/prüfen/exportieren), Rollen
├── repo/             CSV-Leser, Verlaufsindex + JSON-Parser + LRU-Cache
├── eval/             Ertragsauswertung, Flottenanalyse, Tourenplanung, Schwellwerte
├── util/             Anzeige-Formatierung, Zeitstempel, Geo, Gewässer (Bodensee), Sprachumschaltung
└── ui/               Hauptfenster, Modelle, Dialoge, Diagramme, Karte, Statistik, Service
i18n/                 SolarMonitor_en/_fr/_zh.ts (vollständig), SolarMonitor_de.ts (Pluralformen)
```

## Bekannte Einschränkungen

- Entfernungen der Tourenplanung sind Luftlinien (offline keine Straßendaten).
- Die Hintergrundkarte ist offline nur für bereits angesehene Bereiche vorhanden;
  ein Massen-Vorabdownload ist nach der OSM-Richtlinie nicht erlaubt.
- Die Fehlerbilderkennung arbeitet mit festen Schwellwerten und kann bei
  ungewöhnlichen Anlagen (z. B. Ost-West-Dächer) Hinweise erzeugen.
- Änderungen werden in `*_out.csv` gespeichert; beim nächsten Start werden
  wieder die Originaldateien gelesen.

## Abgabe-Checkliste

- [ ] Ordner `build/` und `.qtcreator/` nicht mit ins Zip
- [ ] `data/` mit allen globalen CSVs und mindestens einem vollständigen
      Batch-Ordner (≥ 300 JSON) inkl. `verlauf_index.csv`, dazu `master_verlauf_index.csv`
- [ ] Karte vorher einmal online öffnen und die gewünschten Bereiche/Zoomstufen
      ansehen, dann `data/kartenkacheln` mit abgeben (Karte offline verfügbar)
- [ ] Gewünschte Touren vorher einmal online planen und `data/routen_cache`
      mit abgeben (Autorouten offline verfügbar)
- [ ] Dokumentation als PDF (Times New Roman 11 pt, 2.800–3.000 Wörter, nur Fließtext)
- [ ] Gruppennummer in Projekt- und Ordnernamen eintragen (`SolarMonitor_Gruppe_XY`)
