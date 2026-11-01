#include "rolle.h"

#include <QCryptographicHash>

bool Rollen::darf(Rolle rolle, Recht recht)
{
    switch (recht) {
    case Recht::BerichtExportieren:
        return true;
    case Recht::StoerungBearbeiten:
        return rolle == Rolle::Service || rolle == Rolle::Admin;
    case Recht::StammdatenBearbeiten:
    case Recht::StammdatenExportieren:
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

bool Rollen::pinKorrekt(Rolle rolle, const QString& pin)
{
    // Nur die Hashes der Demo-PINs sind im Code hinterlegt
    // (betrieb = 0000, service = 5678, admin = 1234).
    const char* hash = nullptr;
    switch (rolle) {
    case Rolle::Betrieb:
        hash = "9af15b336e6a9619928537df30b2e6a2376569fcf9d7e773eccede65606529a0";
        break;
    case Rolle::Service:
        hash = "f8638b979b2f4f793ddb6dbd197e0ee25a7a6ea32b0ae22f5e3c5d119d839e75";
        break;
    case Rolle::Admin:
        hash = "03ac674216f3e15c761ee1a5e255f067953623c8b388b4459e13f978d7c846f4";
        break;
    }
    const QByteArray ist = QCryptographicHash::hash(pin.toUtf8(), QCryptographicHash::Sha256).toHex();
    return hash && ist == QByteArray(hash);
}
