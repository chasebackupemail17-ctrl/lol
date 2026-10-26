#include "platform_mac.h"

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <ApplicationServices/ApplicationServices.h>
#import <IOKit/IOKitLib.h>
#import <Security/Security.h>
#import <CommonCrypto/CommonDigest.h>
#include <curl/curl.h>
#include <unistd.h>
#include <vector>
#include <iostream>

namespace MacPlatform {

std::string OpenFileDialog() {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
        [panel setAllowedFileTypes:@[@"png", @"jpg", @"jpeg", @"bmp", @"gif"]];
        [panel setTitle:@"Select Background Image"];

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = [[panel URLs] firstObject];
            if (url) {
                return std::string([[url path] UTF8String]);
            }
        }
    }
    return "";
}

void RevealInFinder(const std::string& path) {
    @autoreleasepool {
        NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
        [[NSWorkspace sharedWorkspace] selectFile:nsPath inFileViewerRootedAtPath:@""];
    }
}

std::string GetApplicationSupportDir() {
    @autoreleasepool {
        NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
        if ([paths count] > 0) {
            NSString* appSupport = [paths objectAtIndex:0];
            NSString* neuroDir = [appSupport stringByAppendingPathComponent:@"neuro"];
            [[NSFileManager defaultManager] createDirectoryAtPath:neuroDir withIntermediateDirectories:YES attributes:nil error:nil];
            return std::string([neuroDir UTF8String]) + "/";
        }
    }
    return "./";
}

bool CheckAccessibilityPermissions() {
    NSDictionary* options = @{(__bridge id)kAXTrustedCheckOptionPrompt: @YES};
    return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

void OpenAccessibilitySettings() {
    @autoreleasepool {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"]];
    }
}

static std::atomic<bool> s_global_keys[256];
static std::atomic<bool> s_global_mouse[8];
static std::atomic<bool> s_monitors_ready{ false };

void InitEventMonitors() {
    if (s_monitors_ready.exchange(true)) return;

    for (int i = 0; i < 256; i++) s_global_keys[i].store(false);
    for (int i = 0; i < 8; i++) s_global_mouse[i].store(false);

    @autoreleasepool {
        // Global monitors capture events delivered to background/other apps (like Roblox)
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^(NSEvent* event) {
            int code = (int)[event keyCode];
            if (code >= 0 && code < 256) s_global_keys[code].store(true);
        }];
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskKeyUp handler:^(NSEvent* event) {
            int code = (int)[event keyCode];
            if (code >= 0 && code < 256) s_global_keys[code].store(false);
        }];
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown handler:^(NSEvent* event) {
            s_global_mouse[0].store(true);
        }];
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskLeftMouseUp handler:^(NSEvent* event) {
            s_global_mouse[0].store(false);
        }];
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskRightMouseDown handler:^(NSEvent* event) {
            s_global_mouse[1].store(true);
        }];
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskRightMouseUp handler:^(NSEvent* event) {
            s_global_mouse[1].store(false);
        }];
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskOtherMouseDown handler:^(NSEvent* event) {
            NSInteger b = [event buttonNumber];
            if (b >= 0 && b < 8) s_global_mouse[b].store(true);
        }];
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskOtherMouseUp handler:^(NSEvent* event) {
            NSInteger b = [event buttonNumber];
            if (b >= 0 && b < 8) s_global_mouse[b].store(false);
        }];

        // Local monitors capture events when neuro window itself has focus
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent*(NSEvent* event) {
            int code = (int)[event keyCode];
            if (code >= 0 && code < 256) s_global_keys[code].store(true);
            return event;
        }];
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp handler:^NSEvent*(NSEvent* event) {
            int code = (int)[event keyCode];
            if (code >= 0 && code < 256) s_global_keys[code].store(false);
            return event;
        }];
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown handler:^NSEvent*(NSEvent* event) {
            s_global_mouse[0].store(true);
            return event;
        }];
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseUp handler:^NSEvent*(NSEvent* event) {
            s_global_mouse[0].store(false);
            return event;
        }];
    }
}

bool IsRobloxActive() {
    @autoreleasepool {
        NSRunningApplication* frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if (frontApp) {
            NSString* name = frontApp.localizedName;
            if (name && [name rangeOfString:@"Roblox" options:NSCaseInsensitiveSearch].location != NSNotFound) {
                return true;
            }
            NSString* bundleId = frontApp.bundleIdentifier;
            if (bundleId && [bundleId rangeOfString:@"roblox" options:NSCaseInsensitiveSearch].location != NSNotFound) {
                return true;
            }
        }
    }
    return false;
}

bool IsKeyDown(int macKeyCode) {
    if (macKeyCode < 0 || macKeyCode >= 256) return false;
    if (s_global_keys[macKeyCode].load()) return true;
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, (CGKeyCode)macKeyCode)) return true;
    if (CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, (CGKeyCode)macKeyCode)) return true;
    return false;
}

bool IsMouseButtonDown(int button) {
    if (button >= 0 && button < 8 && s_global_mouse[button].load()) return true;
    NSUInteger mouseButtons = [NSEvent pressedMouseButtons];
    if ((mouseButtons & (1 << button)) != 0) return true;
    CGMouseButton b = (button == 0) ? kCGMouseButtonLeft : (button == 1 ? kCGMouseButtonRight : (CGMouseButton)button);
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, b)) return true;
    if (CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, b)) return true;
    return false;
}

void SendKey(int macKeyCode, bool down) {
    if (macKeyCode < 0) return;
    CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
    CGEventRef ev = CGEventCreateKeyboardEvent(src, (CGKeyCode)macKeyCode, down);
    if (ev) {
        if (macKeyCode == kVK_Control) {
            CGEventSetFlags(ev, down ? kCGEventFlagMaskControl : 0);
        } else if (macKeyCode == kVK_Shift) {
            CGEventSetFlags(ev, down ? kCGEventFlagMaskShift : 0);
        } else if (macKeyCode == kVK_Option) {
            CGEventSetFlags(ev, down ? kCGEventFlagMaskAlternate : 0);
        } else if (macKeyCode == kVK_Command) {
            CGEventSetFlags(ev, down ? kCGEventFlagMaskCommand : 0);
        }
        // Post to both Session (active user app) and HID event tap
        CGEventPost(kCGSessionEventTap, ev);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }
    if (src) CFRelease(src);
}

void SendMouseClick() {
    CGEventRef ev = CGEventCreate(NULL);
    CGPoint curPos = CGPointZero;
    if (ev) {
        curPos = CGEventGetLocation(ev);
        CFRelease(ev);
    } else {
        NSPoint p = [NSEvent mouseLocation];
        NSScreen* screen = [NSScreen mainScreen];
        curPos = CGPointMake(p.x, screen ? screen.frame.size.height - p.y : p.y);
    }

    CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
    CGEventRef down = CGEventCreateMouseEvent(src, kCGEventLeftMouseDown, curPos, kCGMouseButtonLeft);
    CGEventRef up   = CGEventCreateMouseEvent(src, kCGEventLeftMouseUp, curPos, kCGMouseButtonLeft);

    if (down && up) {
        CGEventPost(kCGSessionEventTap, down);
        CGEventPost(kCGHIDEventTap, down);
        usleep(25000); // 25ms solid hold time so game engine registers the click
        CGEventPost(kCGSessionEventTap, up);
        CGEventPost(kCGHIDEventTap, up);
        CFRelease(down);
        CFRelease(up);
    }
    if (src) CFRelease(src);
}

int WindowsVkToMacKey(int vk) {
    if (vk >= 'a' && vk <= 'z') vk = toupper(vk);

    switch (vk) {
        case 0x1B: return kVK_Escape; // Escape
        case 0x0D: return kVK_Return; // Enter
        case 0x09: return kVK_Tab;    // Tab
        case 0x20: return kVK_Space;  // Space
        case 0x11: return kVK_Control;// Ctrl
        case 0x10: return kVK_Shift;  // Shift
        case 0x12: return kVK_Option; // Alt
        case 0x14: return kVK_CapsLock;
        case 0x26: return kVK_UpArrow;
        case 0x28: return kVK_DownArrow;
        case 0x25: return kVK_LeftArrow;
        case 0x27: return kVK_RightArrow;

        // Letters A-Z
        case 'A': return kVK_ANSI_A; case 'B': return kVK_ANSI_B; case 'C': return kVK_ANSI_C;
        case 'D': return kVK_ANSI_D; case 'E': return kVK_ANSI_E; case 'F': return kVK_ANSI_F;
        case 'G': return kVK_ANSI_G; case 'H': return kVK_ANSI_H; case 'I': return kVK_ANSI_I;
        case 'J': return kVK_ANSI_J; case 'K': return kVK_ANSI_K; case 'L': return kVK_ANSI_L;
        case 'M': return kVK_ANSI_M; case 'N': return kVK_ANSI_N; case 'O': return kVK_ANSI_O;
        case 'P': return kVK_ANSI_P; case 'Q': return kVK_ANSI_Q; case 'R': return kVK_ANSI_R;
        case 'S': return kVK_ANSI_S; case 'T': return kVK_ANSI_T; case 'U': return kVK_ANSI_U;
        case 'V': return kVK_ANSI_V; case 'W': return kVK_ANSI_W; case 'X': return kVK_ANSI_X;
        case 'Y': return kVK_ANSI_Y; case 'Z': return kVK_ANSI_Z;

        // Numbers 0-9
        case '0': return kVK_ANSI_0; case '1': return kVK_ANSI_1; case '2': return kVK_ANSI_2;
        case '3': return kVK_ANSI_3; case '4': return kVK_ANSI_4; case '5': return kVK_ANSI_5;
        case '6': return kVK_ANSI_6; case '7': return kVK_ANSI_7; case '8': return kVK_ANSI_8;
        case '9': return kVK_ANSI_9;

        // Function keys F1-F12
        case 0x70: return kVK_F1; case 0x71: return kVK_F2; case 0x72: return kVK_F3;
        case 0x73: return kVK_F4; case 0x74: return kVK_F5; case 0x75: return kVK_F6;
        case 0x76: return kVK_F7; case 0x77: return kVK_F8; case 0x78: return kVK_F9;
        case 0x79: return kVK_F10; case 0x7A: return kVK_F11; case 0x7B: return kVK_F12;

        // Mouse buttons (special negative codes for distinction)
        case 0x01: return -1; // LMB
        case 0x02: return -2; // RMB
        case 0x04: return -3; // MMB
        case 0x05: return -4; // X1
        case 0x06: return -5; // X2
        default: return -100;
    }
}

int MacKeyToWindowsVk(int macKey) {
    switch (macKey) {
        case kVK_Escape: return 0x1B;
        case kVK_Return: return 0x0D;
        case kVK_Tab:    return 0x09;
        case kVK_Space:  return 0x20;
        case kVK_Control:return 0x11;
        case kVK_Shift:  return 0x10;
        case kVK_Option: return 0x12;
        case kVK_CapsLock:return 0x14;
        case kVK_UpArrow: return 0x26;
        case kVK_DownArrow: return 0x28;
        case kVK_LeftArrow: return 0x25;
        case kVK_RightArrow: return 0x27;

        case kVK_ANSI_A: return 'A'; case kVK_ANSI_B: return 'B'; case kVK_ANSI_C: return 'C';
        case kVK_ANSI_D: return 'D'; case kVK_ANSI_E: return 'E'; case kVK_ANSI_F: return 'F';
        case kVK_ANSI_G: return 'G'; case kVK_ANSI_H: return 'H'; case kVK_ANSI_I: return 'I';
        case kVK_ANSI_J: return 'J'; case kVK_ANSI_K: return 'K'; case kVK_ANSI_L: return 'L';
        case kVK_ANSI_M: return 'M'; case kVK_ANSI_N: return 'N'; case kVK_ANSI_O: return 'O';
        case kVK_ANSI_P: return 'P'; case kVK_ANSI_Q: return 'Q'; case kVK_ANSI_R: return 'R';
        case kVK_ANSI_S: return 'S'; case kVK_ANSI_T: return 'T'; case kVK_ANSI_U: return 'U';
        case kVK_ANSI_V: return 'V'; case kVK_ANSI_W: return 'W'; case kVK_ANSI_X: return 'X';
        case kVK_ANSI_Y: return 'Y'; case kVK_ANSI_Z: return 'Z';

        case kVK_ANSI_0: return '0'; case kVK_ANSI_1: return '1'; case kVK_ANSI_2: return '2';
        case kVK_ANSI_3: return '3'; case kVK_ANSI_4: return '4'; case kVK_ANSI_5: return '5';
        case kVK_ANSI_6: return '6'; case kVK_ANSI_7: return '7'; case kVK_ANSI_8: return '8';
        case kVK_ANSI_9: return '9';

        default: return 0;
    }
}

std::string GetMacKeyName(int macKey) {
    if (macKey == -1) return "Mouse1 / LMB";
    if (macKey == -2) return "Mouse2 / RMB";
    if (macKey == -3) return "Mouse3 / MMB";

    switch (macKey) {
        case kVK_Escape: return "Escape";
        case kVK_Return: return "Return";
        case kVK_Tab:    return "Tab";
        case kVK_Space:  return "Space";
        case kVK_Control:return "Control";
        case kVK_Shift:  return "Shift";
        case kVK_Option: return "Option";
        case kVK_Command:return "Command";
        case kVK_CapsLock:return "CapsLock";
        case kVK_UpArrow: return "Up Arrow";
        case kVK_DownArrow: return "Down Arrow";
        case kVK_LeftArrow: return "Left Arrow";
        case kVK_RightArrow: return "Right Arrow";
        case kVK_ANSI_Q: return "Q";
        case kVK_ANSI_Z: return "Z";
        case kVK_ANSI_P: return "P";
        case kVK_ANSI_R: return "R";
        default: break;
    }
    int vk = MacKeyToWindowsVk(macKey);
    if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
    if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
    return "Key " + std::to_string(macKey);
}

std::string GetMacHWID() {
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFTypeRef uuidAsCFString = IORegistryEntryCreateCFProperty(platformExpert, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
        if (uuidAsCFString) {
            char buf[128] = {0};
            CFStringGetCString((CFStringRef)uuidAsCFString, buf, sizeof(buf), kCFStringEncodingUTF8);
            CFRelease(uuidAsCFString);
            IOObjectRelease(platformExpert);
            return std::string(buf);
        }
        IOObjectRelease(platformExpert);
    }
    return "unknown-mac-hwid";
}

std::string GetMacUsername() {
    const char* user = getenv("USER");
    if (user && strlen(user) > 0) return std::string(user);
    char buf[128];
    if (getlogin_r(buf, sizeof(buf)) == 0) return std::string(buf);
    return "Unknown";
}

static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total);
    return total;
}

bool HttpGet(const std::string& url, std::string& outResponse) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    outResponse.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outResponse);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Neuro/1.0");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

bool HttpPost(const std::string& url, const std::string& jsonBody, std::string& outResponse) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    outResponse.clear();
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outResponse);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Neuro/1.0");

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

bool VerifyRSASHA256Signature(
    const std::string& data,
    const std::string& sigBase64,
    const uint8_t* modulus,
    size_t modulusLen,
    const uint8_t* exponent,
    size_t exponentLen
) {
    @autoreleasepool {
        // Construct PKCS#1 RSAPublicKey ASN.1 DER sequence
        // SEQUENCE {
        //   INTEGER modulus
        //   INTEGER exponent
        // }
        bool needZero = (modulusLen > 0 && (modulus[0] & 0x80) != 0);
        size_t modIntLen = modulusLen + (needZero ? 1 : 0);

        std::vector<uint8_t> der;
        der.push_back(0x30); // SEQUENCE
        size_t innerLen = 1 + 3 + modIntLen + 1 + 1 + exponentLen; // approximate
        // Exact length calculation:
        size_t modHeaderLen = (modIntLen >= 128) ? 4 : 2;
        size_t expHeaderLen = 2;
        size_t totalInner = modHeaderLen + modIntLen + expHeaderLen + exponentLen;

        der.clear();
        der.push_back(0x30);
        if (totalInner >= 128) {
            der.push_back(0x82);
            der.push_back((totalInner >> 8) & 0xFF);
            der.push_back(totalInner & 0xFF);
        } else {
            der.push_back((uint8_t)totalInner);
        }

        // Modulus
        der.push_back(0x02); // INTEGER
        if (modIntLen >= 128) {
            der.push_back(0x82);
            der.push_back((modIntLen >> 8) & 0xFF);
            der.push_back(modIntLen & 0xFF);
        } else {
            der.push_back((uint8_t)modIntLen);
        }
        if (needZero) der.push_back(0x00);
        der.insert(der.end(), modulus, modulus + modulusLen);

        // Exponent
        der.push_back(0x02); // INTEGER
        der.push_back((uint8_t)exponentLen);
        der.insert(der.end(), exponent, exponent + exponentLen);

        CFDataRef derData = CFDataCreate(kCFAllocatorDefault, der.data(), (CFIndex)der.size());
        if (!derData) return false;

        NSDictionary* keyAttrs = @{
            (__bridge id)kSecAttrKeyType: (__bridge id)kSecAttrKeyTypeRSA,
            (__bridge id)kSecAttrKeyClass: (__bridge id)kSecAttrKeyClassPublic,
            (__bridge id)kSecAttrKeySizeInBits: @(modulusLen * 8)
        };

        CFErrorRef error = NULL;
        SecKeyRef secKey = SecKeyCreateWithData(derData, (__bridge CFDictionaryRef)keyAttrs, &error);
        CFRelease(derData);
        if (!secKey) return false;

        // Base64 decode signature
        NSString* nsSig = [NSString stringWithUTF8String:sigBase64.c_str()];
        NSData* sigData = [[NSData alloc] initWithBase64EncodedString:nsSig options:NSDataBase64DecodingIgnoreUnknownCharacters];
        if (!sigData) {
            CFRelease(secKey);
            return false;
        }

        // SHA-256 hash of data
        unsigned char digest[CC_SHA256_DIGEST_LENGTH];
        CC_SHA256(data.data(), (CC_LONG)data.size(), digest);
        CFDataRef digestData = CFDataCreate(kCFAllocatorDefault, digest, CC_SHA256_DIGEST_LENGTH);

        Boolean verified = SecKeyVerifySignature(
            secKey,
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
            digestData,
            (__bridge CFDataRef)sigData,
            &error
        );

        CFRelease(digestData);
        CFRelease(secKey);
        return verified == true;
    }
}

} // namespace MacPlatform

#endif // __APPLE__
