#include "pdf_bericht.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPdfWriter>
#include <QSet>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

#include "eval/ertrags_auswertung.h"
#include "eval/flotten_analyse.h"
#include "eval/schwellwerte.h"
#include "model/datenbasis.h"
#include "repo/verlauf_repository.h"
#include "ui/balken_diagramm.h"
#include "ui/verlauf_plot.h"
#include "util/anzeige.h"
#include "util/stil.h"

namespace {

// ---------------------------------------------------------------
//  Farben und Maße
// ---------------------------------------------------------------
const QString kText = QStringLiteral("#0f172a");
const QString kGedimmt = QStringLiteral("#64748b");
const QString kLinie = QStringLiteral("#e2e8f0");
const QString kKachel = QStringLiteral("#f8fafc");
const QString kAkzent = QStringLiteral("#f59e0b");

constexpr int kDpi = 300;
constexpr double kMm = kDpi / 25.4;
/// Breite des Inhalts in "HTML-Pixeln" (96 dpi), A4 minus 2 × 15 mm Rand
constexpr int kInhaltPx = int(180.0 / 25.4 * 96.0);

QString esc(const QString& s)
{
    return s.toHtmlEscaped();
}

// ---------------------------------------------------------------
//  HTML-Bausteine
// ---------------------------------------------------------------

QString ueberschrift(const QString& text)
{
    return QStringLiteral("<h2>%1</h2>").arg(esc(text));
}

QString absatz(const QString& text, bool gedimmt = false)
{
    return gedimmt ? QStringLiteral("<p class='gedimmt'>%1</p>").arg(text) : QStringLiteral("<p>%1</p>").arg(text);
}

struct Kachel {
    QString wert, titel, notiz, farbe;
};

/// Kennzahl-Kacheln, je @p proZeile nebeneinander.
QString kacheln(const QVector<Kachel>& liste, int proZeile = 3)
{
    QString h = QStringLiteral("<table width='100%' cellspacing='6' cellpadding='8' class='kacheln'>");
    for (int i = 0; i < liste.size(); i += proZeile) {
        h += QStringLiteral("<tr>");
        for (int j = i; j < i + proZeile; ++j) {
            if (j >= liste.size()) {
                h += QStringLiteral("<td width='%1%'></td>").arg(100 / proZeile);
                continue;
            }
            const Kachel& k = liste.at(j);
            h += QStringLiteral("<td width='%1%' class='kachel'>"
                                "<span class='ktitel'>%2</span><br>"
                                "<span class='kwert' style='color:%3'>%4</span><br>"
                                "<span class='knotiz'>%5</span></td>")
                     .arg(100 / proZeile)
                     .arg(esc(k.titel), k.farbe.isEmpty() ? kText : k.farbe, esc(k.wert), esc(k.notiz));
        }
        h += QStringLiteral("</tr>");
    }
    return h + QStringLiteral("</table>");
}

/**
 * @brief Tabelle mit Kopfzeile und abwechselnd hinterlegten Zeilen.
 * @param rechts Spalten, die rechtsbündig (Zahlen) gesetzt werden
 * Zellinhalte werden escaped; ein Inhalt mit führendem "\x01" gilt als
 * fertiges HTML (z. B. farbige Markierung).
 */
QString tabelle(const QStringList& kopf, const QVector<QStringList>& zeilen, const QSet<int>& rechts = {},
                const QVector<int>& breiten = {})
{
    // <thead>: Kopfzeile wird auf jeder Folgeseite wiederholt
    QString h = QStringLiteral("<table width='100%' cellspacing='0' cellpadding='4' class='daten'><thead><tr>");
    for (int i = 0; i < kopf.size(); ++i)
        h += QStringLiteral("<th align='%1'%2>%3</th>")
                 .arg(rechts.contains(i) ? QStringLiteral("right") : QStringLiteral("left"),
                      i < breiten.size() ? QStringLiteral(" width='%1%'").arg(breiten.at(i)) : QString(),
                      esc(kopf.at(i)));
    h += QStringLiteral("</tr></thead>");
    for (int z = 0; z < zeilen.size(); ++z) {
        h += z % 2 ? QStringLiteral("<tr class='ungerade'>") : QStringLiteral("<tr>");
        const QStringList& zeile = zeilen.at(z);
        for (int i = 0; i < zeile.size(); ++i) {
            const QString& zelle = zeile.at(i);
            const QString inhalt = zelle.startsWith(QChar(1)) ? zelle.mid(1) : esc(zelle);
            h += QStringLiteral("<td align='%1'>%2</td>")
                     .arg(rechts.contains(i) ? QStringLiteral("right") : QStringLiteral("left"), inhalt);
        }
        h += QStringLiteral("</tr>");
    }
    return h + QStringLiteral("</table>");
}

/// Zweispaltige Liste "Bezeichnung: Wert" als vierspaltige Tabelle (zwei Paare je Zeile).
QString steckbrief(const QVector<QPair<QString, QString>>& paare)
{
    QString h = QStringLiteral("<table width='100%' cellspacing='0' cellpadding='3' class='steckbrief'>");
    for (int i = 0; i < paare.size(); i += 2) {
        h += QStringLiteral("<tr>");
        for (int j = i; j < i + 2; ++j) {
            if (j < paare.size())
                h += QStringLiteral("<td width='18%' class='gedimmt'>%1</td><td width='32%'>%2</td>")
                         .arg(esc(paare.at(j).first), esc(paare.at(j).second));
            else
                h += QStringLiteral("<td></td><td></td>");
        }
        h += QStringLiteral("</tr>");
    }
    return h + QStringLiteral("</table>");
}

QString markierung(const QString& text, const QColor& farbe)
{
    return QStringLiteral("\x01<span style='color:%1; font-weight:600'>%2</span>").arg(farbe.name(), esc(text));
}

QString bild(const QString& name, int hoehePx)
{
    return QStringLiteral("<p><img src='bild://%1' width='%2' height='%3'></p>").arg(name).arg(kInhaltPx).arg(hoehePx);
}

// ---------------------------------------------------------------
//  Diagramme als Bild (immer helles Farbschema)
// ---------------------------------------------------------------

QImage alsBild(QWidget* w, int hoehePx)
{
    QPalette hell;
    hell.setColor(QPalette::Base, Qt::white);
    hell.setColor(QPalette::Window, Qt::white);
    hell.setColor(QPalette::Text, QColor(kText));
    hell.setColor(QPalette::WindowText, QColor(kText));
    hell.setColor(QPalette::Mid, QColor(203, 213, 225));
    hell.setColor(QPalette::PlaceholderText, QColor(kGedimmt));
    hell.setColor(QPalette::Highlight, QColor(kAkzent));
    w->setAttribute(Qt::WA_DontShowOnScreen, true);
    w->ensurePolished();
    w->setPalette(hell);
    w->setFont(QFont(w->font().family(), 8));
    w->resize(kInhaltPx, hoehePx);

    constexpr double faktor = 3.0; // scharf im Druck
    QImage img(QSize(kInhaltPx, hoehePx) * faktor, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(faktor);
    img.fill(Qt::white);
    w->render(&img);
    return img;
}

// ---------------------------------------------------------------
//  Seiten zeichnen
// ---------------------------------------------------------------

struct Dokument {
    QString titel;
    QString untertitel;
    QString html;
    QHash<QString, QImage> bilder;
};

QString stylesheet()
{
    return QStringLiteral(
               "body { color:%1; }"
               "h2 { font-size:12.5pt; font-weight:700; color:%1; margin-top:14px; margin-bottom:6px; }"
               "p { margin-top:0px; margin-bottom:6px; }"
               ".gedimmt { color:%2; }"
               "table.daten th { background-color:#f1f5f9; color:%2; font-size:8pt; font-weight:600; }"
               "table.daten td { font-size:8.5pt; }"
               "tr.ungerade td { background-color:%3; }"
               "td.kachel { background-color:%3; }"
               ".ktitel { font-size:8pt; color:%2; }"
               ".kwert { font-size:15pt; font-weight:700; }"
               ".knotiz { font-size:7.5pt; color:%2; }"
               "table.steckbrief td { font-size:8.5pt; }"
               "li { margin-bottom:3px; }")
        .arg(kText, kGedimmt, kKachel);
}

void kopfUndFuss(QPainter& p, const QRectF& seite, const Dokument& dok, const QString& rolle, int nr, int anzahl)
{
    const double rand = 15 * kMm;
    // Kopf: Logo, Titel, Untertitel, Akzentlinie
    // Logo als Bild: halbtransparente Verläufe setzt der PDF-Treiber nicht überall gleich um
    p.drawImage(QRectF(rand, 9 * kMm, 13 * kMm, 13 * kMm), Stil::logo(512).toImage());
    QFont titelSchrift = p.font();
    titelSchrift.setPointSizeF(15);
    titelSchrift.setBold(true);
    p.setFont(titelSchrift);
    p.setPen(QColor(kText));
    p.drawText(QRectF(rand + 17 * kMm, 9 * kMm, seite.width() - 2 * rand - 17 * kMm, 7.5 * kMm),
               Qt::AlignLeft | Qt::AlignVCenter, dok.titel);
    QFont klein = p.font();
    klein.setPointSizeF(8.5);
    klein.setBold(false);
    p.setFont(klein);
    p.setPen(QColor(kGedimmt));
    p.drawText(QRectF(rand + 17 * kMm, 16.5 * kMm, seite.width() - 2 * rand - 17 * kMm, 5.5 * kMm),
               Qt::AlignLeft | Qt::AlignVCenter, dok.untertitel);
    // rechts oben: Programmname
    QFont marke = klein;
    marke.setBold(true);
    p.setFont(marke);
    p.setPen(QColor(kText));
    p.drawText(QRectF(seite.width() - rand - 60 * kMm, 9 * kMm, 60 * kMm, 7.5 * kMm), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("SolarMonitor"));
    p.fillRect(QRectF(rand, 25 * kMm, seite.width() - 2 * rand, 0.6 * kMm), QColor(kAkzent));

    // Fuß: Linie, Datum/Rolle links, Seitenzahl rechts
    const double fussY = seite.height() - 13 * kMm;
    p.fillRect(QRectF(rand, fussY, seite.width() - 2 * rand, 0.25 * kMm), QColor(kLinie));
    QFont fuss = klein;
    fuss.setPointSizeF(7.5);
    p.setFont(fuss);
    p.setPen(QColor(kGedimmt));
    const QRectF fussZeile(rand, fussY + 1.5 * kMm, seite.width() - 2 * rand, 5 * kMm);
    p.drawText(fussZeile, Qt::AlignLeft | Qt::AlignVCenter,
               PdfBericht::tr("Erstellt am %1 mit SolarMonitor · Rolle: %2")
                   .arg(Anzeige::zeitpunkt(QDateTime::currentDateTime()), rolle));
    p.drawText(fussZeile, Qt::AlignRight | Qt::AlignVCenter, PdfBericht::tr("Seite %1 von %2").arg(nr).arg(anzahl));
}

bool schreibePdf(const Dokument& dok, const QString& pfad, const QString& rolle, QString* fehler)
{
    QPdfWriter pdf(pfad);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    pdf.setResolution(kDpi);
    pdf.setTitle(dok.titel);
    pdf.setCreator(QStringLiteral("SolarMonitor"));

    QPainter p;
    if (!p.begin(&pdf)) {
        if (fehler)
            *fehler = PdfBericht::tr("Die Datei %1 kann nicht geschrieben werden (evtl. in einem anderen Programm geöffnet).")
                          .arg(QFileInfo(pfad).fileName());
        return false;
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRectF seite(0, 0, pdf.width(), pdf.height());
    const QRectF inhalt(15 * kMm, 31 * kMm, seite.width() - 30 * kMm, seite.height() - 31 * kMm - 17 * kMm);

    QTextDocument doc;
    doc.documentLayout()->setPaintDevice(&pdf);
    doc.setDocumentMargin(0);
    QFont schrift = p.font();
    schrift.setPointSizeF(9);
    doc.setDefaultFont(schrift);
    doc.setDefaultStyleSheet(stylesheet());
    for (auto it = dok.bilder.constBegin(); it != dok.bilder.constEnd(); ++it)
        doc.addResource(QTextDocument::ImageResource, QUrl(QStringLiteral("bild://") + it.key()), it.value());
    doc.setHtml(QStringLiteral("<html><body>") + dok.html + QStringLiteral("</body></html>"));
    doc.setPageSize(inhalt.size()); // QTextDocument bricht selbst auf Seiten um

    // Überschriften nicht allein am Seitenende stehen lassen: liegt eine
    // Überschrift im unteren Achtel der Seite, beginnt sie eine neue Seite.
    for (bool geaendert = true; geaendert;) {
        geaendert = false;
        for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
            if (b.blockFormat().headingLevel() == 0
                || b.blockFormat().pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore)
                continue;
            const double y = doc.documentLayout()->blockBoundingRect(b).top();
            if (std::fmod(y, inhalt.height()) > inhalt.height() * 0.875) {
                QTextBlockFormat f = b.blockFormat();
                f.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
                QTextCursor(b).setBlockFormat(f);
                geaendert = true;
                break; // Layout hat sich verschoben -> von vorn prüfen
            }
        }
    }

    const int seiten = doc.pageCount();
    for (int i = 0; i < seiten; ++i) {
        if (i > 0)
            pdf.newPage();
        kopfUndFuss(p, seite, dok, rolle, i + 1, seiten);
        p.save();
        p.translate(inhalt.left(), inhalt.top() - i * inhalt.height());
        doc.drawContents(&p, QRectF(0, i * inhalt.height(), inhalt.width(), inhalt.height()));
        p.restore();
    }
    p.end();
    return true;
}

QString zeitraum(const QVector<TagesErtrag>& tage)
{
    if (tage.isEmpty())
        return QString();
    return QStringLiteral("%1 – %2").arg(Anzeige::datum(tage.first().tag), Anzeige::datum(tage.last().tag));
}

QColor anomalieFarbe(const Anomalie& a)
{
    return a.istHinweis() ? QColor(37, 99, 235) : QColor(217, 119, 6);
}

} // namespace

// ===================================================================
//  Anlagenbericht
// ===================================================================

bool PdfBericht::anlage(const AppKontext& kontext, const QString& anlageId, const QString& pfad, QString* fehler)
{
    const Anlage* a = kontext.daten->anlage(anlageId);
    if (!a) {
        if (fehler)
            *fehler = tr("Die Anlage %1 ist nicht vorhanden.").arg(anlageId);
        return false;
    }
    const Wechselrichter* wr = kontext.daten->wechselrichter(a->wechselrichterId);
    const Wetterstation* ws = kontext.daten->wetterstation(a->wetterstationId);

    Dokument dok;
    dok.titel = tr("Anlagenbericht %1").arg(a->anlageId);
    dok.untertitel = QStringLiteral("%1 · %2 · %3 kWp · %4")
                         .arg(Anzeige::typName(a->typ), a->ort, Anzeige::zahl(a->leistungKwp, 1),
                              Anzeige::statusName(a->status));
    QString& h = dok.html;

    // ---------- Stammdaten ----------
    h += ueberschrift(tr("Stammdaten"));
    h += steckbrief({
        {tr("Anlage"), a->anlageId},
        {tr("Typ"), Anzeige::typName(a->typ)},
        {tr("Standort"), tr("%1 (%2° N, %3° O)").arg(a->ort, Anzeige::zahl(a->lat, 4), Anzeige::zahl(a->lon, 4))},
        {tr("Status"), Anzeige::statusName(a->status)},
        {tr("Leistung"), Anzeige::zahl(a->leistungKwp, 2) + QStringLiteral(" kWp")},
        {tr("Ausrichtung / Neigung"), QStringLiteral("%1° / %2°").arg(a->ausrichtungGrad).arg(a->neigungGrad)},
        {tr("Wechselrichter"), wr ? QStringLiteral("%1 × %2 (%3 kW)").arg(a->anzahlWr).arg(wr->bezeichnung(), Anzeige::zahl(wr->nennleistungKw, 1))
                                  : a->wechselrichterId},
        {tr("Wetterstation"), ws ? QStringLiteral("%1 (%2)").arg(ws->stationId, ws->ort) : a->wetterstationId},
        {tr("Inbetriebnahme"), Anzeige::datum(a->inbetriebnahme)},
        {tr("Einspeisevergütung"), Anzeige::zahl(a->verguetungCtKwh, 2) + QStringLiteral(" ct/kWh")},
    });

    // ---------- Verlauf und Auswertung ----------
    std::shared_ptr<const Verlauf> verlauf = kontext.verlaeufe->ausCache(anlageId);
    QString verlaufFehler;
    if (!verlauf) {
        const QString datei = kontext.verlaeufe->pfadFuer(anlageId);
        if (datei.isEmpty()) {
            verlaufFehler = kontext.verlaeufe->fehlgrund(anlageId);
        } else {
            const VerlaufLadeErgebnis e = VerlaufRepository::lese(datei, anlageId);
            verlauf = e.verlauf;
            verlaufFehler = e.fehler;
        }
    }

    h += ueberschrift(tr("Ertrag und Performance Ratio"));
    if (!verlauf) {
        h += absatz(esc(tr("Für diese Anlage liegen keine auswertbaren Messwerte vor: %1").arg(verlaufFehler)), true);
    } else {
        const double acGrenze = wr ? wr->nennleistungKw * qMax(1, a->anzahlWr) : 0.0;
        const double median = kontext.analyse->medianPrStation(a->wetterstationId);
        Auswertung aus = ErtragsAuswertung::berechne(*verlauf, a->leistungKwp, acGrenze);
        if (!aus.gueltig) {
            h += absatz(esc(aus.grund), true);
        } else {
            ErtragsAuswertung::bewerteGegenStation(aus, median);
            const double prRef = median > 0.0 ? median : Schwellwerte::kStandardPr;
            const double soll = aus.einstrahlungKwhM2 * a->leistungKwp * prRef;
            const double verlust = qMax(0.0, soll - aus.energieKwh);
            const double erloes = aus.energieKwh * a->verguetungCtKwh / 100.0;

            h += absatz(esc(tr("Zeitraum %1 · Einstrahlung H = %2 kWh/m² · %3 Messpunkte im 15-Minuten-Raster")
                                .arg(zeitraum(aus.tage), Anzeige::zahl(aus.einstrahlungKwhM2, 1))
                                .arg(aus.messpunkte)),
                        true);
            h += kacheln({
                {Anzeige::kwh(aus.energieKwh), tr("Energieertrag E"), tr("erwartet %1").arg(Anzeige::kwh(soll)), {}},
                {Anzeige::zahl(aus.spezErtragKwhKwp, 1) + QStringLiteral(" kWh/kWp"), tr("Spezifischer Ertrag Y"),
                 tr("Ertrag je installiertem kWp"), {}},
                {Anzeige::pr(aus.pr), tr("Performance Ratio"),
                 median > 0.0 ? tr("Median der Station: %1").arg(Anzeige::pr(median)) : tr("kein Stationsvergleich"),
                 Anzeige::prFarbe(aus.pr).darker(130).name()},
                {Anzeige::prozent(aus.abdeckung, 1), tr("Datenabdeckung"),
                 tr("%1 Lücken von %2 Messpunkten").arg(aus.luecken).arg(aus.messpunkte), {}},
                {Anzeige::euro(erloes), tr("Erlös"), tr("Ertrag × Einspeisevergütung"), {}},
                {Anzeige::euro(verlust * a->verguetungCtKwh / 100.0), tr("Ertragsverlust"),
                 Anzeige::kwh(verlust), verlust > 0.1 * soll ? QStringLiteral("#b45309") : QString()},
            });

            // Tageserträge
            BalkenDiagramm tage;
            QVector<BalkenDiagramm::Balken> balken;
            double summe = 0.0;
            for (const TagesErtrag& t : std::as_const(aus.tage)) {
                balken.append({t.tag.toString(QStringLiteral("dd.MM.")), t.energieKwh, Anzeige::prFarbe(t.pr), {}});
                summe += t.energieKwh;
            }
            tage.setDaten(balken, tr("Tageserträge (Farbe = Tages-PR)"), QStringLiteral("kWh"));
            if (!aus.tage.isEmpty())
                tage.setReferenz(summe / aus.tage.size(), tr("Mittel %1").arg(Anzeige::kwh(summe / aus.tage.size())));
            dok.bilder.insert(QStringLiteral("tage"), alsBild(&tage, 210));
            h += bild(QStringLiteral("tage"), 210);

            // Leistungsverlauf
            VerlaufPlot plot;
            plot.setDaten(verlauf->zeit, verlauf->leistungW, tr("Eingespeiste Leistung"), QStringLiteral("W"),
                          QColor(22, 119, 255), true);
            dok.bilder.insert(QStringLiteral("leistung"), alsBild(&plot, 190));
            h += bild(QStringLiteral("leistung"), 190);

            // Fehlerbilder
            h += ueberschrift(tr("Erkannte Fehlerbilder"));
            if (aus.anomalien.isEmpty()) {
                h += absatz(tr("Keine Auffälligkeiten erkannt."));
            } else {
                QVector<QStringList> zeilen;
                for (const Anomalie& an : std::as_const(aus.anomalien))
                    zeilen.append({markierung(Anomalie::typName(an.typ), anomalieFarbe(an)),
                                   an.istHinweis() ? tr("Hinweis") : tr("Befund"), an.text()});
                h += tabelle({tr("Fehlerbild"), tr("Art"), tr("Beschreibung")}, zeilen, {}, {20, 10, 70});
            }
        }
    }

    // ---------- Störungen ----------
    h += ueberschrift(tr("Störungsmeldungen"));
    QVector<Stoerung> stoerungen = kontext.daten->stoerungenVon(anlageId);
    std::sort(stoerungen.begin(), stoerungen.end(),
              [](const Stoerung& x, const Stoerung& y) { return x.zeitpunkt > y.zeitpunkt; });
    if (stoerungen.isEmpty()) {
        h += absatz(tr("Keine Störungsmeldungen vorhanden."));
    } else {
        QVector<QStringList> zeilen;
        for (const Stoerung& s : std::as_const(stoerungen))
            zeilen.append({Anzeige::zeitpunkt(s.zeitpunkt), s.stoerungId, s.fehlercode, s.beschreibung,
                           markierung(Anzeige::schwereName(s.schwere), Anzeige::schwereFarbe(s.schwere)),
                           Anzeige::stoerungsStatusName(s.status),
                           s.behobenAm.isValid() ? Anzeige::zeitpunkt(s.behobenAm) : QStringLiteral("–")});
        h += tabelle({tr("Zeitpunkt"), tr("Störung"), tr("Code"), tr("Beschreibung"), tr("Schwere"), tr("Status"),
                      tr("Behoben am")},
                     zeilen, {}, {15, 9, 7, 29, 10, 13, 17});
    }

    // ---------- Datenprüfung ----------
    const QVector<Pruefmeldung> meldungen = kontext.daten->pruefmeldungenFuer(anlageId);
    if (!meldungen.isEmpty()) {
        h += ueberschrift(tr("Hinweise aus der Datenprüfung"));
        QVector<QStringList> zeilen;
        for (const Pruefmeldung& m : meldungen)
            zeilen.append({m.stufe == Pruefmeldung::Stufe::Fehler    ? markierung(tr("Fehler"), QColor(192, 57, 43))
                           : m.stufe == Pruefmeldung::Stufe::Warnung ? markierung(tr("Warnung"), QColor(217, 119, 6))
                                                                     : markierung(tr("Info"), QColor(37, 99, 235)),
                           m.zeile > 0 ? QStringLiteral("%1:%2").arg(m.datei).arg(m.zeile) : m.datei, m.text});
        h += tabelle({tr("Stufe"), tr("Datei"), tr("Meldung")}, zeilen, {}, {11, 22, 67});
    }

    return schreibePdf(dok, pfad, Rollen::anzeigename(kontext.rolle), fehler);
}

// ===================================================================
//  Flottenbericht
// ===================================================================

bool PdfBericht::flotte(const AppKontext& kontext, const QString& pfad, QString* fehler)
{
    const Datenbasis& d = *kontext.daten;
    const FlottenAnalyse& fa = *kontext.analyse;

    // ---------- Kennzahlen sammeln ----------
    double kwp = 0.0, energie = 0.0, verlustEuro = 0.0, verlustKwh = 0.0;
    int ausgewertet = 0, auffaellig = 0;
    std::map<QString, std::pair<int, double>> nachStatus; // Status -> (Anzahl, kWp)
    struct Station {
        int anlagen = 0, ausgewertet = 0;
        double energie = 0.0, verlustEuro = 0.0;
    };
    std::map<QString, Station> stationen;
    std::map<Anomalie::Typ, int> fehlerbilder;
    QVector<const Kennzahl*> verluste;

    for (const Anlage& a : d.anlagen()) {
        kwp += a.leistungKwp;
        auto& s = nachStatus[a.status];
        ++s.first;
        s.second += a.leistungKwp;
        Station& st = stationen[a.wetterstationId];
        ++st.anlagen;
        const Kennzahl* k = fa.kennzahl(a.anlageId);
        if (!k || !k->ok)
            continue;
        ++ausgewertet;
        energie += k->auswertung.energieKwh;
        verlustEuro += k->verlustEuro;
        verlustKwh += k->verlustKwh;
        ++st.ausgewertet;
        st.energie += k->auswertung.energieKwh;
        st.verlustEuro += k->verlustEuro;
        if (k->auswertung.istAuffaellig())
            ++auffaellig;
        QSet<int> gezaehlt;
        for (const Anomalie& an : k->auswertung.anomalien)
            if (!gezaehlt.contains(int(an.typ))) {
                gezaehlt.insert(int(an.typ));
                ++fehlerbilder[an.typ];
            }
        if (k->verlustEuro > 0.0)
            verluste.append(k);
    }
    int offen = 0, kritisch = 0;
    QVector<Stoerung> offeneKritische;
    for (const Stoerung& s : d.stoerungen()) {
        if (!s.istOffen())
            continue;
        ++offen;
        if (s.istKritisch()) {
            ++kritisch;
            offeneKritische.append(s);
        }
    }

    // Zeitraum aus einem beliebigen Verlauf
    QString zeitraumText;
    for (const QString& id : kontext.verlaeufe->anlagenMitVerlauf()) {
        const VerlaufLadeErgebnis e = VerlaufRepository::lese(kontext.verlaeufe->pfadFuer(id), id);
        if (e.ok() && e.verlauf->anzahl() > 0) {
            zeitraumText = QStringLiteral("%1 – %2").arg(Anzeige::datum(e.verlauf->zeit.first().date()),
                                                        Anzeige::datum(e.verlauf->zeit.last().date()));
            break;
        }
    }

    Dokument dok;
    dok.titel = tr("Flottenbericht");
    dok.untertitel = tr("%1 PV-Anlagen · %2 · Auswertungszeitraum %3")
                         .arg(d.anlagen().size())
                         .arg(kwp >= 1000.0 ? Anzeige::zahl(kwp / 1000.0, 2) + QStringLiteral(" MWp")
                                            : Anzeige::zahl(kwp, 1) + QStringLiteral(" kWp"),
                              zeitraumText);
    QString& h = dok.html;

    // ---------- Übersicht ----------
    h += ueberschrift(tr("Übersicht"));
    const int inBetrieb = nachStatus.count(QStringLiteral("in_betrieb")) ? nachStatus[QStringLiteral("in_betrieb")].first : 0;
    h += kacheln({
        {QString::number(d.anlagen().size()), tr("Anlagen"), tr("davon %1 in Betrieb").arg(inBetrieb), {}},
        {kwp >= 1000.0 ? Anzeige::zahl(kwp / 1000.0, 2) + QStringLiteral(" MWp") : Anzeige::zahl(kwp, 1) + QStringLiteral(" kWp"),
         tr("Installierte Leistung"), tr("Summe aller Anlagen"), {}},
        {Anzeige::kwh(energie), tr("Energieertrag"), tr("%1 Anlagen ausgewertet").arg(ausgewertet), {}},
        {Anzeige::pr(fa.medianPrFlotte()), tr("Median-PR der Flotte"), tr("über alle ausgewerteten Anlagen"),
         Anzeige::prFarbe(fa.medianPrFlotte()).darker(130).name()},
        {Anzeige::euro(verlustEuro), tr("Geschätzter Ertragsverlust"), Anzeige::kwh(verlustKwh), QStringLiteral("#b45309")},
        {QString::number(offen), tr("Offene Störungen"), tr("davon %1 kritisch").arg(kritisch),
         kritisch > 0 ? QStringLiteral("#b91c1c") : QString()},
        {QString::number(auffaellig), tr("Anlagen mit Fehlerbild"),
         Anzeige::prozent(ausgewertet > 0 ? double(auffaellig) / ausgewertet : 0.0, 1) + tr(" der ausgewerteten"), {}},
        {QString::number(d.anlagen().size() - ausgewertet), tr("Ohne Auswertung"), tr("Verlauf fehlt oder defekt"), {}},
        {QString::number(d.anzahlMeldungen(Pruefmeldung::Stufe::Fehler)), tr("Datenfehler beim Einlesen"),
         tr("%1 Warnungen").arg(d.anzahlMeldungen(Pruefmeldung::Stufe::Warnung)), {}},
    });

    // ---------- Status ----------
    h += ueberschrift(tr("Anlagen nach Status"));
    {
        QVector<QStringList> zeilen;
        for (const auto& [status, werte] : nachStatus)
            zeilen.append({markierung(Anzeige::statusName(status), Anzeige::statusFarbe(status)),
                           QString::number(werte.first),
                           Anzeige::prozent(double(werte.first) / qMax<qsizetype>(1, d.anlagen().size()), 1),
                           Anzeige::zahl(werte.second, 1) + QStringLiteral(" kWp")});
        h += tabelle({tr("Status"), tr("Anlagen"), tr("Anteil"), tr("Leistung")}, zeilen, {1, 2, 3}, {40, 20, 20, 20});
    }

    // ---------- Wetterstationen ----------
    h += ueberschrift(tr("Vergleich der Wetterstationen"));
    {
        QVector<BalkenDiagramm::Balken> balken;
        QVector<QStringList> zeilen;
        QStringList ohne;
        for (const auto& [id, st] : stationen) {
            if (st.ausgewertet == 0) {
                ohne.append(id);
                continue;
            }
            const double median = fa.medianPrStation(id);
            const Wetterstation* ws = d.wetterstation(id);
            if (median > 0.0)
                balken.append({id, median * 100.0, Anzeige::prFarbe(median), {}});
            zeilen.append({id, ws ? ws->ort : QStringLiteral("–"),
                           QStringLiteral("%1 / %2").arg(st.ausgewertet).arg(st.anlagen), Anzeige::pr(median),
                           Anzeige::kwh(st.energie), Anzeige::euro(st.verlustEuro)});
        }
        BalkenDiagramm diagramm;
        diagramm.setDaten(balken, tr("Median-PR je Wetterstation"), QStringLiteral("%"), 1);
        if (fa.medianPrFlotte() > 0.0)
            diagramm.setReferenz(fa.medianPrFlotte() * 100.0, tr("Flotte %1").arg(Anzeige::pr(fa.medianPrFlotte())));
        dok.bilder.insert(QStringLiteral("stationen"), alsBild(&diagramm, 220));
        h += bild(QStringLiteral("stationen"), 220);
        h += tabelle({tr("Station"), tr("Ort"), tr("ausgewertet / Anlagen"), tr("Median-PR"), tr("Energie"), tr("Verlust")},
                     zeilen, {2, 3, 4, 5}, {11, 25, 18, 14, 16, 16});
        if (!ohne.isEmpty())
            h += absatz(esc(tr("Ohne ausgewertete Anlagen (keine Verlaufsdaten): %1").arg(ohne.join(QStringLiteral(", ")))),
                        true);
    }

    // ---------- Größte Verluste ----------
    h += ueberschrift(tr("Die 15 Anlagen mit dem größten Ertragsverlust"));
    std::sort(verluste.begin(), verluste.end(),
              [](const Kennzahl* x, const Kennzahl* y) { return x->verlustEuro > y->verlustEuro; });
    {
        QVector<QStringList> zeilen;
        for (int i = 0; i < qMin<qsizetype>(15, verluste.size()); ++i) {
            const Kennzahl* k = verluste.at(i);
            const Anlage* a = d.anlage(k->anlageId);
            QStringList bilder;
            for (const Anomalie& an : k->auswertung.anomalien)
                if (!an.istHinweis())
                    bilder.append(Anomalie::typName(an.typ));
            bilder.removeDuplicates();
            zeilen.append({k->anlageId, a ? a->ort : QString(), a ? Anzeige::zahl(a->leistungKwp, 1) : QString(),
                           Anzeige::pr(k->auswertung.pr), Anzeige::kwh(k->verlustKwh), Anzeige::euro(k->verlustEuro),
                           bilder.isEmpty() ? QStringLiteral("–") : bilder.join(QStringLiteral(", "))});
        }
        h += tabelle({tr("Anlage"), tr("Ort"), tr("kWp"), tr("PR"), tr("Verlust"), tr("Verlust (€)"), tr("Fehlerbilder")},
                     zeilen, {2, 3, 4, 5}, {10, 18, 8, 9, 13, 12, 30});
    }

    // ---------- Fehlerbilder ----------
    h += ueberschrift(tr("Häufigkeit der Fehlerbilder"));
    {
        QVector<QStringList> zeilen;
        QVector<std::pair<Anomalie::Typ, int>> liste(fehlerbilder.begin(), fehlerbilder.end());
        std::sort(liste.begin(), liste.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
        for (const auto& [typ, anzahl] : liste) {
            Anomalie muster;
            muster.typ = typ;
            zeilen.append({markierung(Anomalie::typName(typ), anomalieFarbe(muster)),
                           muster.istHinweis() ? tr("Hinweis") : tr("Befund"), QString::number(anzahl),
                           Anzeige::prozent(ausgewertet > 0 ? double(anzahl) / ausgewertet : 0.0, 1)});
        }
        if (zeilen.isEmpty())
            h += absatz(tr("Keine Fehlerbilder erkannt."));
        else
            h += tabelle({tr("Fehlerbild"), tr("Art"), tr("Anlagen"), tr("Anteil")}, zeilen, {2, 3}, {40, 20, 20, 20});
    }

    // ---------- Offene kritische Störungen ----------
    h += ueberschrift(tr("Offene kritische Störungen"));
    std::sort(offeneKritische.begin(), offeneKritische.end(),
              [](const Stoerung& x, const Stoerung& y) { return x.zeitpunkt > y.zeitpunkt; });
    if (offeneKritische.isEmpty()) {
        h += absatz(tr("Keine offenen kritischen Störungen."));
    } else {
        QVector<QStringList> zeilen;
        for (int i = 0; i < qMin<qsizetype>(25, offeneKritische.size()); ++i) {
            const Stoerung& s = offeneKritische.at(i);
            const Anlage* a = d.anlage(s.anlageId);
            zeilen.append({Anzeige::zeitpunkt(s.zeitpunkt), s.stoerungId, s.anlageId, a ? a->ort : QString(),
                           s.fehlercode, s.beschreibung, Anzeige::stoerungsStatusName(s.status)});
        }
        h += tabelle({tr("Zeitpunkt"), tr("Störung"), tr("Anlage"), tr("Ort"), tr("Code"), tr("Beschreibung"), tr("Status")},
                     zeilen, {}, {15, 10, 9, 15, 7, 30, 14});
        if (offeneKritische.size() > 25)
            h += absatz(esc(tr("… und %1 weitere.").arg(offeneKritische.size() - 25)), true);
    }

    // ---------- Datenqualität ----------
    h += ueberschrift(tr("Datenqualität"));
    {
        std::map<QString, std::array<int, 3>> proDatei; // Fehler, Warnung, Info
        for (const Pruefmeldung& m : d.pruefmeldungen()) {
            auto& z = proDatei[m.datei];
            ++z[m.stufe == Pruefmeldung::Stufe::Fehler ? 0 : m.stufe == Pruefmeldung::Stufe::Warnung ? 1 : 2];
        }
        h += absatz(esc(tr("Beim Einlesen geprüft: Pflichtfelder, Wertebereiche, Verweise zwischen den Dateien, "
                           "doppelte IDs und die Verlaufsdateien. Details stehen in der Ansicht „Datenprüfung“.")),
                    true);
        QVector<QStringList> zeilen;
        for (const auto& [datei, z] : proDatei)
            zeilen.append({datei, QString::number(z[0]), QString::number(z[1]), QString::number(z[2])});
        h += tabelle({tr("Datei"), tr("Fehler"), tr("Warnungen"), tr("Hinweise")}, zeilen, {1, 2, 3}, {46, 18, 18, 18});
    }

    h += absatz(esc(tr("Berechnung: PR = E / (H · Ppeak) über Intervalle mit Leistung und Einstrahlung; "
                       "Verlust = max(0, H · Ppeak · Median-PR der Station − E), bewertet mit der "
                       "Einspeisevergütung der Anlage.")),
                true);

    return schreibePdf(dok, pfad, Rollen::anzeigename(kontext.rolle), fehler);
}
