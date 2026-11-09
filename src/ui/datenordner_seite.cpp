#include "datenordner_seite.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "util/stil.h"

DatenordnerSeite::DatenordnerSeite(QWidget* parent)
    : QWidget(parent)
{
    // Gleicher Aufbau wie die Anmeldung: Karte auf dunkelblauem Grund
    setObjectName(QStringLiteral("Anmeldeseite"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* karte = new QFrame(this);
    karte->setObjectName(QStringLiteral("Anmeldekarte"));
    karte->setFixedWidth(720);
    auto* schatten = new QGraphicsDropShadowEffect(karte);
    schatten->setBlurRadius(60);
    schatten->setOffset(0, 12);
    schatten->setColor(QColor(0, 0, 0, 140));
    karte->setGraphicsEffect(schatten);

    auto* symbol = new QLabel(karte);
    symbol->setPixmap(Stil::appIcon().pixmap(44, 44));
    m_titel = new QLabel(tr("Datenordner festlegen"), karte);
    m_titel->setObjectName(QStringLiteral("AnmeldenTitel"));
    auto* text = new QLabel(tr("Wählen Sie den Ordner mit den Daten der PV-Flotte. Er muss mindestens die Datei "
                               "<b>anlagen.csv</b> enthalten; wetterstationen.csv, wechselrichter.csv, "
                               "stoerungen.csv und die Verlaufsordner werden mitgeladen, wenn vorhanden."),
                            karte);
    text->setObjectName(QStringLiteral("Gedimmt"));
    text->setWordWrap(true);
    // feste Breite, damit die Höhe des umbrochenen Textes stimmt
    // (Kartenbreite − Ränder − Symbol − Abstand)
    text->setFixedWidth(720 - 64 - 44 - 14);

    m_fehler = new QLabel(karte);
    m_fehler->setWordWrap(true);
    m_fehler->setStyleSheet(QStringLiteral("color: #dc2626;"));

    m_pfad = new QLineEdit(karte);
    m_pfad->setPlaceholderText(tr("Pfad zum Datenordner"));
    m_pfad->setClearButtonEnabled(true);
    auto* durchsuchenKnopf = new QPushButton(tr("Durchsuchen …"), karte);
    auto* zeile = new QHBoxLayout;
    zeile->setSpacing(8);
    zeile->addWidget(m_pfad, 1);
    zeile->addWidget(durchsuchenKnopf);

    m_pruefung = new QLabel(karte);
    m_pruefung->setWordWrap(true);
    m_pruefung->setTextFormat(Qt::RichText);

    auto* hinweis = new QLabel(tr("Die Wahl wird gespeichert und beim nächsten Start wieder verwendet."), karte);
    hinweis->setObjectName(QStringLiteral("Gedimmt"));

    m_uebernehmen = new QPushButton(tr("Übernehmen"), karte);
    m_uebernehmen->setObjectName(QStringLiteral("Primaer"));
    m_abbrechen = new QPushButton(tr("Beenden"), karte);
    auto* knoepfe = new QHBoxLayout;
    knoepfe->addWidget(hinweis, 1);
    knoepfe->addWidget(m_abbrechen);
    knoepfe->addWidget(m_uebernehmen);

    auto* kopf = new QHBoxLayout;
    kopf->setSpacing(14);
    kopf->addWidget(symbol, 0, Qt::AlignTop);
    auto* kopfText = new QVBoxLayout;
    kopfText->setSpacing(6);
    kopfText->addWidget(m_titel);
    kopfText->addWidget(text);
    kopf->addLayout(kopfText, 1);

    auto* v = new QVBoxLayout(karte);
    v->setContentsMargins(32, 30, 32, 26);
    v->setSpacing(12);
    v->addLayout(kopf);
    v->addWidget(m_fehler);
    v->addSpacing(4);
    v->addLayout(zeile);
    v->addWidget(m_pruefung);
    v->addSpacing(8);
    v->addLayout(knoepfe);

    auto* raster = new QVBoxLayout(this);
    raster->addStretch(1);
    raster->addWidget(karte, 0, Qt::AlignHCenter);
    raster->addStretch(1);

    connect(m_pfad, &QLineEdit::textChanged, this, &DatenordnerSeite::pruefe);
    connect(m_pfad, &QLineEdit::returnPressed, m_uebernehmen, &QPushButton::click);
    connect(durchsuchenKnopf, &QPushButton::clicked, this, &DatenordnerSeite::durchsuchen);
    connect(m_abbrechen, &QPushButton::clicked, this, &DatenordnerSeite::abgebrochen);
    connect(m_uebernehmen, &QPushButton::clicked, this, [this] {
        if (istDatenordner(m_pfad->text().trimmed()))
            emit ordnerGewaehlt(QDir(m_pfad->text().trimmed()).absolutePath());
    });
    pruefe();
}

bool DatenordnerSeite::istDatenordner(const QString& ordner)
{
    return !ordner.isEmpty() && QFileInfo::exists(QDir(ordner).filePath(QStringLiteral("anlagen.csv")));
}

void DatenordnerSeite::vorbereiten(const QString& ordner, const QString& fehler, bool zurueckMoeglich)
{
    m_titel->setText(fehler.isEmpty() ? tr("Datenordner ändern") : tr("Datenordner festlegen"));
    m_fehler->setText(fehler);
    m_fehler->setVisible(!fehler.isEmpty());
    m_abbrechen->setText(zurueckMoeglich ? tr("Zurück") : tr("Beenden"));
    m_pfad->setText(QDir::toNativeSeparators(ordner));
    pruefe();
    m_pfad->setFocus();
}

void DatenordnerSeite::durchsuchen()
{
    const QString start = m_pfad->text().trimmed().isEmpty() ? QDir::homePath() : m_pfad->text().trimmed();
    const QString ordner = QFileDialog::getExistingDirectory(this, tr("Datenordner wählen"), start);
    if (!ordner.isEmpty())
        m_pfad->setText(QDir::toNativeSeparators(ordner));
}

void DatenordnerSeite::pruefe()
{
    // Sofortige Rückmeldung, welche Dateien im gewählten Ordner liegen
    const QString pfad = m_pfad->text().trimmed();
    const bool ok = istDatenordner(pfad);
    m_uebernehmen->setEnabled(ok);
    if (pfad.isEmpty()) {
        m_pruefung->clear();
        return;
    }
    if (!QFileInfo(pfad).isDir()) {
        m_pruefung->setText(QStringLiteral("<span style='color:#dc2626'>✗ </span>") + tr("Der Ordner existiert nicht."));
        return;
    }
    const QDir dir(pfad);
    QStringList teile;
    for (const QString& datei : {QStringLiteral("anlagen.csv"), QStringLiteral("wetterstationen.csv"),
                                 QStringLiteral("wechselrichter.csv"), QStringLiteral("stoerungen.csv")}) {
        const bool da = QFileInfo::exists(dir.filePath(datei));
        teile.append(QStringLiteral("<span style='color:%1'>%2</span> %3")
                         .arg(da ? QStringLiteral("#16a34a") : QStringLiteral("#dc2626"),
                              da ? QStringLiteral("✓") : QStringLiteral("✗"), datei));
    }
    const int batches = int(dir.entryList({QStringLiteral("anlagen_verlauf_batch_*")}, QDir::Dirs).size());
    teile.append(tr("%1 Verlaufsordner").arg(batches));
    m_pruefung->setText(teile.join(QStringLiteral(" &nbsp;·&nbsp; "))
                        + (ok ? QString() : QStringLiteral("<br><span style='color:#dc2626'>%1</span>")
                                                .arg(tr("Ohne anlagen.csv kann der Ordner nicht verwendet werden."))));
}
