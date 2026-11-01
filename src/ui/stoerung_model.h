#pragma once

#include <QAbstractTableModel>
#include "ui/filter_proxy.h"

#include "ui/app_kontext.h"

struct Stoerung;

/**
 * @brief Tabellenmodell der Störungsliste (Erweiterung 1).
 */
class StoerungModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Spalte {
        SpId, SpAnlage, SpZeitpunkt, SpFehlercode, SpBeschreibung, SpSchwere, SpStatus, SpBehobenAm,
        SpAnzahl
    };
    static constexpr int SortierRolle = Qt::UserRole + 1;
    static constexpr int IdRolle = Qt::UserRole + 2;

    StoerungModel(const AppKontext& kontext, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int rolle = Qt::DisplayRole) const override;
    QVariant headerData(int spalte, Qt::Orientation o, int rolle = Qt::DisplayRole) const override;

    const Stoerung* stoerungAt(int zeile) const;

private slots:
    void neuLaden();

private:
    AppKontext m_kontext;
    int        m_anzahl = 0;
};

/// Filter: Suchtext, Status-Gruppe, Schwere, Anlage.
class StoerungFilterModel : public FilterProxy {
    Q_OBJECT
public:
    enum class StatusFilter { Alle, Offen, NurOffen, InBearbeitung, Behoben };

    explicit StoerungFilterModel(QObject* parent = nullptr);

    void setSuchtext(const QString& text);
    void setStatusFilter(StatusFilter f);
    void setSchwere(const QString& schwere);
    void setAnlage(const QString& anlageId);

protected:
    bool filterAcceptsRow(int quellZeile, const QModelIndex& quellParent) const override;

private:
    QString      m_suche;
    StatusFilter m_status = StatusFilter::Alle;
    QString      m_schwere;
    QString      m_anlage;
};
