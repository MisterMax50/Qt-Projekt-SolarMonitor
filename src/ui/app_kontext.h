#pragma once

#include "model/rolle.h"

class Datenbasis;
class VerlaufRepository;
class FlottenAnalyse;

/**
 * @brief Bündelt die Dienste, die fast jede Ansicht braucht.
 *
 * Die Objekte gehören dem Datensatz des Hauptfensters; die Oberfläche hält nur
 * nicht besitzende Zeiger. So bleiben Konstruktoren schlank und die
 * Rechteprüfung ist überall gleich.
 */
struct AppKontext {
    Datenbasis*        daten = nullptr;
    VerlaufRepository* verlaeufe = nullptr;
    FlottenAnalyse*    analyse = nullptr;
    Rolle              rolle = Rolle::Betrieb;

    bool darf(Recht recht) const { return Rollen::darf(rolle, recht); }
};
