#include "pruefprotokoll_widget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include "model/datenbasis.h"
#include "ui/tabellen_hilfe.h"
#include "util/stil.h"

PruefprotokollWidget::PruefprotokollWidget(const AppKontext& kontext, QWidget* parent)
    : QWidget(parent)
    , m_kontext(kontext)
    , m_iconFehler(Stil::icon(Stil::Symbol::Fehler, QColor(220, 38, 38)))
    , m_iconWarnung(Stil::icon(Stil::Symbol::Warnung, QColor(217, 119, 6)))
    , m_iconInfo(Stil::icon(Stil::Symbol::Info, QColor(37, 99, 235)))
{
    m_stufe = new QComboBox(this);
    m_stufe->addItem(tr("Alle Stufen"), -1);
    m_stufe->addItem(tr("Fehler"), int(Pruefmeldung::Stufe::Fehler));
    m_stufe->addItem(tr("Warnungen"), int(Pruefmeldung::Stufe::Warnung));
    m_stufe->addItem(tr("Korrekturen/Infos"), int(Pruefmeldung::Stufe::Info));
    m_suche = new QLineEdit(this);
    m_suche->setPlaceholderText(tr("Suche in Datei, Objekt oder Meldung …"));
    m_suche->setClearButtonEnabled(true);
    m_zusammenfassung = new QLabel(this);

    m_tabelle = new QTableWidget(this);
    TabellenHilfe::einrichten(m_tabelle, {tr("Stufe"), tr("Datei"), tr("Zeile"), tr("Objekt"), tr("Meldung")});
    m_tabelle->setSortingEnabled(true);
    m_tabelle->setShowGrid(false);
    m_tabelle->verticalHeader()->setDefaultSectionSize(28);
    m_tabelle->setToolTip(tr("Doppelklick auf eine Anlagen-ID öffnet die Anlage."));
    connect(m_tabelle, &QTableWidget::cellDoubleClicked, this, [this](int zeile, int) {
        const QString id = m_tabelle->item(zeile, 3)->text();
        if (m_kontext.daten->anlage(id))
            emit anlageOeffnen(id);
    });

    auto* leiste = new QHBoxLayout;
    leiste->addWidget(m_stufe);
    leiste->addWidget(m_suche, 1);
    leiste->addWidget(m_zusammenfassung);

    auto* v = new QVBoxLayout(this);
    v->addLayout(leiste);
    v->addWidget(m_tabelle, 1);

    connect(m_stufe, &QComboBox::currentIndexChanged, this, &PruefprotokollWidget::aktualisiere);
    connect(m_suche, &QLineEdit::textChanged, this, &PruefprotokollWidget::aktualisiere);
    connect(m_kontext.daten, &Datenbasis::meldungenGeaendert, this, &PruefprotokollWidget::aktualisiere);
    aktualisiere();
}

void PruefprotokollWidget::aktualisiere()
{
    const int stufe = m_stufe->currentData().toInt();
    const QString suche = m_suche->text().trimmed();
    const auto& alle = m_kontext.daten->pruefmeldungen();

    m_tabelle->setSortingEnabled(false);
    m_tabelle->setRowCount(0);
    for (const Pruefmeldung& m : alle) {
        if (stufe >= 0 && int(m.stufe) != stufe)
            continue;
        if (!suche.isEmpty() && !m.text.contains(suche, Qt::CaseInsensitive)
            && !m.datei.contains(suche, Qt::CaseInsensitive) && !m.objektId.contains(suche, Qt::CaseInsensitive))
            continue;

        const int r = m_tabelle->rowCount();
        m_tabelle->insertRow(r);
        QIcon icon = m_iconWarnung;
        QString name = tr("Warnung");
        if (m.stufe == Pruefmeldung::Stufe::Fehler) {
            icon = m_iconFehler;
            name = tr("Fehler");
        } else if (m.stufe == Pruefmeldung::Stufe::Info) {
            icon = m_iconInfo;
            name = tr("Info");
        }
        auto* st = new ZahlItem(name, -int(m.stufe));
        st->setIcon(icon);
        st->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_tabelle->setItem(r, 0, st);
        m_tabelle->setItem(r, 1, TabellenHilfe::text(m.datei));
        m_tabelle->setItem(r, 2, new ZahlItem(m.zeile > 0 ? QString::number(m.zeile) : QString(), m.zeile));
        m_tabelle->setItem(r, 3, TabellenHilfe::text(m.objektId));
        m_tabelle->setItem(r, 4, TabellenHilfe::text(m.text));
    }
    m_tabelle->setSortingEnabled(true);
    m_tabelle->resizeColumnsToContents();

    m_zusammenfassung->setText(tr("%1 Fehler · %2 Warnungen · %3 Korrekturen/Infos")
                                   .arg(m_kontext.daten->anzahlMeldungen(Pruefmeldung::Stufe::Fehler))
                                   .arg(m_kontext.daten->anzahlMeldungen(Pruefmeldung::Stufe::Warnung))
                                   .arg(m_kontext.daten->anzahlMeldungen(Pruefmeldung::Stufe::Info)));
}
