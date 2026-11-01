#include "stoerung_dialog.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include "model/datenbasis.h"
#include "util/anzeige.h"
#include "util/zeit.h"

StoerungDialog::StoerungDialog(const Stoerung& stoerung, const Datenbasis* daten, bool bearbeitbar,
                               QWidget* parent)
    : QDialog(parent)
    , m_original(stoerung)
    , m_daten(daten)
{
    setWindowTitle(tr("Störung %1").arg(stoerung.stoerungId));
    baueLayout(false, bearbeitbar);
}

StoerungDialog::StoerungDialog(const QString& anlageId, const Datenbasis* daten, QWidget* parent)
    : QDialog(parent)
    , m_daten(daten)
{
    m_original.anlageId = anlageId;
    m_original.zeitpunkt = Zeit::jetzt();
    m_original.status = QStringLiteral("offen");
    m_original.schwere = QStringLiteral("warnung");
    setWindowTitle(tr("Störung erfassen – %1").arg(anlageId));
    baueLayout(true, true);
}

void StoerungDialog::baueLayout(bool neu, bool bearbeitbar)
{
    const Anlage* a = m_daten->anlage(m_original.anlageId);
    auto* anlageText = new QLabel(a ? QStringLiteral("<b>%1</b> – %2, %3 (%4 kWp)")
                                          .arg(a->anlageId, a->ort, Anzeige::typName(a->typ),
                                               Anzeige::zahl(a->leistungKwp, 2))
                                    : tr("<b>%1</b> (in anlagen.csv nicht vorhanden)").arg(m_original.anlageId),
                                  this);

    // QDateTimeEdit arbeitet in Ortszeit; Datum und Uhrzeit werden 1:1 übernommen.
    m_zeitpunkt = new QDateTimeEdit(this);
    m_zeitpunkt->setDate(m_original.zeitpunkt.date());
    m_zeitpunkt->setTime(m_original.zeitpunkt.time());
    m_zeitpunkt->setDisplayFormat(QStringLiteral("dd.MM.yyyy hh:mm"));
    m_zeitpunkt->setCalendarPopup(true);
    m_zeitpunkt->setEnabled(neu);

    m_code = new QComboBox(this);
    m_code->setEditable(true);
    const QHash<QString, QString> codes = m_daten->fehlercodes();
    QStringList sortiert = codes.keys();
    sortiert.sort();
    for (const QString& c : sortiert)
        m_code->addItem(QStringLiteral("%1 – %2").arg(c, codes.value(c)), c);
    if (neu)
        m_code->setCurrentIndex(-1);
    else
        m_code->setCurrentText(m_original.fehlercode);
    m_code->setEnabled(neu);
    connect(m_code, &QComboBox::activated, this, &StoerungDialog::fehlercodeGewaehlt);

    m_beschreibung = new QLineEdit(m_original.beschreibung, this);
    m_beschreibung->setEnabled(neu);

    m_schwere = new QComboBox(this);
    for (const QString& s : Datenbasis::gueltigeSchweren())
        m_schwere->addItem(Anzeige::schwereName(s), s);
    const int si = m_schwere->findData(m_original.schwere);
    if (si >= 0) {
        m_schwere->setCurrentIndex(si);
    } else {
        m_schwere->addItem(tr("%1 (ungültig)").arg(m_original.schwere), m_original.schwere);
        m_schwere->setCurrentIndex(m_schwere->count() - 1);
    }
    m_schwere->setEnabled(bearbeitbar);

    m_status = new QComboBox(this);
    for (const QString& s : Datenbasis::gueltigeStoerungsStatus())
        m_status->addItem(Anzeige::stoerungsStatusName(s), s);
    m_status->setCurrentIndex(qMax(0, m_status->findData(m_original.status)));
    m_status->setEnabled(bearbeitbar);

    m_behobenAm = new QLabel(m_original.behobenAm.isValid() ? Anzeige::zeitpunkt(m_original.behobenAm)
                                                            : QStringLiteral("–"),
                             this);

    auto* form = new QFormLayout;
    form->addRow(tr("Anlage:"), anlageText);
    if (!neu)
        form->addRow(tr("Störung:"), new QLabel(m_original.stoerungId, this));
    form->addRow(tr("Zeitpunkt:"), m_zeitpunkt);
    form->addRow(tr("Fehlercode:"), m_code);
    form->addRow(tr("Beschreibung:"), m_beschreibung);
    form->addRow(tr("Schwere:"), m_schwere);
    form->addRow(tr("Status:"), m_status);
    form->addRow(tr("Behoben am:"), m_behobenAm);

    auto* v = new QVBoxLayout(this);
    v->addLayout(form);
    if (!bearbeitbar) {
        auto* nurLesen = new QLabel(tr("Nur Lesen – Störungen dürfen nur die Rollen Service und Administrator bearbeiten."), this);
        nurLesen->setWordWrap(true);
        nurLesen->setObjectName(QStringLiteral("Gedimmt"));
        v->addWidget(nurLesen);
    }

    auto* knoepfe = new QDialogButtonBox(bearbeitbar ? (QDialogButtonBox::Save | QDialogButtonBox::Cancel)
                                                     : QDialogButtonBox::Close,
                                         this);
    connect(knoepfe, &QDialogButtonBox::accepted, this, &StoerungDialog::uebernehmen);
    connect(knoepfe, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(knoepfe);
    resize(520, sizeHint().height());
}

void StoerungDialog::fehlercodeGewaehlt()
{
    const QString code = m_code->currentData().toString();
    const QString beschreibung = m_daten->fehlercodes().value(code);
    if (!beschreibung.isEmpty())
        m_beschreibung->setText(beschreibung);
}

Stoerung StoerungDialog::stoerung() const
{
    Stoerung s = m_original;
    s.zeitpunkt = Zeit::naiv(m_zeitpunkt->date(), m_zeitpunkt->time());
    const QString code = m_code->currentData().toString();
    s.fehlercode = (!code.isEmpty() && m_code->currentText().startsWith(code)) ? code
                                                                               : m_code->currentText().trimmed();
    s.beschreibung = m_beschreibung->text().trimmed();
    s.schwere = m_schwere->currentData().toString();
    s.status = m_status->currentData().toString();
    if (s.status == QLatin1String("behoben") && !s.behobenAm.isValid())
        s.behobenAm = Zeit::jetzt();
    if (s.status != QLatin1String("behoben"))
        s.behobenAm = QDateTime();
    return s;
}

void StoerungDialog::uebernehmen()
{
    const Stoerung s = stoerung();
    if (s.fehlercode.isEmpty() || s.beschreibung.isEmpty()) {
        QMessageBox::warning(this, tr("Eingabe prüfen"), tr("Bitte Fehlercode und Beschreibung angeben."));
        return;
    }
    accept();
}
