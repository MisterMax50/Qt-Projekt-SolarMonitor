#include "benutzer_seite.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "model/benutzer.h"
#include "util/anzeige.h"
#include "util/stil.h"

namespace {
constexpr int kKennungRolle = Qt::UserRole + 1;

QLineEdit* pinFeld(QWidget* eltern)
{
    auto* f = new QLineEdit(eltern);
    f->setEchoMode(QLineEdit::Password);
    f->setMaxLength(8);
    f->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]{0,8}")), f));
    return f;
}
} // namespace

// ===================================================================
// Formular
// ===================================================================

BenutzerDialog::BenutzerDialog(Benutzerverwaltung* verwaltung, const QString& kennung, const QString& angemeldet,
                               QWidget* parent)
    : QDialog(parent)
    , m_verwaltung(verwaltung)
    , m_kennung(kennung)
    , m_angemeldet(angemeldet)
{
    const Benutzer* b = kennung.isEmpty() ? nullptr : verwaltung->finde(kennung);
    setWindowTitle(b ? tr("Benutzer bearbeiten – %1").arg(b->name) : tr("Neuer Benutzer"));
    setMinimumWidth(460);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("z. B. Max Mustermann"));
    m_benutzername = new QLineEdit(this);
    m_benutzername->setPlaceholderText(tr("z. B. mmustermann"));
    m_benutzername->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9._-]{0,32}")), m_benutzername));
    m_rolle = new QComboBox(this);
    for (Rolle r : {Rolle::Betrieb, Rolle::Service, Rolle::Admin})
        m_rolle->addItem(Rollen::anzeigename(r), int(r));
    m_rolle->setToolTip(tr("Betrieb: ansehen und auswerten\nService: zusätzlich Störungen erfassen und bearbeiten\n"
                           "Administrator: zusätzlich Stammdaten und Benutzer verwalten"));
    m_aktiv = new QCheckBox(tr("Konto aktiv (Anmeldung erlaubt)"), this);
    m_aktiv->setChecked(true);
    m_hello = new QCheckBox(tr("Anmeldung mit Windows Hello"), this);
    m_hello->setEnabled(false); // einrichten kann nur der Benutzer selbst (Benutzermenü)
    m_hello->setToolTip(tr("Einrichten kann jeder Benutzer selbst im Benutzermenü oben rechts. "
                           "Hier lässt sich Windows Hello für ein Konto abschalten."));
    m_pin = pinFeld(this);
    m_pin2 = pinFeld(this);
    m_fehler = new QLabel(this);
    m_fehler->setWordWrap(true);
    m_fehler->setStyleSheet(QStringLiteral("color: #dc2626;"));
    m_fehler->hide();

    auto* form = new QFormLayout;
    form->setVerticalSpacing(10);
    form->addRow(tr("Name:"), m_name);
    form->addRow(tr("Benutzername:"), m_benutzername);
    form->addRow(tr("Rolle:"), m_rolle);
    form->addRow(QString(), m_aktiv);
    form->addRow(QString(), m_hello);
    form->addRow(b ? tr("Neue PIN:") : tr("PIN:"), m_pin);
    form->addRow(tr("PIN wiederholen:"), m_pin2);

    auto* hinweis = new QLabel(b ? tr("PIN-Felder leer lassen, um die PIN nicht zu ändern. PIN: 4 bis 8 Ziffern.")
                                 : tr("PIN: 4 bis 8 Ziffern. Sie wird nur verschlüsselt (gehasht) gespeichert."),
                               this);
    hinweis->setObjectName(QStringLiteral("Gedimmt"));
    hinweis->setWordWrap(true);

    auto* knoepfe = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(knoepfe, &QDialogButtonBox::accepted, this, &BenutzerDialog::speichern);
    connect(knoepfe, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(24, 20, 24, 16);
    v->setSpacing(12);
    v->addLayout(form);
    v->addWidget(hinweis);
    v->addWidget(m_fehler);
    v->addWidget(knoepfe);

    if (b) {
        m_name->setText(b->name);
        m_benutzername->setText(b->kennung);
        m_benutzername->setEnabled(false); // Benutzername bleibt fest
        m_rolle->setCurrentIndex(m_rolle->findData(int(b->rolle)));
        m_aktiv->setChecked(b->aktiv);
        m_hello->setChecked(!b->helloKonto.isEmpty());
        m_hello->setEnabled(!b->helloKonto.isEmpty()); // nur abschalten möglich
        if (!b->helloKonto.isEmpty())
            m_hello->setText(tr("Anmeldung mit Windows Hello (%1)").arg(b->helloKonto));
        if (b->kennung == angemeldet) {
            // eigenes Konto: Rolle und Sperre nicht änderbar
            m_rolle->setEnabled(false);
            m_aktiv->setEnabled(false);
            m_rolle->setToolTip(tr("Die eigene Rolle kann nicht geändert werden."));
        }
    } else {
        m_rolle->setCurrentIndex(0);
    }
    m_name->setFocus();
}

void BenutzerDialog::speichern()
{
    auto zeige = [this](const QString& text) {
        m_fehler->setText(text);
        m_fehler->setVisible(!text.isEmpty());
    };
    const QString pin = m_pin->text(), pin2 = m_pin2->text();
    if (pin != pin2) {
        zeige(tr("Die beiden PINs stimmen nicht überein."));
        m_pin2->setFocus();
        return;
    }
    const auto rolle = static_cast<Rolle>(m_rolle->currentData().toInt());
    QString fehler;
    if (m_kennung.isEmpty()) {
        fehler = m_verwaltung->anlegen(m_benutzername->text(), m_name->text(), rolle, pin);
    } else {
        if (!pin.isEmpty() && !Benutzerverwaltung::pinGueltig(pin))
            fehler = tr("Die PIN muss aus 4 bis 8 Ziffern bestehen.");
        if (fehler.isEmpty())
            fehler = m_verwaltung->aendern(m_kennung, m_name->text(), rolle, m_aktiv->isChecked(), m_angemeldet);
        if (fehler.isEmpty() && !pin.isEmpty())
            fehler = m_verwaltung->setzePin(m_kennung, pin);
        const Benutzer* b = m_verwaltung->finde(m_kennung);
        if (fehler.isEmpty() && b && !b->helloKonto.isEmpty() && !m_hello->isChecked())
            fehler = m_verwaltung->setzeHello(m_kennung, QString());
    }
    if (!fehler.isEmpty()) {
        zeige(fehler);
        return;
    }
    accept();
}

// ===================================================================
// Seite
// ===================================================================

BenutzerSeite::BenutzerSeite(Benutzerverwaltung* verwaltung, const QString& angemeldet, QWidget* parent)
    : QWidget(parent)
    , m_verwaltung(verwaltung)
    , m_angemeldet(angemeldet)
{
    const QColor f = Stil::text();
    m_neu = new QAction(Stil::icon(Stil::Symbol::Plus, f), tr("Neuer Benutzer …"), this);
    m_bearbeiten = new QAction(Stil::icon(Stil::Symbol::Bearbeiten, f), tr("Bearbeiten …"), this);
    m_pin = new QAction(Stil::icon(Stil::Symbol::Benutzer, f), tr("PIN zurücksetzen …"), this);
    m_loeschen = new QAction(Stil::icon(Stil::Symbol::Fehler, f), tr("Löschen"), this);
    connect(m_neu, &QAction::triggered, this, &BenutzerSeite::neu);
    connect(m_bearbeiten, &QAction::triggered, this, &BenutzerSeite::bearbeiten);
    connect(m_pin, &QAction::triggered, this, &BenutzerSeite::pinZuruecksetzen);
    connect(m_loeschen, &QAction::triggered, this, &BenutzerSeite::loeschen);

    m_liste = new QTreeWidget(this);
    m_liste->setRootIsDecorated(false);
    m_liste->setUniformRowHeights(true);
    m_liste->setAlternatingRowColors(false);
    m_liste->setHeaderLabels({tr("Name"), tr("Benutzername"), tr("Rolle"), tr("Status"), tr("Windows Hello"),
                              tr("Letzte Anmeldung"), tr("Angelegt")});
    m_liste->header()->setStretchLastSection(false);
    m_liste->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < 7; ++i)
        m_liste->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    m_liste->setSortingEnabled(true);
    m_liste->sortByColumn(0, Qt::AscendingOrder);
    m_liste->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_liste->addActions({m_bearbeiten, m_pin, m_loeschen});
    connect(m_liste, &QTreeWidget::itemDoubleClicked, this, &BenutzerSeite::bearbeiten);
    connect(m_liste, &QTreeWidget::itemSelectionChanged, this, &BenutzerSeite::auswahlGeaendert);

    m_info = new QLabel(this);
    m_info->setObjectName(QStringLiteral("Gedimmt"));
    m_info->setWordWrap(true);

    auto* v = new QVBoxLayout(this);
    v->setSpacing(10);
    v->addWidget(m_liste, 1);
    v->addWidget(m_info);

    connect(m_verwaltung, &Benutzerverwaltung::geaendert, this, &BenutzerSeite::aktualisiere);
    aktualisiere();
}

QString BenutzerSeite::markiert() const
{
    const QTreeWidgetItem* it = m_liste->currentItem();
    return it && it->isSelected() ? it->data(0, kKennungRolle).toString() : QString();
}

void BenutzerSeite::aktualisiere()
{
    const QString vorher = markiert();
    m_liste->setSortingEnabled(false);
    m_liste->clear();
    int aktiv = 0;
    for (const Benutzer& b : m_verwaltung->alle()) {
        auto* it = new QTreeWidgetItem(m_liste);
        const bool ich = b.kennung == m_angemeldet;
        it->setText(0, ich ? tr("%1 (Sie)").arg(b.name) : b.name);
        it->setText(1, b.kennung);
        it->setText(2, Rollen::anzeigename(b.rolle));
        it->setText(3, b.aktiv ? tr("aktiv") : tr("gesperrt"));
        it->setIcon(3, Stil::icon(b.aktiv ? Stil::Symbol::Haken : Stil::Symbol::Fehler,
                                  b.aktiv ? QColor(22, 163, 74) : QColor(220, 38, 38)));
        it->setText(4, b.helloKonto.isEmpty() ? QStringLiteral("–") : tr("eingerichtet"));
        if (!b.helloKonto.isEmpty()) {
            it->setIcon(4, Stil::icon(Stil::Symbol::Fingerabdruck, Stil::text()));
            it->setToolTip(4, tr("Verknüpft mit dem Windows-Konto %1").arg(b.helloKonto));
        }
        it->setText(5, b.letzteAnmeldung.isValid() ? Anzeige::zeitpunkt(b.letzteAnmeldung) : tr("noch nie"));
        it->setText(6, b.erstellt.isValid() ? Anzeige::datum(b.erstellt.date()) : QString());
        it->setData(0, kKennungRolle, b.kennung);
        it->setSizeHint(0, QSize(0, 32)); // Zeilenhöhe wie in den anderen Tabellen
        if (ich) {
            QFont f = it->font(0);
            f.setBold(true);
            it->setFont(0, f);
        }
        if (b.aktiv)
            ++aktiv;
        if (b.kennung == vorher)
            it->setSelected(true), m_liste->setCurrentItem(it);
    }
    m_liste->setSortingEnabled(true);
    m_info->setText(tr("%1 Benutzer, davon %2 aktiv · gespeichert in %3")
                        .arg(m_verwaltung->alle().size())
                        .arg(aktiv)
                        .arg(QDir::toNativeSeparators(m_verwaltung->datei())));
    auswahlGeaendert();
}

void BenutzerSeite::auswahlGeaendert()
{
    const QString k = markiert();
    m_bearbeiten->setEnabled(!k.isEmpty());
    m_pin->setEnabled(!k.isEmpty());
    m_loeschen->setEnabled(!k.isEmpty() && k != m_angemeldet);
    m_loeschen->setToolTip(k == m_angemeldet ? tr("Das eigene Konto kann nicht gelöscht werden.") : QString());
}

void BenutzerSeite::neu()
{
    BenutzerDialog d(m_verwaltung, QString(), m_angemeldet, this);
    if (d.exec() == QDialog::Accepted)
        emit meldung(tr("Benutzer angelegt."), false);
}

void BenutzerSeite::bearbeiten()
{
    const QString k = markiert();
    if (k.isEmpty()) {
        emit meldung(tr("Bitte zuerst einen Benutzer in der Liste auswählen."), true);
        return;
    }
    BenutzerDialog d(m_verwaltung, k, m_angemeldet, this);
    if (d.exec() == QDialog::Accepted)
        emit meldung(tr("Änderungen gespeichert."), false);
}

void BenutzerSeite::pinZuruecksetzen()
{
    const QString k = markiert();
    const Benutzer* b = m_verwaltung->finde(k);
    if (!b) {
        emit meldung(tr("Bitte zuerst einen Benutzer in der Liste auswählen."), true);
        return;
    }
    bool ok = false;
    const QString pin = QInputDialog::getText(this, tr("PIN zurücksetzen"),
                                              tr("Neue PIN für %1 (4 bis 8 Ziffern):").arg(b->name),
                                              QLineEdit::Password, QString(), &ok);
    if (!ok)
        return;
    const QString fehler = m_verwaltung->setzePin(k, pin);
    if (fehler.isEmpty())
        emit meldung(tr("PIN für %1 geändert.").arg(b->name), false);
    else
        QMessageBox::warning(this, tr("PIN zurücksetzen"), fehler);
}

void BenutzerSeite::loeschen()
{
    const QString k = markiert();
    const Benutzer* b = m_verwaltung->finde(k);
    if (!b) {
        emit meldung(tr("Bitte zuerst einen Benutzer in der Liste auswählen."), true);
        return;
    }
    const QString name = b->name;
    if (QMessageBox::question(this, tr("Benutzer löschen"),
                              tr("Soll der Benutzer „%1“ (%2) wirklich gelöscht werden?\n"
                                 "Tipp: Statt zu löschen kann das Konto auch gesperrt werden.")
                                  .arg(name, k))
        != QMessageBox::Yes)
        return;
    const QString fehler = m_verwaltung->loeschen(k, m_angemeldet);
    if (fehler.isEmpty())
        emit meldung(tr("Benutzer „%1“ gelöscht.").arg(name), false);
    else
        QMessageBox::warning(this, tr("Benutzer löschen"), fehler);
}
