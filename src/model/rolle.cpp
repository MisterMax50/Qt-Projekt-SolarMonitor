#include "rolle.h"


bool Rollen::darf(Rolle rolle, Recht recht)
{
    switch (recht) {
    case Recht::BerichtExportieren:
        return true;
    case Recht::StoerungBearbeiten:
        return rolle == Rolle::Service || rolle == Rolle::Admin;
    case Recht::StammdatenBearbeiten:
    case Recht::StammdatenExportieren:
    case Recht::BenutzerVerwalten:
        return rolle == Rolle::Admin;
    }
    return false;
}

QString Rollen::kennung(Rolle rolle)
{
    switch (rolle) {
    case Rolle::Betrieb: return QStringLiteral("betrieb");
    case Rolle::Service: return QStringLiteral("service");
    case Rolle::Admin:   return QStringLiteral("admin");
    }
    return {};
}

QString Rollen::anzeigename(Rolle rolle)
{
    switch (rolle) {
    case Rolle::Betrieb: return tr("Betrieb");
    case Rolle::Service: return tr("Service");
    case Rolle::Admin:   return tr("Administrator");
    }
    return {};
}

bool Rollen::ausKennung(const QString& kennung, Rolle* rolle)
{
    for (Rolle r : {Rolle::Betrieb, Rolle::Service, Rolle::Admin}) {
        if (Rollen::kennung(r) == kennung.trimmed().toLower()) {
            if (rolle)
                *rolle = r;
            return true;
        }
    }
    return false;
}
