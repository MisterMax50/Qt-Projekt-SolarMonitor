#include "csv_parser.h"

#include <cmath>

#include <QFile>
#include <QFileInfo>

QString CsvReader::Tabelle::wert(const Zeile& z, const QString& name) const
{
    const auto it = spalte.constFind(name);
    if (it == spalte.constEnd() || it.value() >= z.felder.size())
        return QString();
    return z.felder.at(it.value()).trimmed();
}

QStringList CsvReader::aufteilen(const QString& zeile)
{
    QStringList felder;
    QString aktuell;
    bool inAnfuehrung = false;

    for (int i = 0; i < zeile.size(); ++i) {
        const QChar c = zeile.at(i);
        if (inAnfuehrung) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < zeile.size() && zeile.at(i + 1) == QLatin1Char('"')) {
                    aktuell += QLatin1Char('"'); // "" = maskiertes Anführungszeichen
                    ++i;
                } else {
                    inAnfuehrung = false;
                }
            } else {
                aktuell += c;
            }
        } else if (c == QLatin1Char('"')) {
            inAnfuehrung = true;
        } else if (c == QLatin1Char(',')) {
            felder.append(aktuell);
            aktuell.clear();
        } else {
            aktuell += c;
        }
    }
    felder.append(aktuell);
    return felder;
}

QString CsvReader::maskiere(const QString& feld)
{
    if (!feld.contains(QLatin1Char(',')) && !feld.contains(QLatin1Char('"'))
        && !feld.contains(QLatin1Char('\n')))
        return feld;
    QString tmp = feld;
    tmp.replace(QLatin1String("\""), QLatin1String("\"\""));
    return QLatin1Char('"') + tmp + QLatin1Char('"');
}

std::optional<double> CsvReader::zahl(const QString& text, bool* kommaKorrigiert)
{
    if (kommaKorrigiert)
        *kommaKorrigiert = false;
    QString t = text.trimmed();
    if (t.isEmpty())
        return std::nullopt;

    bool ok = false;
    double w = t.toDouble(&ok); // QString::toDouble nutzt immer die C-Locale
    if (!ok && t.count(QLatin1Char(',')) == 1 && !t.contains(QLatin1Char('.'))) {
        t.replace(QLatin1Char(','), QLatin1Char('.'));
        w = t.toDouble(&ok);
        if (ok && kommaKorrigiert)
            *kommaKorrigiert = true;
    }
    if (!ok || !std::isfinite(w))
        return std::nullopt;
    return w;
}

CsvReader::Tabelle CsvReader::lade(const QString& pfad, const QStringList& pflichtSpalten)
{
    Tabelle t;
    t.dateiname = QFileInfo(pfad).fileName();

    QFile datei(pfad);
    if (!datei.exists())
        throw DateiFehler(tr("Datei nicht gefunden: %1").arg(QFileInfo(pfad).absoluteFilePath()));
    if (!datei.open(QIODevice::ReadOnly))
        throw DateiFehler(tr("Datei %1 konnte nicht geöffnet werden: %2")
                              .arg(t.dateiname, datei.errorString()));

    QByteArray roh = datei.readAll();
    if (roh.startsWith("\xEF\xBB\xBF"))
        roh.remove(0, 3); // UTF-8-BOM

    const QString text = QString::fromUtf8(roh);
    const QStringList zeilen = text.split(QLatin1Char('\n'));

    int kopfIndex = -1;
    for (int i = 0; i < zeilen.size(); ++i) {
        QString z = zeilen.at(i);
        if (z.endsWith(QLatin1Char('\r')))
            z.chop(1);
        if (z.trimmed().isEmpty())
            continue;
        if (kopfIndex < 0) {
            kopfIndex = i;
            for (const QString& k : aufteilen(z))
                t.kopf.append(k.trimmed());
            for (int s = 0; s < t.kopf.size(); ++s)
                t.spalte.insert(t.kopf.at(s), s);
            continue;
        }
        t.zeilen.append(Zeile{i + 1, aufteilen(z)});
    }

    if (kopfIndex < 0)
        throw DateiFehler(tr("Datei %1 ist leer.").arg(t.dateiname));

    QStringList fehlend;
    for (const QString& p : pflichtSpalten)
        if (!t.spalte.contains(p))
            fehlend.append(p);
    if (!fehlend.isEmpty())
        throw DateiFehler(tr("In %1 fehlen die Spalten: %2")
                              .arg(t.dateiname, fehlend.join(QStringLiteral(", "))));
    return t;
}
