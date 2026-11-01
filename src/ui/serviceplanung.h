#pragma once

#include <QPointF>
#include <QWidget>

#include "eval/tourenplanung.h"
#include "repo/routen_dienst.h"
#include "ui/app_kontext.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

/**
 * @brief Priorisierte Einsatzliste und Tourenplanung (Erweiterung 7).
 *
 * Priorität:
 *  - A: offene kritische Störung, Status "gestört" oder keine Einspeisung
 *  - B: erkanntes Fehlerbild oder offene Störung
 *  - C: Minderertrag > 10 % des Erwarteten ohne erkanntes Fehlerbild
 * Innerhalb einer Priorität wird nach geschätztem Verlust in € sortiert
 * (Verlust in kWh × Einspeisevergütung, siehe FlottenAnalyse).
 *
 * Tourenplanung: Aus den Einsätzen im gewählten Umkreis um den Startpunkt
 * werden die N wichtigsten ausgewählt. Der Umkreis wird um große Gewässer
 * herum gemessen (Gewaesser::Umweg): Eine Anlage am anderen Seeufer liegt
 * nur dann im Umkreis, wenn sie auf dem Landweg nah genug ist. Die Reihenfolge wird über den
 * OSRM-Routingdienst auf dem echten Straßennetz optimiert (schnellste
 * Autoroute inkl. Straßenverlauf für die Karte). Ohne Internet wird mit
 * Nearest Neighbour + 2-opt auf Luftlinienbasis geplant.
 */
class Serviceplanung : public QWidget {
    Q_OBJECT
public:
    explicit Serviceplanung(const AppKontext& kontext, QWidget* parent = nullptr);

signals:
    void anlageOeffnen(const QString& anlageId);
    void tourAnzeigen(const QPointF& startLatLon, const QStringList& anlagenIds,
                      const QVector<QPointF>& verlauf, bool strassenroute, double umkreisKm);

public slots:
    void aktualisiere();

private slots:
    void planeTour();
    void strassenRouteDa(const StrassenRoute& route);
    void aktualisiereUmkreisInfo();
    void exportiere();

private:
    struct Einsatz {
        QString anlageId;
        QChar   prioritaet;
        QString grund;
        double  verlustKwh = 0.0;
        double  verlustEuro = 0.0;
        int     offen = 0;
        int     kritisch = 0;
    };

    QVector<int> zieleImUmkreis() const; ///< Indizes in m_einsaetze
    void zeigeTour(const QVector<int>& reihenfolge, const QVector<double>& etappenKm,
                   const QVector<double>& etappenMin);

    AppKontext       m_kontext;
    RoutenDienst*    m_routen = nullptr;
    QVector<TourStopp> m_tourZiele;
    QVector<QChar>   m_tourPrio;
    QVector<QPointF> m_tourVerlauf;  ///< Straßenverlauf oder Luftlinie um den See
    bool             m_tourStrasse = false;
    QVector<Einsatz> m_einsaetze;
    QStringList      m_tourIds;
    QPointF          m_tourStart;

    QLabel*       m_hinweis = nullptr;
    QTableWidget* m_liste = nullptr;
    QComboBox*    m_start = nullptr;
    QSpinBox*     m_anzahl = nullptr;
    QDoubleSpinBox* m_umkreis = nullptr;
    QCheckBox*    m_strasse = nullptr;
    QLabel*       m_umkreisInfo = nullptr;
    QTableWidget* m_tour = nullptr;
    QLabel*       m_tourSumme = nullptr;
    QPushButton*  m_btnKarte = nullptr;
    QPushButton*  m_btnExport = nullptr;
};
