#include "vergleich_dialog.h"

#include <cmath>

#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include "eval/flotten_analyse.h"
#include "eval/schwellwerte.h"
#include "model/datenbasis.h"
#include "model/favoriten.h"
#include "repo/verlauf_repository.h"
#include "util/anzeige.h"
#include "util/stil.h"

namespace {

/// Welcher Wert ist besser? (für die grüne Markierung)
enum class Besser { Keins, Hoeher, Niedriger };

struct Zeile {
    QString titel;
    QString text[2];
    double  wert[2] = {std::nan(""), std::nan("")};
    Besser  besser = Besser::Keins;
};

QString anlageText(const Anlage& a)
{
    return QStringLiteral("%1 – %2 (%3 kWp)").arg(a.anlageId, a.ort, Anzeige::zahl(a.leistungKwp, 1));
}

} // namespace

// ===================================================================
//  Dialog
// ===================================================================

VergleichDialog::VergleichDialog(const AppKontext& kontext, const QString& erste, QWidget* parent)
    : QDialog(parent)
    , m_kontext(kontext)
{
    setWindowTitle(tr("Anlagen vergleichen"));
    resize(1000, 860);

    // ---------- Auswahl ----------
    auto* auswahl = new QGridLayout;
    auswahl->setHorizontalSpacing(10);
    for (int i = 0; i < 2; ++i) {
        auto* c = new QComboBox(this);
        c->setObjectName(i == 0 ? QStringLiteral("VergleichA") : QStringLiteral("VergleichB"));
        c->setEditable(true);
        c->setInsertPolicy(QComboBox::NoInsert);
        c->setMaxVisibleItems(16);
        // Favoriten zuerst, dann alle übrigen Anlagen
        const QStringList& favoriten = Favoriten::instanz()->liste();
        for (const QString& id : favoriten)
            if (const Anlage* a = m_kontext.daten->anlage(id))
                c->addItem(Stil::icon(Stil::Symbol::SternVoll, QColor(234, 179, 8)), anlageText(*a), a->anlageId);
        if (c->count() > 0)
            c->insertSeparator(c->count());
        for (const Anlage& a : m_kontext.daten->anlagen())
            if (!favoriten.contains(a.anlageId))
                c->addItem(anlageText(a), a.anlageId);
        c->completer()->setFilterMode(Qt::MatchContains);
        c->completer()->setCompletionMode(QCompleter::PopupCompletion);
        c->setMinimumContentsLength(28);
        m_wahl[i] = c;

        auto* punkt = new QLabel(this);
        QPixmap pm(14, 14);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(TagesVergleich::farbe(i));
        p.drawRoundedRect(QRectF(1, 1, 12, 12), 3, 3);
        p.end();
        punkt->setPixmap(pm);

        m_oeffnen[i] = new QPushButton(tr("Öffnen …"), this);
        m_oeffnen[i]->setAutoDefault(false);
        connect(m_oeffnen[i], &QPushButton::clicked, this, [this, i] {
            const QString id = gewaehlt(i);
            if (!id.isEmpty())
                emit anlageOeffnen(id);
        });
        auswahl->addWidget(new QLabel(i == 0 ? tr("Anlage A:") : tr("Anlage B:"), this), i, 0);
        auswahl->addWidget(punkt, i, 1);
        auswahl->addWidget(c, i, 2);
        auswahl->addWidget(m_oeffnen[i], i, 3);
        connect(c, &QComboBox::currentIndexChanged, this, &VergleichDialog::aktualisiere);
    }
    auto* tauschen = new QToolButton(this);
    tauschen->setText(QStringLiteral("⇅"));
    tauschen->setToolTip(tr("A und B tauschen"));
    tauschen->setAutoRaise(true);
    connect(tauschen, &QToolButton::clicked, this, [this] { waehle(gewaehlt(1), gewaehlt(0)); });
    auswahl->addWidget(tauschen, 0, 4, 2, 1);
    auswahl->setColumnStretch(2, 1);

    // ---------- Tabelle ----------
    m_tabelle = new QTableWidget(0, 3, this);
    m_tabelle->setHorizontalHeaderLabels({tr("Merkmal"), tr("Anlage A"), tr("Anlage B")});
    m_tabelle->verticalHeader()->setVisible(false);
    m_tabelle->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabelle->setSelectionMode(QAbstractItemView::NoSelection);
    m_tabelle->setShowGrid(false);
    m_tabelle->setFocusPolicy(Qt::NoFocus);
    m_tabelle->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tabelle->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tabelle->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tabelle->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tabelle->verticalHeader()->setDefaultSectionSize(26);

    // ---------- Diagramm ----------
    m_diagramm = new TagesVergleich(this);
    m_diagrammHinweis = new QLabel(this);
    m_diagrammHinweis->setObjectName(QStringLiteral("Gedimmt"));
    m_diagrammHinweis->setWordWrap(true);

    auto* hinweis = new QLabel(tr("Grün: jeweils der bessere Wert. Der Tagesvergleich zeigt den spezifischen Ertrag "
                                  "(kWh je kWp), damit unterschiedlich große Anlagen vergleichbar sind."),
                               this);
    hinweis->setObjectName(QStringLiteral("Gedimmt"));
    hinweis->setWordWrap(true);

    auto* knoepfe = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(knoepfe, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* v = new QVBoxLayout(this);
    v->setSpacing(10);
    v->addLayout(auswahl);
    v->addWidget(m_tabelle, 3);
    v->addWidget(hinweis);
    v->addWidget(m_diagramm, 2);
    v->addWidget(m_diagrammHinweis);
    v->addWidget(knoepfe);

    connect(m_kontext.verlaeufe, &VerlaufRepository::verlaufGeladen, this, &VergleichDialog::verlaufGeladen);

    // Vorbelegung: übergebene bzw. erste Favoriten-Anlage, dazu ein passender Partner
    QString a = erste;
    if (a.isEmpty() && !Favoriten::instanz()->liste().isEmpty())
        a = Favoriten::instanz()->liste().first();
    if (a.isEmpty() && !m_kontext.daten->anlagen().isEmpty())
        a = m_kontext.daten->anlagen().first().anlageId;
    waehle(a, vorschlagFuer(a));
}

void VergleichDialog::waehle(const QString& a, const QString& b)
{
    for (int i = 0; i < 2; ++i) {
        QSignalBlocker sperre(m_wahl[i]);
        const int idx = m_wahl[i]->findData(i == 0 ? a : b);
        if (idx >= 0)
            m_wahl[i]->setCurrentIndex(idx);
    }
    aktualisiere();
}

QString VergleichDialog::gewaehlt(int seite) const
{
    return m_wahl[seite]->currentData().toString();
}

QString VergleichDialog::vorschlagFuer(const QString& a) const
{
    // Am aussagekräftigsten: ein weiterer Favorit, sonst eine Anlage an derselben
    // Wetterstation (gleiches Wetter) mit ähnlicher Leistung
    for (const QString& id : Favoriten::instanz()->liste())
        if (id != a && m_kontext.daten->anlage(id))
            return id;
    const Anlage* basis = m_kontext.daten->anlage(a);
    if (!basis)
        return QString();
    QString bester;
    double abstand = 1e18;
    for (const Anlage& x : m_kontext.daten->anlagen()) {
        if (x.anlageId == a || x.wetterstationId != basis->wetterstationId)
            continue;
        const double d = std::abs(x.leistungKwp - basis->leistungKwp);
        if (d < abstand) {
            abstand = d;
            bester = x.anlageId;
        }
    }
    if (bester.isEmpty())
        for (const Anlage& x : m_kontext.daten->anlagen())
            if (x.anlageId != a)
                return x.anlageId;
    return bester;
}

void VergleichDialog::aktualisiere()
{
    fuelleTabelle();
    // Tageswerte: Verläufe (asynchron) laden
    for (int i = 0; i < 2; ++i) {
        const QString id = gewaehlt(i);
        m_oeffnen[i]->setEnabled(!id.isEmpty());
        if (id == m_tageFuer[i])
            continue;
        m_tageFuer[i] = id;
        m_tage[i].clear();
        m_tageGeladen[i] = false;
        if (!id.isEmpty() && m_kontext.verlaeufe->hatVerlauf(id))
            m_kontext.verlaeufe->ladeAsync(id);
        else
            m_tageGeladen[i] = true; // kein Verlauf: leer lassen
    }
    m_diagramm->setDaten(gewaehlt(0), m_tage[0], gewaehlt(1), m_tage[1]);
    aktualisiereHinweis();
}

void VergleichDialog::verlaufGeladen(const QString& anlageId, const VerlaufLadeErgebnis& e)
{
    bool betroffen = false;
    for (int i = 0; i < 2; ++i) {
        if (anlageId != m_tageFuer[i] || m_tageGeladen[i])
            continue;
        betroffen = true;
        m_tageGeladen[i] = true;
        const Anlage* a = m_kontext.daten->anlage(anlageId);
        if (!a || !e.ok() || !a->hatGueltigeLeistung())
            continue;
        const Wechselrichter* wr = m_kontext.daten->wechselrichter(a->wechselrichterId);
        const double acGrenze = wr ? wr->nennleistungKw * qMax(1, a->anzahlWr) : 0.0;
        const Auswertung aus = ErtragsAuswertung::berechne(*e.verlauf, a->leistungKwp, acGrenze);
        for (const TagesErtrag& t : aus.tage)
            m_tage[i].insert(t.tag, t.energieKwh / a->leistungKwp);
    }
    if (betroffen) {
        m_diagramm->setDaten(gewaehlt(0), m_tage[0], gewaehlt(1), m_tage[1]);
        aktualisiereHinweis();
    }
}

void VergleichDialog::aktualisiereHinweis()
{
    QStringList fehlt;
    for (int i = 0; i < 2; ++i)
        if (m_tageGeladen[i] && m_tage[i].isEmpty() && !gewaehlt(i).isEmpty() && !fehlt.contains(gewaehlt(i)))
            fehlt << gewaehlt(i);
    if (!m_tageGeladen[0] || !m_tageGeladen[1])
        m_diagrammHinweis->setText(tr("Tageswerte werden aus den Verläufen berechnet …"));
    else if (!fehlt.isEmpty())
        m_diagrammHinweis->setText(tr("Kein auswertbarer Verlauf für: %1").arg(fehlt.join(QStringLiteral(", "))));
    else
        m_diagrammHinweis->clear();
}

void VergleichDialog::fuelleTabelle()
{
    const Datenbasis& d = *m_kontext.daten;
    const Anlage* an[2] = {d.anlage(gewaehlt(0)), d.anlage(gewaehlt(1))};
    const Kennzahl* kz[2] = {nullptr, nullptr};
    for (int i = 0; i < 2; ++i)
        if (an[i])
            if (const Kennzahl* k = m_kontext.analyse->kennzahl(an[i]->anlageId); k && k->ok)
                kz[i] = k;

    QVector<Zeile> zeilen;
    auto text = [&](const QString& titel, auto&& wert) {
        Zeile z;
        z.titel = titel;
        for (int i = 0; i < 2; ++i)
            z.text[i] = an[i] ? wert(*an[i]) : QStringLiteral("–");
        zeilen.append(z);
    };
    // Zahl mit Vergleich; holen() liefert NaN, wenn nicht verfügbar
    auto zahl = [&](const QString& titel, Besser besser, auto&& holen, auto&& format) {
        Zeile z;
        z.titel = titel;
        z.besser = besser;
        for (int i = 0; i < 2; ++i) {
            z.wert[i] = an[i] ? holen(*an[i], kz[i]) : std::nan("");
            z.text[i] = std::isnan(z.wert[i]) ? QStringLiteral("–") : format(z.wert[i]);
        }
        zeilen.append(z);
    };
    const double nan = std::nan("");

    text(tr("Ort"), [](const Anlage& a) { return a.ort; });
    text(tr("Typ"), [](const Anlage& a) { return Anzeige::typName(a.typ); });
    text(tr("Status"), [](const Anlage& a) { return Anzeige::statusName(a.status); });
    zahl(tr("Installierte Leistung"), Besser::Keins, [](const Anlage& a, const Kennzahl*) { return a.leistungKwp; },
         [](double w) { return Anzeige::zahl(w, 1) + QStringLiteral(" kWp"); });
    text(tr("Ausrichtung / Neigung"),
         [](const Anlage& a) { return QStringLiteral("%1° / %2°").arg(a.ausrichtungGrad).arg(a.neigungGrad); });
    text(tr("Wechselrichter"), [&d](const Anlage& a) {
        const Wechselrichter* wr = d.wechselrichter(a.wechselrichterId);
        return wr ? QStringLiteral("%1 × %2").arg(a.anzahlWr).arg(wr->bezeichnung()) : a.wechselrichterId;
    });
    text(tr("Wetterstation"), [&d](const Anlage& a) {
        const Wetterstation* ws = d.wetterstation(a.wetterstationId);
        return ws ? QStringLiteral("%1 (%2)").arg(a.wetterstationId, ws->name) : a.wetterstationId;
    });
    text(tr("In Betrieb seit"), [](const Anlage& a) { return Anzeige::datum(a.inbetriebnahme); });
    zahl(tr("Energie (14 Tage)"), Besser::Keins,
         [nan](const Anlage&, const Kennzahl* k) { return k ? k->auswertung.energieKwh : nan; },
         [](double w) { return Anzeige::kwh(w); });
    zahl(tr("Spezifischer Ertrag"), Besser::Hoeher,
         [nan](const Anlage&, const Kennzahl* k) { return k ? k->auswertung.spezErtragKwhKwp : nan; },
         [](double w) { return Anzeige::zahl(w, 1) + QStringLiteral(" kWh/kWp"); });
    zahl(tr("Performance Ratio"), Besser::Hoeher,
         [nan](const Anlage&, const Kennzahl* k) { return k && k->auswertung.pr > 0 ? k->auswertung.pr : nan; },
         [](double w) { return Anzeige::pr(w); });
    zahl(tr("PR-Median der Wetterstation"), Besser::Keins,
         [nan](const Anlage&, const Kennzahl* k) { return k && k->medianPrStation > 0 ? k->medianPrStation : nan; },
         [](double w) { return Anzeige::pr(w); });
    zahl(tr("Geschätzter Ertragsverlust"), Besser::Niedriger,
         [nan](const Anlage&, const Kennzahl* k) { return k ? k->verlustEuro : nan; },
         [](double w) { return Anzeige::euro(w); });
    zahl(tr("Datenabdeckung"), Besser::Hoeher,
         [nan](const Anlage&, const Kennzahl* k) { return k ? k->auswertung.abdeckung : nan; },
         [](double w) { return Anzeige::prozent(w, 1); });
    zahl(tr("Offene Störungen"), Besser::Niedriger,
         [&d](const Anlage& a, const Kennzahl*) { return double(d.offeneStoerungen(a.anlageId)); },
         [](double w) { return QString::number(int(w)); });
    {
        Zeile z;
        z.titel = tr("Erkannte Fehlerbilder");
        z.besser = Besser::Niedriger;
        for (int i = 0; i < 2; ++i) {
            if (!kz[i]) {
                z.text[i] = QStringLiteral("–");
                continue;
            }
            QStringList namen;
            int anzahl = 0;
            for (const Anomalie& x : kz[i]->auswertung.anomalien) {
                if (x.istHinweis())
                    continue;
                ++anzahl;
                const QString n = Anomalie::typName(x.typ);
                if (!namen.contains(n))
                    namen << n;
            }
            z.wert[i] = anzahl;
            z.text[i] = namen.isEmpty() ? tr("keine") : namen.join(QStringLiteral(", "));
        }
        zeilen.append(z);
    }

    m_tabelle->setRowCount(int(zeilen.size()));
    const QColor gruen(22, 163, 74);
    for (int r = 0; r < zeilen.size(); ++r) {
        const Zeile& z = zeilen.at(r);
        auto* titel = new QTableWidgetItem(z.titel);
        titel->setForeground(Stil::textGedimmt());
        m_tabelle->setItem(r, 0, titel);
        int sieger = -1;
        if (z.besser != Besser::Keins && !std::isnan(z.wert[0]) && !std::isnan(z.wert[1])
            && std::abs(z.wert[0] - z.wert[1]) > 1e-9 && gewaehlt(0) != gewaehlt(1)) {
            const bool aBesser = z.besser == Besser::Hoeher ? z.wert[0] > z.wert[1] : z.wert[0] < z.wert[1];
            sieger = aBesser ? 0 : 1;
        }
        for (int i = 0; i < 2; ++i) {
            auto* it = new QTableWidgetItem(z.text[i]);
            it->setToolTip(z.text[i]);
            if (i == sieger) {
                it->setForeground(gruen);
                QFont f = it->font();
                f.setBold(true);
                it->setFont(f);
                it->setText(z.text[i] + QStringLiteral("  ▲"));
            }
            m_tabelle->setItem(r, i + 1, it);
        }
    }
}

// ===================================================================
//  Diagramm
// ===================================================================

TagesVergleich::TagesVergleich(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

QColor TagesVergleich::farbe(int seite)
{
    return seite == 0 ? QColor(37, 99, 235) : QColor(245, 158, 11);
}

void TagesVergleich::setDaten(const QString& nameA, const QMap<QDate, double>& a, const QString& nameB,
                              const QMap<QDate, double>& b)
{
    m_name[0] = nameA;
    m_name[1] = nameB;
    m_werte[0] = a;
    m_werte[1] = b;
    update();
}

QList<QDate> TagesVergleich::tage() const
{
    QList<QDate> alle = m_werte[0].keys();
    for (const QDate& t : m_werte[1].keys())
        if (!m_werte[0].contains(t))
            alle.append(t);
    std::sort(alle.begin(), alle.end());
    return alle;
}

QRectF TagesVergleich::plot() const
{
    return QRectF(56, 34, width() - 56 - 12, height() - 34 - 28);
}

void TagesVergleich::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor text = Stil::text(), gedimmt = Stil::textGedimmt();

    // Titel und Legende
    QFont fett = font();
    fett.setBold(true);
    p.setFont(fett);
    p.setPen(text);
    p.drawText(QRectF(0, 0, width(), 22), Qt::AlignLeft | Qt::AlignVCenter, tr("Spezifischer Tagesertrag (kWh/kWp)"));
    p.setFont(font());
    double x = width() - 8;
    for (int i = 1; i >= 0; --i) {
        if (m_name[i].isEmpty())
            continue;
        const double b = p.fontMetrics().horizontalAdvance(m_name[i]);
        x -= b;
        p.setPen(text);
        p.drawText(QRectF(x, 0, b, 22), Qt::AlignVCenter, m_name[i]);
        x -= 18;
        p.setPen(Qt::NoPen);
        p.setBrush(farbe(i));
        p.drawRoundedRect(QRectF(x, 6, 12, 12), 3, 3);
        x -= 14;
    }

    const QList<QDate> alle = tage();
    const QRectF r = plot();
    if (alle.isEmpty()) {
        p.setPen(gedimmt);
        p.drawText(r, Qt::AlignCenter, tr("Keine Tageswerte vorhanden."));
        return;
    }
    double maxWert = 0.1;
    for (int i = 0; i < 2; ++i)
        for (double w : m_werte[i])
            maxWert = std::max(maxWert, w);
    // runde Achsenschritte
    double schritt = 1.0;
    for (double s : {0.5, 1.0, 2.0, 2.5, 5.0, 10.0})
        if (maxWert / s <= 5.0) {
            schritt = s;
            break;
        }
    const double oben = std::ceil(maxWert / schritt) * schritt;

    p.setPen(QPen(Stil::istDunkel() ? QColor(70, 80, 100) : QColor(226, 232, 240), 1));
    for (double w = 0; w <= oben + 1e-9; w += schritt) {
        const double y = r.bottom() - w / oben * r.height();
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        p.save();
        p.setPen(gedimmt);
        p.drawText(QRectF(0, y - 9, r.left() - 6, 18), Qt::AlignRight | Qt::AlignVCenter, Anzeige::zahl(w, 1));
        p.restore();
    }

    const double gruppe = r.width() / alle.size();
    const double balken = std::max(2.0, std::min(18.0, gruppe * 0.36));
    const int beschriftungAlle = std::max(1, int(std::ceil(46.0 / gruppe)));
    for (int t = 0; t < alle.size(); ++t) {
        const double mitte = r.left() + gruppe * (t + 0.5);
        for (int i = 0; i < 2; ++i) {
            if (!m_werte[i].contains(alle.at(t)))
                continue;
            const double w = m_werte[i].value(alle.at(t));
            const double h = w / oben * r.height();
            p.setPen(Qt::NoPen);
            p.setBrush(farbe(i));
            p.drawRoundedRect(QRectF(mitte - balken - 1 + i * (balken + 2), r.bottom() - h, balken, h), 2, 2);
        }
        if (t % beschriftungAlle == 0) {
            p.setPen(gedimmt);
            p.drawText(QRectF(mitte - 30, r.bottom() + 4, 60, 18), Qt::AlignCenter,
                       alle.at(t).toString(QStringLiteral("dd.MM.")));
        }
    }
}

void TagesVergleich::mouseMoveEvent(QMouseEvent* e)
{
    const QList<QDate> alle = tage();
    const QRectF r = plot();
    if (alle.isEmpty() || !r.contains(e->position())) {
        QToolTip::hideText();
        return;
    }
    const int t = qBound(0, int((e->position().x() - r.left()) / (r.width() / alle.size())), int(alle.size()) - 1);
    const QDate tag = alle.at(t);
    QStringList zeilen{Anzeige::datum(tag)};
    for (int i = 0; i < 2; ++i)
        if (m_werte[i].contains(tag))
            zeilen << QStringLiteral("%1: %2 kWh/kWp").arg(m_name[i], Anzeige::zahl(m_werte[i].value(tag), 2));
    QToolTip::showText(e->globalPosition().toPoint(), zeilen.join(QLatin1Char('\n')), this);
}
