// Windows-Hello-Anbindung ohne C++/WinRT (funktioniert auch mit MinGW).
//
// Genutzt wird Windows.Security.Credentials.UI.UserConsentVerifier über die
// rohe WinRT-ABI: Die Funktionen aus combase.dll werden zur Laufzeit
// geladen, die benötigten Schnittstellen sind unten als Vtables
// nachgebildet. Dieses Stück ist reines C++ mit windows.h, damit es sich
// auch ohne Qt prüfen lässt; es wird von windows_hello.cpp eingebunden.

#include <windows.h>

#include <string>

namespace hello_win32 {

using HSTRING_ = void*;

// Rückgabewerte der Windows-Aufzählungen
enum class Verfuegbarkeit { Verfuegbar = 0, KeinGeraet = 1, NichtEingerichtet = 2, Richtlinie = 3, Belegt = 4, Fehler = 99 };
enum class Pruefung { Bestaetigt = 0, KeinGeraet = 1, NichtEingerichtet = 2, Richtlinie = 3, Belegt = 4,
                      ZuVieleVersuche = 5, Abgebrochen = 6, Fehler = 99 };

// IIDs (aus den Windows-SDK-Headern bzw. nach dem WinRT-Verfahren für
// parametrisierte Schnittstellen berechnet)
const GUID IID_IUserConsentVerifierStatics = {0xAF4F3F91, 0x564C, 0x4DDC, {0xB8, 0xB5, 0x97, 0x34, 0x47, 0x62, 0x7C, 0x65}};
const GUID IID_IUserConsentVerifierInterop = {0x39E050C3, 0x4E74, 0x441A, {0x8D, 0xC0, 0xB8, 0x11, 0x04, 0xDF, 0x94, 0x9C}};
const GUID IID_IAsyncInfo = {0x00000036, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
// IAsyncOperation<UserConsentVerificationResult>
const GUID IID_AsyncOpErgebnis = {0xfd596ffd, 0x2318, 0x558f, {0x9d, 0xbe, 0xd2, 0x1d, 0xf4, 0x37, 0x64, 0xa5}};

// --- nachgebildete Schnittstellen (nur die benötigten Methoden) ---
struct IInspectable_ : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG*, GUID**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING_*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(int*) = 0;
};
struct IAsyncInfo_ : IInspectable_ {
    virtual HRESULT STDMETHODCALLTYPE get_Id(UINT32*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Status(int*) = 0; // 0 läuft, 1 fertig, 2 abgebrochen, 3 Fehler
    virtual HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Cancel() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
};
/// IAsyncOperation<Enum> – Ergebnis ist bei beiden Aufzählungen ein int32
struct IAsyncOperationInt_ : IInspectable_ {
    virtual HRESULT STDMETHODCALLTYPE put_Completed(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Completed(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetResults(int*) = 0;
};
struct IUserConsentVerifierStatics_ : IInspectable_ {
    virtual HRESULT STDMETHODCALLTYPE CheckAvailabilityAsync(IAsyncOperationInt_**) = 0;
    virtual HRESULT STDMETHODCALLTYPE RequestVerificationAsync(HSTRING_, IAsyncOperationInt_**) = 0;
};
struct IUserConsentVerifierInterop_ : IInspectable_ {
    virtual HRESULT STDMETHODCALLTYPE RequestVerificationForWindowAsync(HWND, HSTRING_, REFIID, void**) = 0;
};

// --- Funktionen aus combase.dll (zur Laufzeit geladen) ---
using RoInitialize_t = HRESULT(WINAPI*)(int);
using RoGetActivationFactory_t = HRESULT(WINAPI*)(HSTRING_, REFIID, void**);
using WindowsCreateString_t = HRESULT(WINAPI*)(const wchar_t*, UINT32, HSTRING_*);
using WindowsDeleteString_t = HRESULT(WINAPI*)(HSTRING_);

struct Combase {
    RoInitialize_t           roInitialize = nullptr;
    RoGetActivationFactory_t factory = nullptr;
    WindowsCreateString_t    erzeuge = nullptr;
    WindowsDeleteString_t    loesche = nullptr;
    bool ok = false;

    Combase()
    {
        HMODULE dll = LoadLibraryW(L"combase.dll");
        if (!dll)
            return;
        roInitialize = reinterpret_cast<RoInitialize_t>(reinterpret_cast<void*>(GetProcAddress(dll, "RoInitialize")));
        factory = reinterpret_cast<RoGetActivationFactory_t>(
            reinterpret_cast<void*>(GetProcAddress(dll, "RoGetActivationFactory")));
        erzeuge = reinterpret_cast<WindowsCreateString_t>(
            reinterpret_cast<void*>(GetProcAddress(dll, "WindowsCreateString")));
        loesche = reinterpret_cast<WindowsDeleteString_t>(
            reinterpret_cast<void*>(GetProcAddress(dll, "WindowsDeleteString")));
        ok = roInitialize && factory && erzeuge && loesche;
        if (ok)
            roInitialize(0); // RO_INIT_SINGLETHREADED; ist COM schon initialisiert, ist das in Ordnung
    }
};

inline Combase& combase()
{
    static Combase c;
    return c;
}

/// Activation Factory von UserConsentVerifier (Aufrufer gibt sie frei).
inline IUserConsentVerifierStatics_* statics()
{
    Combase& c = combase();
    if (!c.ok)
        return nullptr;
    static const wchar_t name[] = L"Windows.Security.Credentials.UI.UserConsentVerifier";
    HSTRING_ klasse = nullptr;
    if (FAILED(c.erzeuge(name, UINT32(sizeof(name) / sizeof(wchar_t) - 1), &klasse)))
        return nullptr;
    IUserConsentVerifierStatics_* s = nullptr;
    const HRESULT hr = c.factory(klasse, IID_IUserConsentVerifierStatics, reinterpret_cast<void**>(&s));
    c.loesche(klasse);
    return SUCCEEDED(hr) ? s : nullptr;
}

/**
 * Wartet auf eine asynchrone Windows-Operation. @p pumpe wird währenddessen
 * regelmäßig aufgerufen (hält die Oberfläche bedienbar).
 * Liefert true und das Ergebnis, wenn die Operation erfolgreich endete.
 */
template <typename Pumpe>
bool warte(IAsyncOperationInt_* op, int* ergebnis, Pumpe pumpe)
{
    IAsyncInfo_* info = nullptr;
    if (FAILED(op->QueryInterface(IID_IAsyncInfo, reinterpret_cast<void**>(&info))) || !info)
        return false;
    int status = 0;
    while (SUCCEEDED(info->get_Status(&status)) && status == 0) {
        pumpe();
        Sleep(15);
    }
    info->Release();
    return status == 1 && SUCCEEDED(op->GetResults(ergebnis));
}

template <typename Pumpe>
Verfuegbarkeit verfuegbarkeit(Pumpe pumpe)
{
    IUserConsentVerifierStatics_* s = statics();
    if (!s)
        return Verfuegbarkeit::Fehler;
    IAsyncOperationInt_* op = nullptr;
    int ergebnis = int(Verfuegbarkeit::Fehler);
    const bool ok = SUCCEEDED(s->CheckAvailabilityAsync(&op)) && op && warte(op, &ergebnis, pumpe);
    if (op)
        op->Release();
    s->Release();
    return ok ? Verfuegbarkeit(ergebnis) : Verfuegbarkeit::Fehler;
}

/// Zeigt den Windows-Hello-Dialog zum Fenster @p fenster an.
template <typename Pumpe>
Pruefung pruefe(HWND fenster, const std::wstring& nachricht, Pumpe pumpe)
{
    IUserConsentVerifierStatics_* s = statics();
    if (!s)
        return Pruefung::Fehler;
    IUserConsentVerifierInterop_* interop = nullptr;
    if (FAILED(s->QueryInterface(IID_IUserConsentVerifierInterop, reinterpret_cast<void**>(&interop))) || !interop) {
        s->Release();
        return Pruefung::Fehler;
    }
    HSTRING_ text = nullptr;
    combase().erzeuge(nachricht.c_str(), UINT32(nachricht.size()), &text);
    IAsyncOperationInt_* op = nullptr;
    int ergebnis = int(Pruefung::Fehler);
    const bool ok = SUCCEEDED(interop->RequestVerificationForWindowAsync(fenster, text, IID_AsyncOpErgebnis,
                                                                         reinterpret_cast<void**>(&op)))
        && op && warte(op, &ergebnis, pumpe);
    if (op)
        op->Release();
    if (text)
        combase().loesche(text);
    interop->Release();
    s->Release();
    return ok ? Pruefung(ergebnis) : Pruefung::Fehler;
}

/// "DOMÄNE\Benutzer" des angemeldeten Windows-Kontos.
inline std::wstring windowsKonto()
{
    wchar_t name[256] = {}, domaene[256] = {};
    DWORD n = 256;
    GetUserNameW(name, &n);
    GetEnvironmentVariableW(L"USERDOMAIN", domaene, 256);
    return std::wstring(domaene) + L"\\" + name;
}

} // namespace hello_win32
