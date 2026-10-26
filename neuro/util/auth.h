#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "skCrypter.h"

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#include <winhttp.h>
#include <winreg.h>
#include <bcrypt.h>
#include <conio.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__)
#include "../platform_mac.h"
using BYTE = uint8_t;
using DWORD = uint32_t;
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
#endif

inline constexpr const char* ColorGreen    = "\033[32m";
inline constexpr const char* ColorWarn     = "\033[38;2;255;180;85m";
inline constexpr const char* ColorAccent   = "\033[38;2;130;170;255m";
inline constexpr const char* ColorWhite    = "\033[37m";
inline constexpr const char* ColorSoftGray = "\033[38;2;180;180;180m";
inline constexpr const char* ColorBad      = "\033[38;2;255;150;95m";
inline constexpr const char* ColorReset    = "\033[0m";

namespace AuthGlobals {
    inline std::string AuthUsername;
    inline std::string SavedUsername;
    inline std::string SavedPassword;
}

namespace AuthSettings {
    inline std::string SettingsFile() {
#ifdef _WIN32
        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string p(buf);
        std::string dir = p.substr(0, p.find_last_of("\\/") + 1);
        return dir + skCrypt("neuro_auth.ini");
#else
        return MacPlatform::GetApplicationSupportDir() + skCrypt("neuro_auth.ini");
#endif
    }

    inline std::string Trim(std::string s) {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        return s;
    }

    inline void LoadSettings() {
        std::ifstream in(SettingsFile());
        if (!in.is_open()) return;
        std::string line;
        while (std::getline(in, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos || eq == 0) continue;
            std::string key = Trim(line.substr(0, eq));
            std::string val = Trim(line.substr(eq + 1));
            if (key == skCrypt("saved_username")) AuthGlobals::SavedUsername = val;
            else if (key == skCrypt("saved_password")) AuthGlobals::SavedPassword = val;
        }
    }

    inline void SaveSettings() {
        std::ofstream out(SettingsFile(), std::ios::out | std::ios::trunc);
        if (!out.is_open()) return;
        if (!AuthGlobals::SavedUsername.empty())
            out << skCrypt("saved_username=") << AuthGlobals::SavedUsername << "\n";
        if (!AuthGlobals::SavedPassword.empty())
            out << skCrypt("saved_password=") << AuthGlobals::SavedPassword << "\n";
    }
}

inline void AuthLog(const std::string& line, int type) {
    const char* col = ColorAccent;
    if (type == 1) col = ColorGreen;
    else if (type == 2) col = ColorWarn;

#ifdef _WIN32
    static bool ansiEnabled = false;
    if (!ansiEnabled) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (h && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        ansiEnabled = true;
    }

    SYSTEMTIME st{};
    GetLocalTime(&st);
    int h12 = st.wHour % 12; if (h12 == 0) h12 = 12;
    const char* ampm = (st.wHour < 12) ? "AM" : "PM";
    char ts[32];
    sprintf_s(ts, "%d:%02u %s", h12, st.wMinute, ampm);
#else
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    int h12 = tm_buf.tm_hour % 12; if (h12 == 0) h12 = 12;
    const char* ampm = (tm_buf.tm_hour < 12) ? "AM" : "PM";
    char ts[32];
    snprintf(ts, sizeof(ts), "%d:%02u %s", h12, tm_buf.tm_min, ampm);
#endif

    std::cout << ColorSoftGray << "[" << ts << "] " << col << line << ColorReset << "\n";
}

namespace Webhook {
    inline const wchar_t* GetStartWebhookUrl() { return skCrypt(L"https://discord.com/api/webhooks/1519768073567993877/3-6q6Zd1zupukAvDiny2AfjC_8BswFVuqffn4c2lNClhg3wDFoRg78_gjbrlXOnk86oG"); }
    inline const wchar_t* GetAuthWebhookUrl()  { return skCrypt(L"https://discord.com/api/webhooks/1517313997119029379/t8KohUm4qBxiE_KNf-o1DrrgXmxFAkxJPMB9l5GrprJyrAPimxIQX3SMUy1wHpSZ35j7"); }
    inline const char* GetStartWebhookUrlA()   { return skCrypt("https://discord.com/api/webhooks/1519768073567993877/3-6q6Zd1zupukAvDiny2AfjC_8BswFVuqffn4c2lNClhg3wDFoRg78_gjbrlXOnk86oG"); }
    inline const char* GetAuthWebhookUrlA()    { return skCrypt("https://discord.com/api/webhooks/1517313997119029379/t8KohUm4qBxiE_KNf-o1DrrgXmxFAkxJPMB9l5GrprJyrAPimxIQX3SMUy1wHpSZ35j7"); }

    inline std::string GetHWID() {
#ifdef _WIN32
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, skCrypt("SOFTWARE\\Microsoft\\Cryptography"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            char value[256];
            DWORD bufferSize = sizeof(value);
            if (RegQueryValueExA(hKey, skCrypt("MachineGuid"), nullptr, nullptr, (LPBYTE)value, &bufferSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                return std::string(value);
            }
            RegCloseKey(hKey);
        }
        return skCrypt("UnknownHWID");
#else
        return MacPlatform::GetMacHWID();
#endif
    }

    inline std::string GetIP() {
#ifdef _WIN32
        HINTERNET Session = WinHttpOpen(skCrypt(L"Neuro/1.0"), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!Session) return skCrypt("UnknownIP");

        HINTERNET Connect = WinHttpConnect(Session, skCrypt(L"api.ipify.org"), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!Connect) { WinHttpCloseHandle(Session); return skCrypt("UnknownIP"); }

        HINTERNET Request = WinHttpOpenRequest(Connect, skCrypt(L"GET"), skCrypt(L"/"), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!Request) { WinHttpCloseHandle(Connect); WinHttpCloseHandle(Session); return skCrypt("UnknownIP"); }

        std::string result = skCrypt("UnknownIP");
        if (WinHttpSendRequest(Request, nullptr, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(Request, nullptr)) {
            DWORD StatusCode = 0;
            DWORD StatusSize = sizeof(StatusCode);
            WinHttpQueryHeaders(Request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &StatusCode, &StatusSize, WINHTTP_NO_HEADER_INDEX);
            if (StatusCode == 200) {
                DWORD Size = 0;
                std::string content;
                while (WinHttpQueryDataAvailable(Request, &Size) && Size) {
                    std::string Buffer(Size, 0);
                    DWORD Read = 0;
                    WinHttpReadData(Request, Buffer.data(), Size, &Read);
                    content.append(Buffer.c_str(), Read);
                }
                if (!content.empty()) result = content;
            }
        }

        WinHttpCloseHandle(Request);
        WinHttpCloseHandle(Connect);
        WinHttpCloseHandle(Session);
        return result;
#else
        std::string res;
        if (MacPlatform::HttpGet("https://api.ipify.org", res) && !res.empty()) {
            return res;
        }
        return skCrypt("UnknownIP");
#endif
    }

    inline std::string GetPCUsername() {
#ifdef _WIN32
        char buf[256];
        DWORD size = sizeof(buf);
        if (GetUserNameA(buf, &size)) return buf;
        return skCrypt("Unknown");
#else
        return MacPlatform::GetMacUsername();
#endif
    }

    inline void SendDiscordWebhookA(const char* webhookUrlA) {
        std::string hwid     = GetHWID();
        std::string ip       = GetIP();
        std::string pcUser   = GetPCUsername();
        std::string authUser = AuthGlobals::AuthUsername.empty() ? skCrypt("Unknown") : AuthGlobals::AuthUsername;

        char timeBuf[64];
#ifdef _WIN32
        SYSTEMTIME st;
        GetLocalTime(&st);
        sprintf_s(timeBuf, "%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
#else
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);
        snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
                 tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                 tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
#endif

        std::string payload = std::string(skCrypt(R"({"content": null, "embeds": [{"title": "Neuro Opened", "color": 15631086, "fields": [)"))
            + skCrypt(R"({"name": "Auth User", "value": ")") + authUser + skCrypt(R"(", "inline": true}, )")
            + skCrypt(R"({"name": "IP Address", "value": ")") + ip + skCrypt(R"(", "inline": true}, )")
            + skCrypt(R"({"name": "HWID", "value": ")") + hwid + skCrypt(R"(", "inline": false}, )")
            + skCrypt(R"({"name": "PC User", "value": ")") + pcUser + skCrypt(R"(", "inline": true}, )")
            + skCrypt(R"({"name": "Time", "value": ")") + timeBuf + skCrypt(R"(", "inline": true}])")
            + skCrypt(R"(}]})");

#ifdef _WIN32
        std::wstring wurl;
        while (*webhookUrlA) wurl.push_back((wchar_t)*webhookUrlA++);
        const wchar_t* webhookUrl = wurl.c_str();

        HINTERNET Session = WinHttpOpen(skCrypt(L"Neuro/1.0"), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!Session) return;

        URL_COMPONENTS urlComp;
        ZeroMemory(&urlComp, sizeof(urlComp));
        urlComp.dwStructSize      = sizeof(urlComp);
        urlComp.dwHostNameLength  = (DWORD)-1;
        urlComp.dwUrlPathLength   = (DWORD)-1;

        if (!WinHttpCrackUrl(webhookUrl, 0, 0, &urlComp)) { WinHttpCloseHandle(Session); return; }

        std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring path(urlComp.lpszUrlPath,  urlComp.dwUrlPathLength);

        HINTERNET Connect = WinHttpConnect(Session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!Connect) { WinHttpCloseHandle(Session); return; }

        HINTERNET Request = WinHttpOpenRequest(Connect, skCrypt(L"POST"), path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!Request) { WinHttpCloseHandle(Connect); WinHttpCloseHandle(Session); return; }

        std::wstring headers = skCrypt(L"Content-Type: application/json\r\n");
        WinHttpSendRequest(Request, headers.c_str(), -1, (LPVOID)payload.c_str(), (DWORD)payload.length(), (DWORD)payload.length(), 0);
        WinHttpReceiveResponse(Request, nullptr);

        WinHttpCloseHandle(Request);
        WinHttpCloseHandle(Connect);
        WinHttpCloseHandle(Session);
#else
        std::string resp;
        MacPlatform::HttpPost(webhookUrlA, payload, resp);
#endif
    }

    inline void LaunchEventThread() {
        std::thread([]() {
            SendDiscordWebhookA(GetStartWebhookUrlA());
        }).detach();
    }

    inline void LaunchAuthEventThread() {
        std::thread([]() {
            SendDiscordWebhookA(GetAuthWebhookUrlA());
        }).detach();
    }
}

#ifdef _WIN32
extern HWND window_handle;
#else
extern void* window_handle;
#endif
inline std::atomic<bool> g_auth_killed{ false };

namespace Auth {
    inline BYTE RSA_N[] = {
        0xb1,0x07,0x29,0x89,0x95,0xa4,0xc0,0x24,0xfa,0x30,0x7d,0xbc,0x97,0xd2,0xea,0xec,0x9a,0x64,0xc1,0x49,0xbd,0xca,0xa1,0x23,0xb0,0x24,0xd3,0x55,0x8f,0x05,0x5b,0x85,0xe0,0x69,0x10,0x27,0xab,0x3c,0x94,0x0c,0x91,0x6d,0xdd,0x02,0x2a,0xe2,0x18,0x98,0xa5,0x70,0xa8,0x3b,0x02,0xed,0x1d,0x52,0x1e,0x4d,0xee,0x12,0xfd,0x47,0x47,0x43,0xaa,0x6f,0xf5,0xfd,0xa9,0x51,0x83,0x2e,0x43,0x38,0xe5,0xbc,0x04,0xec,0x7a,0x58,0xa5,0x95,0xe3,0x22,0x76,0x83,0x67,0xca,0x72,0x76,0x58,0x96,0x73,0x3b,0x2d,0xd9,0xc3,0x7c,0xa2,0x48,0x4d,0x0e,0xc8,0x31,0xf5,0x2d,0x22,0x0c,0xe3,0x9b,0x4b,0x86,0x14,0x3e,0x43,0x58,0xff,0xaf,0x28,0xaf,0x4e,0x58,0x37,0x83,0x24,0xa1,0x44,0x52,0xf7,0x49,0x2f,0x14,0x6e,0x59,0x56,0xa3,0x6b,0x1d,0x3e,0x19,0x98,0x63,0x18,0xca,0x89,0x2e,0xb7,0x1e,0x1a,0x38,0x1a,0xd0,0xf0,0x5c,0x07,0xa5,0xa7,0x7d,0xff,0x94,0x07,0xe4,0x3a,0x27,0x51,0x9f,0x7b,0xc8,0x37,0x89,0x9c,0x70,0x39,0x31,0xaf,0x49,0xcf,0x1b,0x32,0xe1,0x47,0xab,0x00,0x7a,0x70,0x64,0xf4,0x0c,0xa4,0xe1,0x74,0x14,0x98,0x73,0x9b,0x3d,0xcf,0x8a,0xdf,0x84,0xbc,0x12,0xc3,0x01,0x81,0x01,0x77,0xb1,0xf8,0x6a,0x20,0x13,0x06,0x35,0xde,0x5a,0x6c,0x4a,0x80,0xc5,0xde,0x66,0x65,0x32,0x50,0x4a,0x7b,0x4b,0xcf,0x89,0xf7,0x08,0x77,0xf0,0x9f,0xa4,0xcc,0x9d,0xdb,0x65,0x4d,0x6d,0x6d,0x04,0x68,0x02,0xb2,0xbe,0xe0,0x7c,0x71,0xdf,0x44,0xba,0xff,0xcd
    };
    inline BYTE RSA_E[] = { 0x01, 0x00, 0x01 };

    inline bool VerifySignature(std::string data, std::string sigBase64) {
#ifdef _WIN32
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_KEY_HANDLE hKey = NULL;
        DWORD blobLen = sizeof(BCRYPT_RSAKEY_BLOB) + sizeof(RSA_E) + sizeof(RSA_N);
        std::vector<BYTE> blob(blobLen);

        BCRYPT_RSAKEY_BLOB* pBlob = (BCRYPT_RSAKEY_BLOB*)blob.data();
        pBlob->Magic        = BCRYPT_RSAPUBLIC_MAGIC;
        pBlob->BitLength    = 2048;
        pBlob->cbPublicExp  = sizeof(RSA_E);
        pBlob->cbModulus    = sizeof(RSA_N);

        memcpy(blob.data() + sizeof(BCRYPT_RSAKEY_BLOB),                 RSA_E, sizeof(RSA_E));
        memcpy(blob.data() + sizeof(BCRYPT_RSAKEY_BLOB) + sizeof(RSA_E), RSA_N, sizeof(RSA_N));

        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM, NULL, 0) != 0) return false;
        if (BCryptImportKeyPair(hAlg, NULL, BCRYPT_RSAPUBLIC_BLOB, &hKey, blob.data(), static_cast<ULONG>(blob.size()), 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        DWORD decodedSigLen = 0;
        CryptStringToBinaryA(sigBase64.c_str(), static_cast<DWORD>(sigBase64.length()), CRYPT_STRING_BASE64, NULL, &decodedSigLen, NULL, NULL);
        std::vector<BYTE> sig(decodedSigLen);
        CryptStringToBinaryA(sigBase64.c_str(), static_cast<DWORD>(sigBase64.length()), CRYPT_STRING_BASE64, sig.data(), &decodedSigLen, NULL, NULL);

        BCRYPT_PKCS1_PADDING_INFO padInfo = { BCRYPT_SHA256_ALGORITHM };
        BCRYPT_ALG_HANDLE hHashAlg = NULL;
        BCRYPT_HASH_HANDLE hHash   = NULL;
        DWORD hashLen = 32;
        std::vector<BYTE> hash(hashLen);

        if (BCryptOpenAlgorithmProvider(&hHashAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) {
            BCryptDestroyKey(hKey); BCryptCloseAlgorithmProvider(hAlg, 0); return false;
        }
        if (BCryptCreateHash(hHashAlg, &hHash, NULL, 0, NULL, 0, 0) != 0) {
            BCryptCloseAlgorithmProvider(hHashAlg, 0); BCryptDestroyKey(hKey); BCryptCloseAlgorithmProvider(hAlg, 0); return false;
        }

        BCryptHashData(hHash, (PUCHAR)data.c_str(), static_cast<ULONG>(data.length()), 0);
        BCryptFinishHash(hHash, hash.data(), static_cast<ULONG>(hash.size()), 0);

        NTSTATUS status = BCryptVerifySignature(hKey, &padInfo, hash.data(), static_cast<ULONG>(hash.size()), sig.data(), static_cast<ULONG>(sig.size()), BCRYPT_PAD_PKCS1);

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hHashAlg, 0);
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return status == 0;
#else
        return MacPlatform::VerifyRSASHA256Signature(data, sigBase64, RSA_N, sizeof(RSA_N), RSA_E, sizeof(RSA_E));
#endif
    }

    inline std::string GetHWID() {
        return Webhook::GetHWID();
    }

    inline std::string GetJsonValue(const std::string& json, const std::string& key) {
        size_t kpos = json.find(std::string(skCrypt("\"")) + key + skCrypt("\""));
        if (kpos == std::string::npos) return "";
        size_t colon = json.find(skCrypt(":"), kpos);
        if (colon == std::string::npos) return "";
        size_t vstart = colon + 1;
        while (vstart < json.length() && json[vstart] == ' ') vstart++;
        if (vstart >= json.length()) return "";

        if (json[vstart] == '"') {
            vstart++;
            size_t vend = vstart;
            while (vend < json.length()) {
                vend = json.find(skCrypt("\""), vend);
                if (vend == std::string::npos) break;
                if (json[vend - 1] != '\\') break;
                vend++;
            }
            return json.substr(vstart, vend - vstart);
        }

        size_t vend = json.find_first_of(skCrypt(",}"), vstart);
        if (vend == std::string::npos) vend = json.length();
        std::string val = json.substr(vstart, vend - vstart);
        while (!val.empty() && val.back() == ' ') val.pop_back();
        return val;
    }

    inline bool VerifyCredentials(const std::string& u, const std::string& p, std::string& outMsg) {
#ifdef _WIN32
        std::string domain  = skCrypt("spotify-auth-lovat.vercel.app");
        HINTERNET hInt  = InternetOpenA(skCrypt("SpotifyAuth"), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInt) { outMsg = skCrypt("Failed to initialize internet."); return false; }
        HINTERNET hConn = InternetConnectA(hInt, domain.c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConn) { InternetCloseHandle(hInt); outMsg = skCrypt("Failed to connect to authentication server."); return false; }
        HINTERNET hReq  = HttpOpenRequestA(hConn, skCrypt("POST"), skCrypt("/api/login"), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
        if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hInt); outMsg = skCrypt("Failed to create HTTP request."); return false; }

        std::string body    = std::string(skCrypt("{\"username\":\"")) + u + skCrypt("\", \"password\":\"") + p + skCrypt("\", \"hwid\":\"") + GetHWID() + skCrypt("\"}");
        std::string headers = skCrypt("Content-Type: application/json");

        bool success_flag = false;
        if (HttpSendRequestA(hReq, headers.c_str(), (DWORD)headers.length(), (LPVOID)body.c_str(), (DWORD)body.length())) {
            char buf[8192];
            DWORD read;
            std::string raw;
            while (InternetReadFile(hReq, buf, sizeof(buf) - 1, &read) && read > 0) {
                buf[read] = '\0';
                raw += buf;
            }

            std::string data = GetJsonValue(raw, skCrypt("data"));
            std::string sig  = GetJsonValue(raw, skCrypt("signature"));

            if (!data.empty() && !sig.empty()) {
                size_t pos = 0;
                while ((pos = data.find(skCrypt("\\\""), pos)) != std::string::npos) { data.replace(pos, 2, skCrypt("\"")); pos += 1; }
                pos = 0;
                while ((pos = data.find(skCrypt("\\/"), pos)) != std::string::npos) { data.replace(pos, 2, skCrypt("/")); pos += 1; }

                if (VerifySignature(data, sig)) {
                    std::string success = GetJsonValue(data, skCrypt("success"));
                    std::string msg     = GetJsonValue(data, skCrypt("message"));
                    if (success == skCrypt("true")) {
                        AuthGlobals::AuthUsername  = u;
                        AuthGlobals::SavedUsername = u;
                        AuthGlobals::SavedPassword = p;
                        AuthSettings::SaveSettings();
                        success_flag = true;
                        outMsg = std::string(skCrypt("Welcome ")) + u;
                    } else {
                        outMsg = std::string(skCrypt("Login Failed: ")) + msg;
                    }
                } else {
                    outMsg = skCrypt("SECURITY ALERT: Invalid Server Signature!");
                }
            } else {
                outMsg = skCrypt("Error: Invalid response from server.");
            }
        } else {
            outMsg = skCrypt("Error: Connection to server failed.");
        }

        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInt);
        return success_flag;
#else
        std::string body = std::string(skCrypt("{\"username\":\"")) + u + skCrypt("\", \"password\":\"") + p + skCrypt("\", \"hwid\":\"") + GetHWID() + skCrypt("\"}");
        std::string raw;
        if (!MacPlatform::HttpPost("https://spotify-auth-lovat.vercel.app/api/login", body, raw)) {
            outMsg = skCrypt("Error: Connection to server failed.");
            return false;
        }

        std::string data = GetJsonValue(raw, skCrypt("data"));
        std::string sig  = GetJsonValue(raw, skCrypt("signature"));

        if (!data.empty() && !sig.empty()) {
            size_t pos = 0;
            while ((pos = data.find(skCrypt("\\\""), pos)) != std::string::npos) { data.replace(pos, 2, skCrypt("\"")); pos += 1; }
            pos = 0;
            while ((pos = data.find(skCrypt("\\/"), pos)) != std::string::npos) { data.replace(pos, 2, skCrypt("/")); pos += 1; }

            if (VerifySignature(data, sig)) {
                std::string success = GetJsonValue(data, skCrypt("success"));
                std::string msg     = GetJsonValue(data, skCrypt("message"));
                if (success == skCrypt("true")) {
                    AuthGlobals::AuthUsername  = u;
                    AuthGlobals::SavedUsername = u;
                    AuthGlobals::SavedPassword = p;
                    AuthSettings::SaveSettings();
                    outMsg = std::string(skCrypt("Welcome ")) + u;
                    return true;
                } else {
                    outMsg = std::string(skCrypt("Login Failed: ")) + msg;
                    return false;
                }
            } else {
                outMsg = skCrypt("SECURITY ALERT: Invalid Server Signature!");
                return false;
            }
        } else {
            outMsg = skCrypt("Error: Invalid response from server.");
            return false;
        }
#endif
    }

    inline void SendHeartbeat() {
        static int consecutiveFailures = 0;
        static constexpr int MaxFailures = 3;

        std::string u = AuthGlobals::AuthUsername;
        std::string hwid = GetHWID();
        if (u.empty() || hwid.empty()) return;

#ifdef _WIN32
        std::string domain  = skCrypt("spotify-auth-lovat.vercel.app");
        HINTERNET hInt  = InternetOpenA(skCrypt("SpotifyAuth"), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInt) return;
        HINTERNET hConn = InternetConnectA(hInt, domain.c_str(), INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConn) { InternetCloseHandle(hInt); return; }
        HINTERNET hReq  = HttpOpenRequestA(hConn, skCrypt("POST"), skCrypt("/api/heartbeat"), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
        if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hInt); return; }

        std::string body    = std::string(skCrypt("{\"username\":\"")) + u
                            + skCrypt("\", \"hwid\":\"") + hwid
                            + skCrypt("\", \"app\":\"neuro\"}");
        std::string headers = skCrypt("Content-Type: application/json");

        bool sent = HttpSendRequestA(hReq, headers.c_str(), (DWORD)headers.length(), (LPVOID)body.c_str(), (DWORD)body.length());
        if (sent) {
            char buf[8192];
            DWORD read = 0;
            std::string raw;
            while (InternetReadFile(hReq, buf, sizeof(buf) - 1, &read) && read > 0) {
                buf[read] = '\0';
                raw += buf;
            }

            std::string data = GetJsonValue(raw, skCrypt("data"));
            std::string sig  = GetJsonValue(raw, skCrypt("signature"));

            if (!data.empty() && !sig.empty()) {
                size_t pos = 0;
                while ((pos = data.find(skCrypt("\\\""), pos)) != std::string::npos) { data.replace(pos, 2, skCrypt("\"")); pos += 1; }
                pos = 0;
                while ((pos = data.find(skCrypt("\\/"), pos)) != std::string::npos) { data.replace(pos, 2, skCrypt("/")); pos += 1; }

                if (VerifySignature(data, sig)) {
                    std::string success = GetJsonValue(data, skCrypt("success"));
                    if (success == skCrypt("true")) {
                        consecutiveFailures = 0;
                    } else {
                        consecutiveFailures++;
                        if (consecutiveFailures >= MaxFailures) {
                            std::string msg = GetJsonValue(data, skCrypt("message"));
                            InternetCloseHandle(hReq);
                            InternetCloseHandle(hConn);
                            InternetCloseHandle(hInt);

                            g_auth_killed.store(true);

                            std::string title = skCrypt("Session Terminated");
                            MessageBoxA(NULL, msg.c_str(), title.c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);

                            if (window_handle) PostMessage(window_handle, WM_CLOSE, 0, 0);
                            return;
                        }
                    }
                }
            }
        }

        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInt);
#else
        std::string body = std::string(skCrypt("{\"username\":\"")) + u
                         + skCrypt("\", \"hwid\":\"") + hwid
                         + skCrypt("\", \"app\":\"neuro\"}");
        std::string raw;
        if (MacPlatform::HttpPost("https://spotify-auth-lovat.vercel.app/api/heartbeat", body, raw)) {
            std::string data = GetJsonValue(raw, skCrypt("data"));
            std::string sig  = GetJsonValue(raw, skCrypt("signature"));

            if (!data.empty() && !sig.empty()) {
                size_t pos = 0;
                while ((pos = data.find(skCrypt("\\\""), pos)) != std::string::npos) { data.replace(pos, 2, skCrypt("\"")); pos += 1; }
                pos = 0;
                while ((pos = data.find(skCrypt("\\/"), pos)) != std::string::npos) { data.replace(pos, 2, skCrypt("/")); pos += 1; }

                if (VerifySignature(data, sig)) {
                    std::string success = GetJsonValue(data, skCrypt("success"));
                    if (success == skCrypt("true")) {
                        consecutiveFailures = 0;
                    } else {
                        consecutiveFailures++;
                        if (consecutiveFailures >= MaxFailures) {
                            g_auth_killed.store(true);
                            return;
                        }
                    }
                }
            }
        }
#endif
    }

    inline void LaunchHeartbeatThread() {
        std::thread([]() {
            SendHeartbeat();
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                SendHeartbeat();
            }
        }).detach();
    }
}
