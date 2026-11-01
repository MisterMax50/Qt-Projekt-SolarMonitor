#pragma once

#include <QAbstractTableModel>
#include <QIcon>
#include "ui/filter_proxy.h"

#include "ui/app_kontext.h"

struct Anlage;

/**
 * @brief Tabellenmodell der Anlagenliste (Pflichtpunkt 2).
 *
 * Liest direkt aus der Datenbasis (keine Kopie) und ergänzt Kennzahlen
 * der Flottenanalyse (PR, spez. Ertrag, Auffälligkeiten), sobald diese
 * vorliegen. Für jede Spalte liefert @c SortierRolle den Rohwert, damit
 * Zahlen und Daten korrekt sortieren (Erweiterung 2).
 */
class AnlageModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Spalte {
        SpId, SpTyp, SpOrt, SpLeistung, SpStatus, SpStation, SpWechselrichter,
        SpInbetrieb, SpOffeneStoerungen, SpPr, SpSpezErtrag, SpAuffaellig,
        SpAnzahl
    };
    static constexpr int SortierRolle = Qt::UserRole + 1;
    static constexpr int IdRolle = Qt::UserRole + 2;

    AnlageModel(const AppKontext& kontext, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int rolle = Qt::DisplayRole) const override;
    QVariant headerData(int spalte, Qt::Orientation o, int rolle = Qt::DisplayRole) const override;

    const Anlage* anlageAt(int zeile) const;
    bool istAuffaellig(int zeile) const;

public slots:
    void anlageGeaendert(const QString& anlageId);
    void kennzahlenGeaendert();

private:
    AppKontext m_kontext;
    QIcon      m_warnIcon; ///< einmal erzeugt, nicht bei jedem data()-Aufruf
};

/**
 * @brief Such- und Filtermodell (Pflichtpunkt 3, Erweiterung 2).
 *
 * Alle Kriterien werden UND-verknüpft, z. B. Typ = dach_gewerbe UND
 * Leistung > 100 kWp UND Status = gestoert.
 */
class AnlageFilterModel : public FilterProxy {
    Q_OBJECT
public:
    explicit AnlageFilterModel(QObject* parent = nullptr);

    void setSuchtext(const QString& text);           ///< anlage_id, Ort, wetterstation_id
    void setStatus(const QString& status);           ///< leer = alle
    void setTyp(const QString& typ);
    void setOrt(const QString& ort);
    void setLeistungsbereich(double minKwp, double maxKwp); ///< ≤ 0 = offen
    void setNurAuffaellige(bool an);
    void setNurOffeneStoerungen(bool an);

protected:
    bool filterAcceptsRow(int quellZeile, const QModelIndex& quellParent) const override;

private:
    QString m_suche;
    QString m_status;
    QString m_typ;
    QString m_ort;
    double  m_minKwp = 0.0;
    double  m_maxKwp = 0.0;
    bool    m_nurAuffaellig = false;
    bool    m_nurOffen = false;
};
