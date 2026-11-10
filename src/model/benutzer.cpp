#include "benutzer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString standardDatei()
{
    const QString ordner = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(ordner);
    return QDir(ordner).filePath(QStringLiteral("benutzer.json"));
}
} // namespace

Benutzerverwaltung::Benutzerverwaltung(QObject* parent)
    : Benutzerverwaltung(standardDatei(), parent)
{
}

Benutzerverwaltung::Benutzerverwaltung(const QString& datei, QObject* parent)
    : QObject(parent)
    , m_datei(datei)
{
    laden();
}

QByteArray Benutzerverwaltung::neuesSalz()
{
    QByteArray salz(16, Qt::Uninitialized);
    for (char& c : salz)
        c = char(QRandomGenerator::system()->bounded(256));
    return salz.toHex();
}

QByteArray Benutzerverwaltung::hash(const QByteArray& salz, const QString& pin)
{
    return QCryptographicHash::hash(salz + pin.toUtf8(), QCryptographicHash::Sha256).toHex();
}

bool Benutzerverwaltung::pinGueltig(const QString& pin)
{
    static const QRegularExpression muster(QStringLiteral("^[0-9]{4,8}$"));
    return muster.match(pin).hasMatch();
}

bool Benutzerverwaltung::kennungGueltig(const QString& kennung)
{
    static const QRegularExpression muster(QStringLiteral("^[a-z0-9._-]{2,32}$"));
    return muster.match(kennung).hasMatch();
}

const Benutzer* Benutzerverwaltung::finde(const QString& kennung) const
{
    for (const Benutzer& b : m_benutzer)
        if (b.kennung == kennung)
            return &b;
    return nullptr;
}

Benutzer* Benutzerverwaltung::findeVeraenderbar(const QString& kennung)
{
    for (Benutzer& b : m_benutzer)
        if (b.kennung == kennung)
            return &b;
    return nullptr;
}

int Benutzerverwaltung::aktiveAdmins(const QString& ohne) const
{
    int n = 0;
    for (const Benutzer& b : m_benutzer)
        if (b.aktiv && b.rolle == Rolle::Admin && b.kennung != ohne)
            ++n;
    return n;
}

void Benutzerverwaltung::demoKontenAnlegen()
{
    // Gleiche Zugänge wie bisher, damit die Demo unverändert funktioniert
    const QDateTime jetzt = QDateTime::currentDateTime();
    const struct {
        const char* kennung;
        const char* name;
        Rolle       rolle;
        const char* pin;
    } demo[] = {{"betrieb", QT_TRANSLATE_NOOP("Benutzerverwaltung", "Betrieb (Demo)"), Rolle::Betrieb, "0000"},
                {"service", QT_TRANSLATE_NOOP("Benutzerverwaltung", "Service (Demo)"), Rolle::Service, "5678"},
                {"admin", QT_TRANSLATE_NOOP("Benutzerverwaltung", "Administrator (Demo)"), Rolle::Admin, "1234"}};
    for (const auto& d : demo) {
        Benutzer b;
        b.kennung = QString::fromLatin1(d.kennung);
        b.name = tr(d.name);
        b.rolle = d.rolle;
        b.salz = neuesSalz();
        b.pinHash = hash(b.salz, QString::fromLatin1(d.pin));
        b.erstellt = jetzt;
        m_benutzer.append(b);
    }
}

void Benutzerverwaltung::laden()
{
    m_benutzer.clear();
    QFile f(m_datei);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray liste = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("benutzer")).toArray();
        for (const QJsonValue& v : liste) {
            const QJsonObject o = v.toObject();
            Benutzer b;
            b.kennung = o.value(QStringLiteral("kennung")).toString();
            b.name = o.value(QStringLiteral("name")).toString();
            if (!Rollen::ausKennung(o.value(QStringLiteral("rolle")).toString(), &b.rolle) || b.kennung.isEmpty())
                continue;
            b.aktiv = o.value(QStringLiteral("aktiv")).toBool(true);
            b.salz = o.value(QStringLiteral("salz")).toString().toLatin1();
            b.pinHash = o.value(QStringLiteral("pin_hash")).toString().toLatin1();
            b.erstellt = QDateTime::fromString(o.value(QStringLiteral("erstellt")).toString(), Qt::ISODate);
            b.letzteAnmeldung = QDateTime::fromString(o.value(QStringLiteral("letzte_anmeldung")).toString(), Qt::ISODate);
            b.helloKonto = o.value(QStringLiteral("windows_hello")).toString();
            if (!finde(b.kennung))
                m_benutzer.append(b);
        }
    }
    // Ohne gültige Datei oder ohne aktiven Admin: Demo-Konten (sonst käme
    // niemand mehr in die Benutzerverwaltung)
    if (m_benutzer.isEmpty() || aktiveAdmins() == 0) {
        if (m_benutzer.isEmpty())
            demoKontenAnlegen();
        else if (!finde(QStringLiteral("admin"))) {
            const QList<Benutzer> alt = m_benutzer;
            m_benutzer.clear();
            demoKontenAnlegen();
            const Benutzer admin = *finde(QStringLiteral("admin"));
            m_benutzer = alt;
            m_benutzer.append(admin);
        } else {
            findeVeraenderbar(QStringLiteral("admin"))->aktiv = true;
            findeVeraenderbar(QStringLiteral("admin"))->rolle = Rolle::Admin;
        }
        speichern();
    }
}

bool Benutzerverwaltung::speichern(QString* fehler)
{
    QJsonArray liste;
    for (const Benutzer& b : std::as_const(m_benutzer)) {
        QJsonObject o;
        o.insert(QStringLiteral("kennung"), b.kennung);
        o.insert(QStringLiteral("name"), b.name);
        o.insert(QStringLiteral("rolle"), Rollen::kennung(b.rolle));
        o.insert(QStringLiteral("aktiv"), b.aktiv);
        o.insert(QStringLiteral("salz"), QString::fromLatin1(b.salz));
        o.insert(QStringLiteral("pin_hash"), QString::fromLatin1(b.pinHash));
        o.insert(QStringLiteral("erstellt"), b.erstellt.toString(Qt::ISODate));
        if (b.letzteAnmeldung.isValid())
            o.insert(QStringLiteral("letzte_anmeldung"), b.letzteAnmeldung.toString(Qt::ISODate));
        if (!b.helloKonto.isEmpty())
            o.insert(QStringLiteral("windows_hello"), b.helloKonto);
        liste.append(o);
    }
    QJsonObject wurzel;
    wurzel.insert(QStringLiteral("benutzer"), liste);
    QDir().mkpath(QFileInfo(m_datei).absolutePath());
    QSaveFile datei(m_datei);
    if (!datei.open(QIODevice::WriteOnly) || datei.write(QJsonDocument(wurzel).toJson()) < 0 || !datei.commit()) {
        if (fehler)
            *fehler = tr("Die Benutzerdatei konnte nicht gespeichert werden: %1").arg(datei.errorString());
        return false;
    }
    return true;
}

bool Benutzerverwaltung::anmelden(const QString& kennung, const QString& pin)
{
    Benutzer* b = findeVeraenderbar(kennung);
    if (!b || !b->aktiv || hash(b->salz, pin) != b->pinHash)
        return false;
    b->letzteAnmeldung = QDateTime::currentDateTime();
    speichern();
    emit geaendert();
    return true;
}

bool Benutzerverwaltung::anmeldenMitHello(const QString& kennung, const QString& windowsKonto)
{
    Benutzer* b = findeVeraenderbar(kennung);
    if (!b || !b->aktiv || windowsKonto.isEmpty() || b->helloKonto.compare(windowsKonto, Qt::CaseInsensitive) != 0)
        return false;
    b->letzteAnmeldung = QDateTime::currentDateTime();
    speichern();
    emit geaendert();
    return true;
}

QString Benutzerverwaltung::setzeHello(const QString& kennung, const QString& windowsKonto)
{
    Benutzer* b = findeVeraenderbar(kennung);
    if (!b)
        return tr("Den Benutzer „%1“ gibt es nicht.").arg(kennung);
    const QString alt = b->helloKonto;
    b->helloKonto = windowsKonto;
    QString fehler;
    if (!speichern(&fehler)) {
        b->helloKonto = alt;
        return fehler;
    }
    emit geaendert();
    return QString();
}

QString Benutzerverwaltung::anlegen(const QString& kennung, const QString& name, Rolle rolle, const QString& pin)
{
    const QString k = kennung.trimmed().toLower();
    if (!kennungGueltig(k))
        return tr("Der Benutzername muss 2–32 Zeichen lang sein und darf nur Kleinbuchstaben, Ziffern, Punkt, "
                  "Binde- und Unterstrich enthalten.");
    if (finde(k))
        return tr("Den Benutzernamen „%1“ gibt es bereits.").arg(k);
    if (name.trimmed().isEmpty())
        return tr("Bitte einen Namen angeben.");
    if (!pinGueltig(pin))
        return tr("Die PIN muss aus 4 bis 8 Ziffern bestehen.");
    Benutzer b;
    b.kennung = k;
    b.name = name.trimmed();
    b.rolle = rolle;
    b.salz = neuesSalz();
    b.pinHash = hash(b.salz, pin);
    b.erstellt = QDateTime::currentDateTime();
    m_benutzer.append(b);
    QString fehler;
    if (!speichern(&fehler)) {
        m_benutzer.removeLast();
        return fehler;
    }
    emit geaendert();
    return QString();
}

QString Benutzerverwaltung::aendern(const QString& kennung, const QString& name, Rolle rolle, bool aktiv,
                                    const QString& angemeldet)
{
    Benutzer* b = findeVeraenderbar(kennung);
    if (!b)
        return tr("Den Benutzer „%1“ gibt es nicht.").arg(kennung);
    if (name.trimmed().isEmpty())
        return tr("Bitte einen Namen angeben.");
    if (kennung == angemeldet && (rolle != b->rolle || !aktiv))
        return tr("Die eigene Rolle kann nicht geändert und das eigene Konto nicht gesperrt werden.");
    if (b->rolle == Rolle::Admin && b->aktiv && (rolle != Rolle::Admin || !aktiv) && aktiveAdmins(kennung) == 0)
        return tr("Es muss mindestens ein aktiver Administrator bleiben.");
    const Benutzer alt = *b;
    b->name = name.trimmed();
    b->rolle = rolle;
    b->aktiv = aktiv;
    QString fehler;
    if (!speichern(&fehler)) {
        *b = alt;
        return fehler;
    }
    emit geaendert();
    return QString();
}

QString Benutzerverwaltung::setzePin(const QString& kennung, const QString& pin)
{
    Benutzer* b = findeVeraenderbar(kennung);
    if (!b)
        return tr("Den Benutzer „%1“ gibt es nicht.").arg(kennung);
    if (!pinGueltig(pin))
        return tr("Die PIN muss aus 4 bis 8 Ziffern bestehen.");
    const Benutzer alt = *b;
    b->salz = neuesSalz();
    b->pinHash = hash(b->salz, pin);
    QString fehler;
    if (!speichern(&fehler)) {
        *b = alt;
        return fehler;
    }
    emit geaendert();
    return QString();
}

QString Benutzerverwaltung::loeschen(const QString& kennung, const QString& angemeldet)
{
    if (kennung == angemeldet)
        return tr("Das eigene Konto kann nicht gelöscht werden.");
    const Benutzer* b = finde(kennung);
    if (!b)
        return tr("Den Benutzer „%1“ gibt es nicht.").arg(kennung);
    if (b->rolle == Rolle::Admin && b->aktiv && aktiveAdmins(kennung) == 0)
        return tr("Es muss mindestens ein aktiver Administrator bleiben.");
    const QList<Benutzer> alt = m_benutzer;
    m_benutzer.removeIf([&](const Benutzer& x) { return x.kennung == kennung; });
    QString fehler;
    if (!speichern(&fehler)) {
        m_benutzer = alt;
        return fehler;
    }
    emit geaendert();
    return QString();
}
