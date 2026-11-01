#include "edit_dialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "model/datenbasis.h"
#include "util/anzeige.h"

namespace {
/// Combo mit Rohwerten als Daten; unbekannter Rohwert wird ergänzt und markiert.
void waehle(QComboBox* combo, const QString& roh)
{
    int i = combo->findData(roh);
    if (i < 0) {
        combo->addItem(EditDialog::tr("%1 (ungültig)").arg(roh.isEmpty() ? QStringLiteral("–") : roh), roh);
        i = combo->count() - 1;
    }
    combo->setCurrentIndex(i);
}
} // namespace

EditDialog::EditDialog(const Anlage& anlage, const Datenbasis* datenbasis, QWidget* parent)
    : QDialog(parent)
    , m_original(anlage)
    , m_daten(datenbasis)
{
    setWindowTitle(tr("Stammdaten bearbeiten – %1").arg(anlage.anlageId));

    m_typ = new QComboBox(this);
    for (const QString& t : Datenbasis::gueltigeTypen())
        m_typ->addItem(Anzeige::typName(t), t);
    m_ort = new QLineEdit(this);
    m_lat = new QDoubleSpinBox(this);
    m_lat->setRange(-90.0, 90.0);
    m_lat->setDecimals(5);
    m_lon = new QDoubleSpinBox(this);
    m_lon->setRange(-180.0, 180.0);
    m_lon->setDecimals(5);
    m_leistung = new QDoubleSpinBox(this);
    m_leistung->setRange(0.0, 100000.0);
    m_leistung->setDecimals(2);
    m_leistung->setSuffix(QStringLiteral(" kWp"));
    m_ausrichtung = new QSpinBox(this);
    m_ausrichtung->setRange(0, 359);
    m_ausrichtung->setSuffix(QStringLiteral("°"));
    m_ausrichtung->setToolTip(tr("90° = Ost, 180° = Süd, 270° = West"));
    m_neigung = new QSpinBox(this);
    m_neigung->setRange(0, 90);
    m_neigung->setSuffix(QStringLiteral("°"));

    m_wechselrichter = new QComboBox(this);
    for (const Wechselrichter& w : m_daten->wechselrichterListe())
        m_wechselrichter->addItem(QStringLiteral("%1 – %2 (%3 kW)")
                                      .arg(w.wechselrichterId, w.bezeichnung(), Anzeige::zahl(w.nennleistungKw, 1)),
                                  w.wechselrichterId);
    m_anzahlWr = new QSpinBox(this);
    m_anzahlWr->setRange(1, 500);

    m_station = new QComboBox(this);
    for (const Wetterstation& w : m_daten->wetterstationen())
        m_station->addItem(QStringLiteral("%1 – %2").arg(w.stationId, w.name), w.stationId);

    m_inbetrieb = new QDateEdit(this);
    m_inbetrieb->setCalendarPopup(true);
    m_inbetrieb->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_inbetrieb->setDateRange(QDate(1990, 1, 1), QDate::currentDate());
    m_verguetung = new QDoubleSpinBox(this);
    m_verguetung->setRange(0.0, 100.0);
    m_verguetung->setDecimals(2);
    m_verguetung->setSuffix(QStringLiteral(" ct/kWh"));
    m_status = new QComboBox(this);
    for (const QString& s : Datenbasis::gueltigeStatus())
        m_status->addItem(Anzeige::statusName(s), s);

    // Werte übernehmen
    waehle(m_typ, anlage.typ);
    m_ort->setText(anlage.ort);
    m_lat->setValue(anlage.lat);
    m_lon->setValue(anlage.lon);
    m_leistung->setValue(anlage.leistungKwp);
    m_ausrichtung->setValue(qBound(0, anlage.ausrichtungGrad, 359));
    m_neigung->setValue(qBound(0, anlage.neigungGrad, 90));
    waehle(m_wechselrichter, anlage.wechselrichterId);
    m_anzahlWr->setValue(qMax(1, anlage.anzahlWr));
    waehle(m_station, anlage.wetterstationId);
    m_inbetrieb->setDate(anlage.inbetriebnahme.isValid() ? qMin(anlage.inbetriebnahme, QDate::currentDate())
                                                         : QDate::currentDate());
    m_verguetung->setValue(anlage.verguetungCtKwh);
    waehle(m_status, anlage.status);

    auto* form = new QFormLayout;
    form->addRow(tr("Anlage:"), new QLabel(QStringLiteral("<b>%1</b>").arg(anlage.anlageId), this));
    form->addRow(tr("Typ:"), m_typ);
    form->addRow(tr("Ort:"), m_ort);
    form->addRow(tr("Breitengrad:"), m_lat);
    form->addRow(tr("Längengrad:"), m_lon);
    form->addRow(tr("Leistung:"), m_leistung);
    form->addRow(tr("Ausrichtung:"), m_ausrichtung);
    form->addRow(tr("Neigung:"), m_neigung);
    form->addRow(tr("Wechselrichter:"), m_wechselrichter);
    form->addRow(tr("Anzahl WR:"), m_anzahlWr);
    form->addRow(tr("Wetterstation:"), m_station);
    form->addRow(tr("Inbetriebnahme:"), m_inbetrieb);
    form->addRow(tr("Vergütung:"), m_verguetung);
    form->addRow(tr("Status:"), m_status);

    m_hinweise = new QLabel(this);
    m_hinweise->setWordWrap(true);
    m_hinweise->setStyleSheet(QStringLiteral("color: #b9770e;"));

    // Werte, die im Formular nicht darstellbar sind (z. B. Ausrichtung 450°),
    // werden begrenzt; der ursprüngliche Befund bleibt sichtbar.
    QStringList original;
    for (const Pruefmeldung& m : m_daten->pruefmeldungenFuer(anlage.anlageId))
        if (m.datei == QLatin1String("anlagen.csv"))
            original.append(QStringLiteral("• ") + m.text);
    auto* originalLabel = new QLabel(tr("Befund in den Ausgangsdaten:\n%1").arg(original.join(QLatin1Char('\n'))), this);
    originalLabel->setWordWrap(true);
    originalLabel->setObjectName(QStringLiteral("Gedimmt"));
    originalLabel->setVisible(!original.isEmpty());

    auto* knoepfe = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(knoepfe, &QDialogButtonBox::accepted, this, &EditDialog::uebernehmen);
    connect(knoepfe, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* v = new QVBoxLayout(this);
    v->addLayout(form);
    v->addWidget(originalLabel);
    v->addWidget(m_hinweise);
    v->addWidget(knoepfe);

    // Live-Prüfung bei jeder Änderung
    for (auto* c : {m_typ, m_wechselrichter, m_station, m_status})
        connect(c, &QComboBox::currentIndexChanged, this, &EditDialog::pruefen);
    for (auto* s : {m_lat, m_lon, m_leistung, m_verguetung})
        connect(s, &QDoubleSpinBox::valueChanged, this, &EditDialog::pruefen);
    for (auto* s : {m_ausrichtung, m_neigung, m_anzahlWr})
        connect(s, &QSpinBox::valueChanged, this, &EditDialog::pruefen);
    connect(m_ort, &QLineEdit::textChanged, this, &EditDialog::pruefen);
    pruefen();
}

Anlage EditDialog::anlage() const
{
    Anlage a = m_original;
    a.typ = m_typ->currentData().toString();
    a.ort = m_ort->text().trimmed();
    a.lat = m_lat->value();
    a.lon = m_lon->value();
    a.leistungKwp = m_leistung->value();
    a.ausrichtungGrad = m_ausrichtung->value();
    a.neigungGrad = m_neigung->value();
    a.wechselrichterId = m_wechselrichter->currentData().toString();
    a.anzahlWr = m_anzahlWr->value();
    a.wetterstationId = m_station->currentData().toString();
    a.inbetriebnahme = m_inbetrieb->date();
    a.verguetungCtKwh = m_verguetung->value();
    a.status = m_status->currentData().toString();
    return a;
}

void EditDialog::pruefen()
{
    QStringList zeilen;
    for (const Pruefmeldung& m : m_daten->pruefeAnlage(anlage()))
        zeilen.append(QStringLiteral("⚠ ") + m.text);
    m_hinweise->setText(zeilen.join(QLatin1Char('\n')));
    m_hinweise->setVisible(!zeilen.isEmpty());
}

void EditDialog::uebernehmen()
{
    const Anlage a = anlage();
    QString fehler;
    if (a.ort.isEmpty())
        fehler = tr("Der Ort darf nicht leer sein.");
    else if (a.leistungKwp <= 0.0)
        fehler = tr("Die Leistung muss größer als 0 kWp sein.");
    else if (!m_daten->wechselrichter(a.wechselrichterId))
        fehler = tr("Bitte einen vorhandenen Wechselrichter wählen.");
    else if (!m_daten->wetterstation(a.wetterstationId))
        fehler = tr("Bitte eine vorhandene Wetterstation wählen.");
    else if (!Datenbasis::gueltigeTypen().contains(a.typ) || !Datenbasis::gueltigeStatus().contains(a.status))
        fehler = tr("Bitte einen gültigen Typ und Status wählen.");

    if (!fehler.isEmpty()) {
        QMessageBox::warning(this, tr("Eingabe prüfen"), fehler);
        return;
    }
    accept();
}
