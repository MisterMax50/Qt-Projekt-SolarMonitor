#include "abo.h"

#include <QCoreApplication>
#include <QSettings>

Abo* Abo::instanz()
{
    static Abo* abo = new Abo(QCoreApplication::instance());
    return abo;
}

Abo::Stufe Abo::stufe()
{
    QSettings s;
    if (s.contains(QStringLiteral("abo"))) {
        const int w = s.value(QStringLiteral("abo")).toInt();
        return w == 2 ? Stufe::Ultimate : w == 1 ? Stufe::Premium : Stufe::Kostenlos;
    }
    // ältere Einstellung: nur "premium" an/aus
    return s.value(QStringLiteral("premium"), false).toBool() ? Stufe::Premium : Stufe::Kostenlos;
}

void Abo::setzeStufe(Stufe neu)
{
    const Stufe alt = stufe();
    QSettings s;
    s.setValue(QStringLiteral("abo"), int(neu));
    s.setValue(QStringLiteral("premium"), neu != Stufe::Kostenlos);
    if (alt != neu)
        emit instanz()->stufeGeaendert(neu);
}

bool Abo::pruefe(Funktion f)
{
    if (erlaubt(f))
        return true;
    emit instanz()->paywall(f);
    return false;
}

bool Abo::istPaketSprache(const QString& kuerzel)
{
    return kuerzel == QLatin1String("fr") || kuerzel == QLatin1String("zh");
}

QString Abo::name(Stufe s)
{
    switch (s) {
    case Stufe::Premium:
        return QStringLiteral("SolarMonitor Premium");
    case Stufe::Ultimate:
        return QStringLiteral("SolarMonitor Ultimate");
    case Stufe::Kostenlos:
        break;
    }
    return tr("Kostenlos");
}

QString Abo::preis(Stufe s)
{
    switch (s) {
    case Stufe::Premium:
        return tr("9,99 €");
    case Stufe::Ultimate:
        return tr("19,99 €");
    case Stufe::Kostenlos:
        break;
    }
    return tr("0,00 €");
}

QStringList Abo::vorteile(Stufe s)
{
    QStringList v{tr("Keine Werbebanner mehr"), tr("Keine Popups im Minutentakt")};
    if (s == Stufe::Ultimate)
        v << tr("Dunkles Design") << tr("Vordefinierte Dashboard-Anordnungen")
          << tr("Sprachpaket International (Français, 中文)") << tr("Intro beim Start abschaltbar")
          << tr("Violettes ULTIMATE-Abzeichen im Kopf");
    else
        v << tr("Goldenes PREMIUM-Abzeichen im Kopf");
    v << tr("Jederzeit im Kontomenü kündbar");
    return v;
}

QString Abo::funktionsName(Funktion f)
{
    switch (f) {
    case Funktion::DunklesDesign:
        return tr("Dunkles Design");
    case Funktion::Anordnungen:
        return tr("Vordefinierte Dashboard-Anordnungen");
    case Funktion::Sprachpaket:
        return tr("Sprachpaket International (Français, 中文)");
    case Funktion::IntroAus:
        return tr("Intro beim Start abschalten");
    }
    return {};
}
