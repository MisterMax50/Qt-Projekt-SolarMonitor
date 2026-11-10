#pragma once

#include <QObject>
#include <QStringList>

/**
 * @brief Favoriten (angepinnte Anlagen) des angemeldeten Benutzers.
 *
 * Gespeichert in QSettings unter "favoriten/<kennung>", damit jeder
 * Benutzer seine eigenen Favoriten hat. Die Reihenfolge entspricht der
 * Reihenfolge des Anpinnens. Ansichten hören auf @ref geaendert.
 */
class Favoriten : public QObject {
    Q_OBJECT
public:
    static Favoriten* instanz();

    /// Nach der Anmeldung: Favoriten dieses Benutzers laden.
    void setzeBenutzer(const QString& kennung);

    bool ist(const QString& anlageId) const { return m_ids.contains(anlageId); }
    void setze(const QString& anlageId, bool favorit);
    void umschalten(const QString& anlageId) { setze(anlageId, !ist(anlageId)); }
    const QStringList& liste() const { return m_ids; }
    int anzahl() const { return int(m_ids.size()); }

signals:
    /// Eine Anlage wurde an- oder abgepinnt (leer = ganze Liste neu, z. B. Benutzerwechsel).
    void geaendert(const QString& anlageId);

private:
    using QObject::QObject;
    void speichere() const;

    QString     m_kennung;
    QStringList m_ids;
};
