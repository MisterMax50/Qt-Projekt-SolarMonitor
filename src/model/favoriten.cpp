#include "favoriten.h"

#include <QCoreApplication>
#include <QSettings>

Favoriten* Favoriten::instanz()
{
    static Favoriten* f = new Favoriten(QCoreApplication::instance());
    return f;
}

void Favoriten::setzeBenutzer(const QString& kennung)
{
    m_kennung = kennung;
    m_ids = kennung.isEmpty() ? QStringList()
                              : QSettings().value(QStringLiteral("favoriten/") + kennung).toStringList();
    m_ids.removeDuplicates();
    emit geaendert(QString());
}

void Favoriten::setze(const QString& anlageId, bool favorit)
{
    if (anlageId.isEmpty() || ist(anlageId) == favorit)
        return;
    if (favorit)
        m_ids.append(anlageId);
    else
        m_ids.removeAll(anlageId);
    speichere();
    emit geaendert(anlageId);
}

void Favoriten::speichere() const
{
    if (!m_kennung.isEmpty())
        QSettings().setValue(QStringLiteral("favoriten/") + m_kennung, m_ids);
}
