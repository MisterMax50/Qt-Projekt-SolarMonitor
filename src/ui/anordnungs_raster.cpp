#include "anordnungs_raster.h"

#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QMouseEvent>
#include <QPainter>
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

void AnordnungsRaster::neueVorlage(const Vorlage& vorlage)
{
    m_vorlagen.append(vorlage);
    if (m_vorlagen.size() == 1)
        wendeVorlageAn(vorlage);
}

void AnordnungsRaster::wendeVorlageAn(const Vorlage& v)
{
    m_vorlage = v.id;
    m_plaetze = v.plaetze;
    m_standard = v.belegung;
    for (int i = 0; i < 8; ++i) {
        m_raster->setColumnStretch(i, i < v.spaltenGewicht.size() ? v.spaltenGewicht.at(i) : 0);
        m_raster->setRowStretch(i, i < v.zeilenGewicht.size() ? v.zeilenGewicht.at(i) : 0);
    }
}

void AnordnungsRaster::waehleVorlage(const QString& id)
{
    for (const Vorlage& v : std::as_const(m_vorlagen)) {
        if (v.id != id)
            continue;
        wendeVorlageAn(v);
        m_reihenfolge = v.belegung;
        QSettings().setValue(m_schluessel + QStringLiteral("_vorlage"), id);
        QSettings().setValue(m_schluessel, m_reihenfolge);
        ordneAn();
        emit anordnungGeaendert();
        return;
    }
}

QIcon AnordnungsRaster::vorschau(const QString& id) const
{
    // Plätze als kleine Rechtecke, Breiten/Höhen nach Gewichtung
    for (const Vorlage& v : m_vorlagen) {
        if (v.id != id)
            continue;
        int spalten = 0, zeilen = 0;
        for (const Platz& p : v.plaetze) {
            spalten = qMax(spalten, p.spalte + p.spalten);
            zeilen = qMax(zeilen, p.zeile + p.zeilen);
        }
        auto gewicht = [](const QList<int>& g, int i) { return qMax(1, i < g.size() ? g.at(i) : 1); };
        QVector<double> x(spalten + 1, 0.0), y(zeilen + 1, 0.0);
        for (int i = 0; i < spalten; ++i)
            x[i + 1] = x[i] + gewicht(v.spaltenGewicht, i);
        for (int i = 0; i < zeilen; ++i)
            y[i + 1] = y[i] + gewicht(v.zeilenGewicht, i);
        QIcon icon;
        for (int g : {32, 64}) {
            QPixmap pm(g, g);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing, true);
            const double s = g / 32.0, rand = 3 * s, luecke = 1.5 * s;
            const double b = g - 2 * rand, h = g - 2 * rand - 6 * s;
            for (int i = 0; i < v.plaetze.size(); ++i) {
                const Platz& pl = v.plaetze.at(i);
                const QRectF r(rand + b * x[pl.spalte] / x[spalten] + luecke / 2,
                               rand + 3 * s + h * y[pl.zeile] / y[zeilen] + luecke / 2,
                               b * (x[pl.spalte + pl.spalten] - x[pl.spalte]) / x[spalten] - luecke,
                               h * (y[pl.zeile + pl.zeilen] - y[pl.zeile]) / y[zeilen] - luecke);
                // Platz der Karte "karte" (Lageplan) in Orange hervorheben
                const bool istKarte = i < v.belegung.size() && v.belegung.at(i) == QLatin1String("karte");
                p.setPen(Qt::NoPen);
                p.setBrush(istKarte ? QColor(245, 158, 11) : QColor(125, 141, 163));
                p.drawRoundedRect(r, 1.5 * s, 1.5 * s);
            }
            p.end();
            icon.addPixmap(pm);
        }
        return icon;
    }
    return QIcon();
}

void AnordnungsRaster::fuegeHinzu(const QString& id, QWidget* karte, int griffHoehe)
{
    karte->setParent(this);
    karte->installEventFilter(this);
    karte->setMouseTracking(true);
    m_karten.insert(id, karte);
    m_griff.insert(karte, griffHoehe);
    if (m_vorlagen.isEmpty())
        m_standard.append(id);
    m_reihenfolge = m_standard;
    ordneAn();
}

void AnordnungsRaster::ladeAnordnung()
{
    // nur übernehmen, wenn genau dieselben Karten gespeichert sind
    const QString vorlage = QSettings().value(m_schluessel + QStringLiteral("_vorlage")).toString();
    for (const Vorlage& v : std::as_const(m_vorlagen))
        if (v.id == vorlage)
            wendeVorlageAn(v);
    m_reihenfolge = m_standard;
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
    // Belegung der aktuellen Vorlage wiederherstellen
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
        if (QWidget* w = m_karten.value(m_reihenfolge.at(i))) // noch nicht hinzugefügte Karten überspringen
            m_raster->addWidget(w, p.zeile, p.spalte, p.zeilen, p.spalten);
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
