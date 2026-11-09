#include "anordnungs_raster.h"

#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QMouseEvent>
#include <QSettings>

AnordnungsRaster::AnordnungsRaster(const QString& schluessel, QWidget* parent)
    : QWidget(parent)
    , m_schluessel(schluessel)
{
    m_raster = new QGridLayout(this);
    m_raster->setContentsMargins(0, 0, 0, 0);
    m_raster->setSpacing(16);

    // gestrichelter Platzhalter am verlassenen Platz
    m_platzhalter = new QFrame(this);
    m_platzhalter->setObjectName(QStringLiteral("Platzhalter"));
    m_platzhalter->hide();
    // hervorgehobener Zielplatz unter dem Mauszeiger
    m_ziel = new QFrame(this);
    m_ziel->setObjectName(QStringLiteral("Ablageziel"));
    m_ziel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_ziel->hide();
}

void AnordnungsRaster::neuerPlatz(int zeile, int spalte, int zeilen, int spalten)
{
    m_plaetze.append({zeile, spalte, zeilen, spalten});
}

void AnordnungsRaster::fuegeHinzu(const QString& id, QWidget* karte, int griffHoehe)
{
    karte->setParent(this);
    karte->installEventFilter(this);
    karte->setMouseTracking(true);
    m_karten.insert(id, karte);
    m_griff.insert(karte, griffHoehe);
    m_standard.append(id);
    m_reihenfolge = m_standard;
    ordneAn();
}

void AnordnungsRaster::ladeAnordnung()
{
    // nur übernehmen, wenn genau dieselben Karten gespeichert sind
    const QStringList gespeichert = QSettings().value(m_schluessel).toStringList();
    QStringList a = gespeichert, b = m_standard;
    a.sort();
    b.sort();
    if (a == b)
        m_reihenfolge = gespeichert;
    ordneAn();
}

void AnordnungsRaster::zuruecksetzen()
{
    m_reihenfolge = m_standard;
    QSettings().remove(m_schluessel);
    ordneAn();
    emit anordnungGeaendert();
}

void AnordnungsRaster::ordneAn()
{
    for (QWidget* w : std::as_const(m_karten))
        m_raster->removeWidget(w);
    for (int i = 0; i < m_reihenfolge.size() && i < m_plaetze.size(); ++i) {
        const Platz& p = m_plaetze.at(i);
        m_raster->addWidget(m_karten.value(m_reihenfolge.at(i)), p.zeile, p.spalte, p.zeilen, p.spalten);
    }
}

QRect AnordnungsRaster::platzRechteck(int platz) const
{
    const Platz& p = m_plaetze.at(platz);
    return m_raster->cellRect(p.zeile, p.spalte)
        .united(m_raster->cellRect(p.zeile + p.zeilen - 1, p.spalte + p.spalten - 1));
}

int AnordnungsRaster::platzBei(const QPoint& pos) const
{
    // Platz unter dem Mauszeiger, sonst der mit dem nächstgelegenen Mittelpunkt
    int bester = -1;
    double abstand = 1e18;
    for (int i = 0; i < m_rechtecke.size(); ++i) {
        if (m_rechtecke.at(i).contains(pos))
            return i;
        const QPointF d = QPointF(m_rechtecke.at(i).center() - pos);
        const double a = d.x() * d.x() + d.y() * d.y();
        if (a < abstand) {
            abstand = a;
            bester = i;
        }
    }
    return bester;
}

void AnordnungsRaster::beginneZiehen()
{
    QWidget* karte = m_kandidat;
    m_startPlatz = int(m_reihenfolge.indexOf(m_karten.key(karte)));
    if (m_startPlatz < 0)
        return;
    m_rechtecke.clear();
    for (int i = 0; i < m_plaetze.size(); ++i)
        m_rechtecke.append(platzRechteck(i));

    // Karte aus dem Raster lösen, Platzhalter an ihre Stelle
    const QRect start = karte->geometry();
    const Platz& p = m_plaetze.at(m_startPlatz);
    m_raster->removeWidget(karte);
    m_platzhalter->setMinimumSize(start.size());
    m_raster->addWidget(m_platzhalter, p.zeile, p.spalte, p.zeilen, p.spalten);
    m_platzhalter->show();
    karte->setGeometry(start);
    karte->raise();
    m_alterCursor = karte->cursor();
    karte->setCursor(Qt::ClosedHandCursor);
    m_ziel->setGeometry(m_rechtecke.at(m_startPlatz));
    m_ziel->show();
    m_ziel->raise();
    karte->raise();
    m_zieht = true;
}

void AnordnungsRaster::beendeZiehen(const QPoint& pos)
{
    QWidget* karte = m_kandidat;
    m_zieht = false;
    m_ziel->hide();
    m_raster->removeWidget(m_platzhalter);
    m_platzhalter->hide();
    if (karte)
        karte->setCursor(m_alterCursor);

    const int ziel = platzBei(pos);
    if (ziel >= 0 && ziel != m_startPlatz) {
        m_reihenfolge.swapItemsAt(m_startPlatz, ziel);
        QSettings().setValue(m_schluessel, m_reihenfolge);
        emit anordnungGeaendert();
    }
    ordneAn();
}

bool AnordnungsRaster::eventFilter(QObject* obj, QEvent* event)
{
    auto* karte = qobject_cast<QWidget*>(obj);
    if (!karte || !m_griff.contains(karte))
        return QWidget::eventFilter(obj, event);
    const int griff = m_griff.value(karte);

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint lokal = me->position().toPoint();
        if (me->button() == Qt::LeftButton && (griff == 0 || lokal.y() < griff)) {
            m_kandidat = karte;
            m_startGlobal = me->globalPosition().toPoint();
            m_versatz = lokal;
        }
        return false; // Klicks auf Kacheln weiterhin zulassen
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (!(me->buttons() & Qt::LeftButton)) {
            // Griff anzeigen, wenn die Maus über der Überschrift steht
            if (griff > 0)
                karte->setCursor(me->position().y() < griff ? Qt::OpenHandCursor : Qt::ArrowCursor);
            return false;
        }
        if (m_kandidat != karte)
            return false;
        const QPoint global = me->globalPosition().toPoint();
        if (!m_zieht) {
            if ((global - m_startGlobal).manhattanLength() < QApplication::startDragDistance())
                return false;
            beginneZiehen();
        }
        // Karte folgt frei dem Mauszeiger; Zielplatz hervorheben
        const QPoint imRaster = mapFromGlobal(global);
        karte->move(imRaster - m_versatz);
        const int ziel = platzBei(imRaster);
        if (ziel >= 0)
            m_ziel->setGeometry(m_rechtecke.at(ziel));
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (m_zieht && m_kandidat == karte && me->button() == Qt::LeftButton) {
            beendeZiehen(mapFromGlobal(me->globalPosition().toPoint()));
            m_kandidat = nullptr;
            return true; // kein Klick auf die Kachel auslösen
        }
        m_kandidat = nullptr;
        return false;
    }
    case QEvent::Leave:
        if (!m_zieht && griff > 0)
            karte->unsetCursor();
        return false;
    default:
        return false;
    }
}
