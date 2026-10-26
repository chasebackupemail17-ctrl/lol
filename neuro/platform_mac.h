#pragma once
#include <string>
#include <cstdint>
#include <vector>

#ifdef __APPLE__

namespace MacPlatform {

// UI & File system
std::string OpenFileDialog();
void RevealInFinder(const std::string& path);
std::string GetApplicationSupportDir();

// Permissions & Active app
bool CheckAccessibilityPermissions();
bool IsRobloxActive();

// Input monitoring and simulation
bool IsKeyDown(int macKeyCode);
bool IsMouseButtonDown(int button); // 0=LMB, 1=RMB, 2=MMB
void SendKey(int macKeyCode, bool down);
void SendMouseClick();

// Key code mapping between Windows VK and macOS Carbon virtual keycodes
int WindowsVkToMacKey(int vk);
int MacKeyToWindowsVk(int macKey);
std::string GetMacKeyName(int macKey);

// System info
std::string GetMacHWID();
std::string GetMacUsername();

// Network (using built-in libcurl)
bool HttpGet(const std::string& url, std::string& outResponse);
bool HttpPost(const std::string& url, const std::string& jsonBody, std::string& outResponse);

// Cryptography (using Apple Security.framework)
bool VerifyRSASHA256Signature(
    const std::string& data,
    const std::string& sigBase64,
    const uint8_t* modulus,
    size_t modulusLen,
    const uint8_t* exponent,
    size_t exponentLen
);

} // namespace MacPlatform

#endif // __APPLE__
