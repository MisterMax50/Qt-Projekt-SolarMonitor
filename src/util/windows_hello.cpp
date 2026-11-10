#include "windows_hello.h"

#include <QWidget>

#ifdef Q_OS_WIN
#include <QEventLoop>

#include "windows_hello_win32.inc"

namespace {
void pumpe()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

bool g_geprueft = false;
hello_win32::Verfuegbarkeit g_verfuegbarkeit = hello_win32::Verfuegbarkeit::Fehler;
} // namespace

bool WindowsHello::verfuegbar(QString* grund)
{
    using V = hello_win32::Verfuegbarkeit;
    if (!g_geprueft) {
        g_verfuegbarkeit = hello_win32::verfuegbarkeit(pumpe);
        // nur ein eindeutiges Ergebnis merken; "belegt" und technische
        // Fehler beim nächsten Mal erneut prüfen
        g_geprueft = g_verfuegbarkeit != V::Belegt && g_verfuegbarkeit != V::Fehler;
    }
    if (grund) {
        switch (g_verfuegbarkeit) {
        case V::Verfuegbar: grund->clear(); break;
        case V::KeinGeraet: *grund = tr("Kein Fingerabdruckleser bzw. keine Windows-Hello-Kamera gefunden."); break;
        case V::NichtEingerichtet: *grund = tr("Windows Hello ist für dieses Windows-Konto nicht eingerichtet (Windows-Einstellungen → Konten → Anmeldeoptionen)."); break;
        case V::Richtlinie: *grund = tr("Windows Hello ist durch eine Richtlinie deaktiviert."); break;
        case V::Belegt: *grund = tr("Windows Hello ist gerade belegt. Bitte erneut versuchen."); break;
        default:
            *grund = tr("Windows Hello konnte nicht abgefragt werden: %1")
                         .arg(QString::fromStdWString(hello_win32::letzterFehler()));
            break;
        }
    }
    return g_verfuegbarkeit == V::Verfuegbar;
}

WindowsHello::Ergebnis WindowsHello::pruefe(QWidget* fenster, const QString& nachricht, QString* fehler)
{
    using P = hello_win32::Pruefung;
    QString grund;
    if (!verfuegbar(&grund)) {
        if (fehler)
            *fehler = grund;
        return Ergebnis::NichtVerfuegbar;
    }
    const HWND hwnd = fenster ? reinterpret_cast<HWND>(fenster->window()->winId()) : nullptr;
    const P p = hello_win32::pruefe(hwnd, nachricht.toStdWString(), pumpe);
    switch (p) {
    case P::Bestaetigt:
        return Ergebnis::Bestaetigt;
    case P::Abgebrochen:
        return Ergebnis::Abgebrochen;
    case P::ZuVieleVersuche:
        if (fehler)
            *fehler = tr("Zu viele Fehlversuche bei Windows Hello.");
        return Ergebnis::Fehler;
    case P::KeinGeraet:
    case P::NichtEingerichtet:
    case P::Richtlinie:
        g_geprueft = false; // Zustand hat sich geändert, beim nächsten Mal neu prüfen
        if (fehler)
            *fehler = tr("Windows Hello ist nicht verfügbar.");
        return Ergebnis::NichtVerfuegbar;
    default:
        if (fehler)
            *fehler = tr("Windows Hello konnte die Anmeldung nicht bestätigen: %1")
                          .arg(QString::fromStdWString(hello_win32::letzterFehler()));
        return Ergebnis::Fehler;
    }
}

QString WindowsHello::windowsKonto()
{
    return QString::fromStdWString(hello_win32::windowsKonto());
}

#else // andere Systeme: kein Windows Hello

bool WindowsHello::verfuegbar(QString* grund)
{
    if (grund)
        *grund = tr("Windows Hello gibt es nur unter Windows.");
    return false;
}

WindowsHello::Ergebnis WindowsHello::pruefe(QWidget*, const QString&, QString* fehler)
{
    if (fehler)
        *fehler = tr("Windows Hello gibt es nur unter Windows.");
    return Ergebnis::NichtVerfuegbar;
}

QString WindowsHello::windowsKonto()
{
    return QString();
}

#endif
