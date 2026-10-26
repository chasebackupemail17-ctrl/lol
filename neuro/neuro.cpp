#include "platform_mac.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <Carbon/Carbon.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <algorithm>
#include <thread>
#include <fstream>
#include <vector>
#include <cmath>
#include <sstream>
#include <filesystem>
#include <chrono>
#include "util/skCrypter.h"
#include "util/auth.h"

namespace fs = std::filesystem;

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#define Sleep(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
inline uint32_t GetTickCount() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
#define strcpy_s(dst, src) do { strncpy((dst), (src), sizeof(dst) - 1); (dst)[sizeof(dst) - 1] = '\0'; } while(0)

#define VK_LBUTTON  0x01
#define VK_RBUTTON  0x02
#define VK_MBUTTON  0x04
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#define VK_ESCAPE   0x1B
#define VK_RETURN   0x0D
#define VK_TAB      0x09
#define VK_SPACE    0x20
#define VK_CONTROL  0x11
#define VK_SHIFT    0x10
#define VK_MENU     0x12
#define VK_CAPITAL  0x14
#define VK_UP       0x26
#define VK_DOWN     0x28
#define VK_LEFT     0x25
#define VK_RIGHT    0x27
#define VK_NUMPAD0  0x60
#define VK_NUMPAD9  0x69
#define VK_F1       0x70
#define VK_F24      0x87

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#pragma warning(push)
#pragma warning(disable: 4244 4996 26451)
#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb_image.h"
#pragma warning(pop)


int shotgun = 4;
int spray = 5;
int katana = 3;
int macrokey = 0x51;
int resetkey = 0x5A;
int autoclickkey = 0x50;
bool macroheld = false;
bool resetheld = false;
bool autoclick = false;
bool reload = true;
int heldduration = 0;
int autoclicksleep = 1;
int macrospeed = 375;
int actionsleep = 35;
int reloadsleep = 33;
int resetsleep = 50;
int resetduration = 5000;

bool crouch_macro = true;
bool pull_out = true;

bool enabled = true;

bool double_shoot_enabled = false;
int double_shoot_delay = 185;

struct preset {
    std::string name;
    int macrospeed;
    int actionsleep;
    int reloadsleep;
    int resetsleep;
    int resetduration;
    bool double_shoot_enabled;
    int double_shoot_delay;
};

std::vector<preset> presets;
int activepreset = -1;

std::string GetPresetsDir() {
    std::string dir = MacPlatform::GetApplicationSupportDir() + "presets/";
    try {
        fs::create_directories(dir);
    } catch (...) {}
    return dir;
}

void SaveSinglePreset(const std::string& dir, const preset& pr) {
    std::string filepath = dir + pr.name + ".json";
    std::ofstream out(filepath);
    if (!out.is_open()) return;
    out << "{\n";
    out << "  \"macrospeed\": " << pr.macrospeed << ",\n";
    out << "  \"actionsleep\": " << pr.actionsleep << ",\n";
    out << "  \"reloadsleep\": " << pr.reloadsleep << ",\n";
    out << "  \"resetsleep\": " << pr.resetsleep << ",\n";
    out << "  \"resetduration\": " << pr.resetduration << ",\n";
    out << "  \"double_shoot_enabled\": " << (pr.double_shoot_enabled ? "true" : "false") << ",\n";
    out << "  \"double_shoot_delay\": " << pr.double_shoot_delay << "\n";
    out << "}\n";
}

bool ExtractJsonVal(const std::string& content, const std::string& key, std::string& out_val) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return false;
    size_t colon = content.find(":", pos);
    if (colon == std::string::npos) return false;
    size_t start = colon + 1;
    while (start < content.size() && (content[start] == ' ' || content[start] == '\t' || content[start] == '\r' || content[start] == '\n')) {
        start++;
    }
    size_t end = start;
    while (end < content.size() && content[end] != ',' && content[end] != '}' && content[end] != '\r' && content[end] != '\n') {
        end++;
    }
    out_val = content.substr(start, end - start);
    while (!out_val.empty() && (out_val.back() == ' ' || out_val.back() == '\t' || out_val.back() == '\r' || out_val.back() == '\n')) {
        out_val.pop_back();
    }
    return true;
}

bool LoadSinglePreset(const std::string& filepath, const std::string& name, preset& out_pr) {
    std::ifstream in(filepath);
    if (!in.is_open()) return false;
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string content = buffer.str();

    out_pr.name = name;
    out_pr.macrospeed = 560;
    out_pr.actionsleep = 10;
    out_pr.reloadsleep = 33;
    out_pr.resetsleep = 50;
    out_pr.resetduration = 5000;
    out_pr.double_shoot_enabled = false;
    out_pr.double_shoot_delay = 185;

    std::string val;
    if (ExtractJsonVal(content, "macrospeed", val)) {
        try { out_pr.macrospeed = std::stoi(val); } catch (...) {}
    }
    if (ExtractJsonVal(content, "actionsleep", val)) {
        try { out_pr.actionsleep = std::stoi(val); } catch (...) {}
    }
    if (ExtractJsonVal(content, "reloadsleep", val)) {
        try { out_pr.reloadsleep = std::stoi(val); } catch (...) {}
    }
    if (ExtractJsonVal(content, "resetsleep", val)) {
        try { out_pr.resetsleep = std::stoi(val); } catch (...) {}
    }
    if (ExtractJsonVal(content, "resetduration", val)) {
        try { out_pr.resetduration = std::stoi(val); } catch (...) {}
    }
    if (ExtractJsonVal(content, "double_shoot_enabled", val)) {
        out_pr.double_shoot_enabled = (val == "true" || val == "1");
    }
    if (ExtractJsonVal(content, "double_shoot_delay", val)) {
        try { out_pr.double_shoot_delay = std::stoi(val); } catch (...) {}
    }
    return true;
}

void WriteDefaultPresets(const std::string& dir) {
    std::vector<preset> defaults = {
        { "SG",       560, 10, 33, 50, 5000, false, 185 },
        { "DB",       817, 10, 33, 50, 5000, false,  185 },
        { "Tac",      480, 10, 33, 50, 5000, false, 185 },
        { "SG (ST)",  900, 35, 33, 50, 5000, true,  635 },
        { "DB (ST)",  761, 35, 33, 50, 5000, true,  325 },
        { "Tac (ST)", 875, 32, 33, 50, 5000, true,  530 }
    };
    for (const auto& pr : defaults) {
        SaveSinglePreset(dir, pr);
    }
}

void DeletePresetFile(const std::string& name) {
    std::string dir = GetPresetsDir();
    std::string filepath = dir + name + ".json";
    try {
        fs::remove(filepath);
    } catch (...) {}
}

void RenamePresetFile(const std::string& old_name, const std::string& new_name) {
    std::string dir = GetPresetsDir();
    std::string old_path = dir + old_name + ".json";
    std::string new_path = dir + new_name + ".json";
    try {
        if (fs::exists(old_path)) {
            fs::rename(old_path, new_path);
        }
    } catch (...) {}
}

void LoadPresets() {
    presets.clear();
    std::string dir = MacPlatform::GetApplicationSupportDir();
    std::string old_filename = dir + "neuro_presets.ini";
    std::string presets_dir = dir + "presets/";

    std::ifstream in(old_filename);
    if (in.is_open()) {
        try { fs::create_directories(presets_dir); } catch (...) {}
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string name = line.substr(0, eq);
            std::string valStr = line.substr(eq + 1);

            std::stringstream ss(valStr);
            std::string token;
            std::vector<int> vals;
            while (std::getline(ss, token, ',')) {
                try {
                    vals.push_back(std::stoi(token));
                }
                catch (...) {}
            }
            if (vals.size() >= 5) {
                bool ds_en = false;
                int ds_del = 185;
                if (vals.size() >= 7) {
                    ds_en = (vals[5] == 1);
                    ds_del = vals[6];
                }
                preset migrated_pr = { name, vals[0], vals[1], vals[2], vals[3], vals[4], ds_en, ds_del };
                SaveSinglePreset(presets_dir, migrated_pr);
            }
        }
        in.close();
        try { fs::remove(old_filename); } catch (...) {}
    }

    try { fs::create_directories(presets_dir); } catch (...) {}
    WriteDefaultPresets(presets_dir);

    std::vector<std::string> ordered_defaults = {
        "SG", "DB", "Tac", "SG (ST)", "DB (ST)", "Tac (ST)"
    };
    for (const auto& name : ordered_defaults) {
        std::string path = presets_dir + name + ".json";
        preset pr;
        if (LoadSinglePreset(path, name, pr)) {
            presets.push_back(pr);
        }
    }

    try {
        for (const auto& entry : fs::directory_iterator(presets_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string stem = entry.path().stem().string();
                if (std::find(ordered_defaults.begin(), ordered_defaults.end(), stem) == ordered_defaults.end()) {
                    preset pr;
                    if (LoadSinglePreset(entry.path().string(), stem, pr)) {
                        presets.push_back(pr);
                    }
                }
            }
        }
    } catch (...) {}
}

void SavePresets() {
    std::string dir = GetPresetsDir();
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                fs::remove(entry.path());
            }
        }
    } catch (...) {}
    for (const auto& pr : presets) {
        SaveSinglePreset(dir, pr);
    }
}

volatile bool lmbheld = false;

void* window_handle = nullptr;
static GLFWwindow* glfw_window = nullptr;
static GLuint bg_texture_id = 0;
static std::vector<GLuint> bg_gif_texture_ids;

enum AppState {
    STATE_LOGIN,
    STATE_LOGGING_IN,
    STATE_SETTINGS
};
AppState app_state = STATE_LOGIN;
std::string auth_error = "";
char Username[128] = "";
char Password[128] = "";
bool RememberMe = true;

bool bindingmacrokey = false;
bool bindingresetkey = false;
bool bindingautoclickkey = false;
bool bindingshotgunslot = false;
bool bindingsprayslot = false;
bool bindingkatanaslot = false;
bool exiting = false;
bool robloxactive = false;

bool ui_stars_enabled = true;
bool ui_streamproof = false;
bool ui_glow_enabled = true;
bool ui_glow_gradient = false;
float ui_glow_top[4] = { 0.80f, 0.20f, 0.20f, 1.00f };
float ui_glow_bottom[4] = { 0.20f, 0.20f, 0.80f, 1.00f };
float ui_bg_color[4] = { 0.067f, 0.067f, 0.082f, 1.00f };

float ui_accent[4] = { 0.38f, 0.48f, 1.00f, 1.00f };
float ui_accent2[4] = { 0.22f, 0.28f, 0.55f, 1.00f };
bool ui_rainbow = false;
float ui_rainbow_speed = 0.5f;
bool ui_trail_enabled = false;
bool ui_ripples_enabled = false;
std::string ui_bg_path = "";

static std::vector<int> bg_gif_delays;
static int bg_gif_current_frame = 0;
static float bg_gif_timer = 0.0f;
static int bg_width = 0, bg_height = 0;
static bool bg_is_gif = false;

struct CursorParticle {
    ImVec2 pos;
    float alpha;
    float size;
};
std::vector<CursorParticle> cursor_trail;

struct Ripple {
    ImVec2 pos;
    float radius;
    float alpha;
};
std::vector<Ripple> click_ripples;


static ImFont* font_bold = nullptr;

inline bool IsKeyOrMouseDown(int vk) {
    if (vk == 0x01) return MacPlatform::IsMouseButtonDown(0);
    if (vk == 0x02) return MacPlatform::IsMouseButtonDown(1);
    if (vk == 0x04) return MacPlatform::IsMouseButtonDown(2);
    int macKey = MacPlatform::WindowsVkToMacKey(vk);
    if (macKey >= 0) return MacPlatform::IsKeyDown(macKey);
    return false;
}

std::string GetKeyName(int vk) {
    if (vk == 0) return "None";
    if (vk == VK_LBUTTON) return "Mouse1 / LMB";
    if (vk == VK_RBUTTON) return "Mouse2 / RMB";
    if (vk == VK_MBUTTON) return "Mouse3 / MMB";
    if (vk == VK_XBUTTON1) return "Mouse4 / X1";
    if (vk == VK_XBUTTON2) return "Mouse5 / X2";
    if (vk == VK_SPACE) return "Space";
    if (vk == VK_TAB) return "Tab";
    if (vk == VK_RETURN) return "Enter";
    if (vk == VK_ESCAPE) return "Escape";
    if (vk == VK_SHIFT) return "Shift";
    if (vk == VK_CONTROL) return "Control";
    if (vk == VK_MENU) return "Alt";
    if (vk == VK_CAPITAL) return "CapsLock";
    if (vk == VK_UP) return "Up Arrow";
    if (vk == VK_DOWN) return "Down Arrow";
    if (vk == VK_LEFT) return "Left Arrow";
    if (vk == VK_RIGHT) return "Right Arrow";
    if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
    if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
    if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);

    int macKey = MacPlatform::WindowsVkToMacKey(vk);
    if (macKey >= 0 || macKey <= -1) {
        return MacPlatform::GetMacKeyName(macKey);
    }
    return "Key " + std::to_string(vk);
}

void LoadSettings() {
    std::string filename = MacPlatform::GetApplicationSupportDir() + "neuro_settings.ini";

    std::ifstream in(filename);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        try {
            if (key == "shotgun") shotgun = std::stoi(val);
            else if (key == "spray") spray = std::stoi(val);
            else if (key == "katana") katana = std::stoi(val);
            else if (key == "macrokey") macrokey = std::stoi(val);
            else if (key == "resetkey") resetkey = std::stoi(val);
            else if (key == "autoclickkey") autoclickkey = std::stoi(val);
            else if (key == "autoclick") autoclick = (val == "1");
            else if (key == "reload") reload = (val == "1");
            else if (key == "autoclicksleep") autoclicksleep = std::stoi(val);
            else if (key == "macrospeed") macrospeed = std::stoi(val);
            else if (key == "actionsleep") actionsleep = std::stoi(val);
            else if (key == "reloadsleep") reloadsleep = std::stoi(val);
            else if (key == "resetsleep") resetsleep = std::stoi(val);
            else if (key == "resetduration") resetduration = std::stoi(val);
            else if (key == "enabled") enabled = (val == "1");
            else if (key == "double_shoot_enabled") double_shoot_enabled = (val == "1");
            else if (key == "double_shoot_delay") double_shoot_delay = std::stoi(val);
            else if (key == "ui_stars") ui_stars_enabled = (val == "1");
            else if (key == "ui_streamproof") ui_streamproof = (val == "1");
            else if (key == "ui_glow") ui_glow_enabled = (val == "1");
            else if (key == "ui_accent") {
                std::stringstream css(val); std::string tok; int ci = 0;
                while (std::getline(css, tok, ',') && ci < 4) { try { ui_accent[ci++] = std::stof(tok); } catch (...) {} }
            }
            else if (key == "ui_accent2") {
                std::stringstream css(val); std::string tok; int ci = 0;
                while (std::getline(css, tok, ',') && ci < 4) { try { ui_accent2[ci++] = std::stof(tok); } catch (...) {} }
            }
            else if (key == "ui_rainbow") ui_rainbow = (val == "1");
            else if (key == "ui_rainbow_speed") ui_rainbow_speed = std::stof(val);
            else if (key == "ui_trail") ui_trail_enabled = (val == "1");
            else if (key == "ui_ripples") ui_ripples_enabled = (val == "1");
            else if (key == "ui_bg_path") ui_bg_path = val;
            else if (key == "ui_glow_gradient") ui_glow_gradient = (val == "1");
            else if (key == "ui_glow_top") {
                std::stringstream css(val); std::string tok; int ci = 0;
                while (std::getline(css, tok, ',') && ci < 4) { try { ui_glow_top[ci++] = std::stof(tok); } catch (...) {} }
            }
            else if (key == "ui_glow_bottom") {
                std::stringstream css(val); std::string tok; int ci = 0;
                while (std::getline(css, tok, ',') && ci < 4) { try { ui_glow_bottom[ci++] = std::stof(tok); } catch (...) {} }
            }
            else if (key == "ui_bg_color") {
                std::stringstream css(val); std::string tok; int ci = 0;
                while (std::getline(css, tok, ',') && ci < 4) { try { ui_bg_color[ci++] = std::stof(tok); } catch (...) {} }
            }
            else if (key == "reload") reload = (val == "1");
            else if (key == "crouch_macro") crouch_macro = (val == "1");
            else if (key == "pull_out") pull_out = (val == "1");
        }
        catch (...) {}
    }
}

void SaveSettings() {
    std::string filename = MacPlatform::GetApplicationSupportDir() + "neuro_settings.ini";

    std::ofstream out(filename);
    if (!out.is_open()) return;
    out << "shotgun=" << shotgun << "\n";
    out << "spray=" << spray << "\n";
    out << "katana=" << katana << "\n";
    out << "macrokey=" << macrokey << "\n";
    out << "resetkey=" << resetkey << "\n";
    out << "autoclickkey=" << autoclickkey << "\n";
    out << "autoclick=" << (autoclick ? "1" : "0") << "\n";
    out << "reload=" << (reload ? "1" : "0") << "\n";
    out << "autoclicksleep=" << autoclicksleep << "\n";
    out << "macrospeed=" << macrospeed << "\n";
    out << "actionsleep=" << actionsleep << "\n";
    out << "reloadsleep=" << reloadsleep << "\n";
    out << "resetsleep=" << resetsleep << "\n";
    out << "resetduration=" << resetduration << "\n";
    out << "enabled=" << (enabled ? "1" : "0") << "\n";
    out << "double_shoot_enabled=" << (double_shoot_enabled ? "1" : "0") << "\n";
    out << "double_shoot_delay=" << double_shoot_delay << "\n";
    out << "ui_stars=" << (ui_stars_enabled ? "1" : "0") << "\n";
    out << "ui_streamproof=" << (ui_streamproof ? "1" : "0") << "\n";
    out << "ui_glow=" << (ui_glow_enabled ? "1" : "0") << "\n";
    out << "ui_accent=" << ui_accent[0] << "," << ui_accent[1] << "," << ui_accent[2] << "," << ui_accent[3] << "\n";
    out << "ui_accent2=" << ui_accent2[0] << "," << ui_accent2[1] << "," << ui_accent2[2] << "," << ui_accent2[3] << "\n";
    out << "ui_rainbow=" << (ui_rainbow ? "1" : "0") << "\n";
    out << "ui_rainbow_speed=" << ui_rainbow_speed << "\n";
    out << "ui_trail=" << (ui_trail_enabled ? "1" : "0") << "\n";
    out << "ui_ripples=" << (ui_ripples_enabled ? "1" : "0") << "\n";
    out << "ui_bg_path=" << ui_bg_path << "\n";
    out << "ui_glow_gradient=" << (ui_glow_gradient ? "1" : "0") << "\n";
    out << "ui_glow_top=" << ui_glow_top[0] << "," << ui_glow_top[1] << "," << ui_glow_top[2] << "," << ui_glow_top[3] << "\n";
    out << "ui_glow_bottom=" << ui_glow_bottom[0] << "," << ui_glow_bottom[1] << "," << ui_glow_bottom[2] << "," << ui_glow_bottom[3] << "\n";
    out << "ui_bg_color=" << ui_bg_color[0] << "," << ui_bg_color[1] << "," << ui_bg_color[2] << "," << ui_bg_color[3] << "\n";
    out << "reload=" << (reload ? "1" : "0") << "\n";
    out << "crouch_macro=" << (crouch_macro ? "1" : "0") << "\n";
    out << "pull_out=" << (pull_out ? "1" : "0") << "\n";
}

GLuint CreateGLTextureFromRGBA(unsigned char* data, int w, int h) {
    if (!data) return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return tex;
}

void ClearBGTextures() {
    if (bg_texture_id != 0) { glDeleteTextures(1, &bg_texture_id); bg_texture_id = 0; }
    for (auto tid : bg_gif_texture_ids) {
        if (tid != 0) glDeleteTextures(1, &tid);
    }
    bg_gif_texture_ids.clear();
    bg_gif_delays.clear();
    bg_gif_current_frame = 0;
    bg_gif_timer = 0.0f;
    bg_is_gif = false;
    bg_width = 0;
    bg_height = 0;
}

bool LoadBGImage(const std::string& path) {
    ClearBGTextures();
    if (path.empty()) return false;

    std::ifstream test(path, std::ios::binary);
    if (!test.good()) return false;
    test.close();

    std::string ext;
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = path.substr(dotPos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == "gif") {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        size_t fileSize = (size_t)file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> buffer(fileSize);
        if (!file.read((char*)buffer.data(), fileSize)) return false;
        file.close();

        int* delays = nullptr;
        int x = 0, y = 0, frames = 0, comp = 0;
        unsigned char* data = stbi_load_gif_from_memory(buffer.data(), (int)fileSize, &delays, &x, &y, &frames, &comp, 4);
        if (!data) return false;

        bg_width = x;
        bg_height = y;
        bg_is_gif = true;

        int frameSize = x * y * 4;
        for (int i = 0; i < frames; i++) {
            GLuint tex = CreateGLTextureFromRGBA(data + i * frameSize, x, y);
            if (tex != 0) {
                bg_gif_texture_ids.push_back(tex);
                bg_gif_delays.push_back(delays ? (delays[i] > 0 ? delays[i] : 100) : 100);
            }
        }

        stbi_image_free(data);
        if (delays) stbi_image_free(delays);

        if (bg_gif_texture_ids.empty()) {
            bg_is_gif = false;
            return false;
        }
        return true;
    }
    else {
        int w = 0, h = 0, comp = 0;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
        if (!data) return false;

        bg_width = w;
        bg_height = h;
        bg_texture_id = CreateGLTextureFromRGBA(data, w, h);
        bool ok = (bg_texture_id != 0);
        stbi_image_free(data);
        return ok;
    }
}

void OpenBGFilePicker() {
    std::string filename = MacPlatform::OpenFileDialog();
    if (!filename.empty()) {
        if (LoadBGImage(filename)) {
            ui_bg_path = filename;
            SaveSettings();
        }
    }
}

void key(int vk) {
    int macKey = MacPlatform::WindowsVkToMacKey(vk);
    if (macKey >= 0) {
        MacPlatform::SendKey(macKey, true);
        Sleep(15);
        MacPlatform::SendKey(macKey, false);
    }
}

void fastkey(int vk, uint16_t scan) {
    int macKey = MacPlatform::WindowsVkToMacKey(vk);
    if (macKey >= 0) {
        MacPlatform::SendKey(macKey, true);
        MacPlatform::SendKey(macKey, false);
    }
}

void click() {
    MacPlatform::SendMouseClick();
}

void crouch(bool down) {
    int macKey = MacPlatform::WindowsVkToMacKey(VK_CONTROL);
    if (macKey >= 0) {
        MacPlatform::SendKey(macKey, down);
    }
}

int slot(int n) {
    return 0x30 + n;
}

void* MacroThread(void* lpParam) {
    DWORD lastAutoclickTime = 0;

    while (true) {
        if (app_state != STATE_SETTINGS || !enabled || g_auth_killed.load()) {
            macroheld = false;
            resetheld = false;
            heldduration = 0;
            Sleep(50);
            continue;
        }

        robloxactive = MacPlatform::IsRobloxActive();
        lmbheld = MacPlatform::IsMouseButtonDown(0);

        if (!robloxactive) {
            macroheld = false;
            resetheld = false;
            heldduration = 0;
            lmbheld = false;
            Sleep(50);
            continue;
        }

        if (autoclick) {
            if (lmbheld) {
                DWORD now = GetTickCount();
                if (now - lastAutoclickTime >= (DWORD)autoclicksleep) {
                    click();
                    lastAutoclickTime = now;
                }
            }
        }

        static bool autoclickheld = false;
        bool autoclickdown = false;
        if (autoclickkey != 0) {
            autoclickdown = IsKeyOrMouseDown(autoclickkey);
        }

        if (autoclickdown && !autoclickheld) {
            autoclick = !autoclick;
            SaveSettings();
        }

        autoclickheld = autoclickdown;

        bool resetdown = false;
        if (resetkey != 0) {
            resetdown = IsKeyOrMouseDown(resetkey);
        }

        if (resetdown && !resetheld) {
            key(VK_ESCAPE);
            Sleep(resetsleep);
            key('R');
            Sleep(resetsleep);
            key(VK_RETURN);
            Sleep(resetsleep);

            int kvk = slot(katana);
            uint16_t kscan = 0;
            DWORD starttime = GetTickCount();

            while (GetTickCount() - starttime < (DWORD)resetduration) {
                if (app_state != STATE_SETTINGS || !enabled)
                    break;

                if (!MacPlatform::IsRobloxActive())
                    break;

                fastkey(kvk, kscan);
                Sleep(1);
            }
        }

        resetheld = resetdown;

        bool down = false;
        if (macrokey != 0) {
            down = IsKeyOrMouseDown(macrokey);
        }

        if (down) {
            if (!macroheld || heldduration >= macrospeed) {

                if (pull_out)
                    key(slot(shotgun));

                Sleep(actionsleep);

                click();
                Sleep(actionsleep);

                if (double_shoot_enabled) {

                    if (crouch_macro)
                        crouch(true);

                    Sleep(actionsleep);

                    if (pull_out)
                        key(slot(spray));

                    Sleep(actionsleep);

                    if (crouch_macro)
                        crouch(false);

                    Sleep(actionsleep);

                    Sleep(double_shoot_delay);

                    if (pull_out)
                        key(slot(shotgun));

                    Sleep(actionsleep);

                    click();
                    Sleep(actionsleep);

                    if (reload) {
                        key('R');
                        Sleep(reloadsleep);
                    }

                    if (crouch_macro)
                        crouch(true);

                    Sleep(actionsleep);

                    if (pull_out)
                        key(slot(spray));

                    Sleep(actionsleep);

                    if (crouch_macro)
                        crouch(false);
                }
                else {

                    if (reload) {
                        key('R');
                        Sleep(reloadsleep);
                    }

                    if (crouch_macro)
                        crouch(true);

                    Sleep(actionsleep);

                    if (pull_out)
                        key(slot(spray));

                    Sleep(actionsleep);

                    if (crouch_macro)
                        crouch(false);
                }

                Sleep(actionsleep);
                heldduration = 0;
            }

            heldduration += 10;
        }
        else {
            heldduration = 0;
        }

        macroheld = down;
        Sleep(10);
    }

    return nullptr;
}

void DoLoginThread() {
    app_state = STATE_LOGGING_IN;
    std::string msg;
    bool success = Auth::VerifyCredentials(Username, Password, msg);
    if (success) {
        app_state = STATE_SETTINGS;
        auth_error = "";
        Webhook::LaunchAuthEventThread();
        Auth::LaunchHeartbeatThread();
    }
    else {
        auth_error = msg;
        app_state = STATE_LOGIN;
        AuthGlobals::SavedUsername.clear();
        AuthGlobals::SavedPassword.clear();
        AuthSettings::SaveSettings();
    }
}

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.TabRounding = 6.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 0.0f;

    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 10.0f;

    const ImVec4 accent = ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]);
    const ImVec4 accentDim = ImVec4(ui_accent[0] * 0.75f, ui_accent[1] * 0.75f, ui_accent[2] * 0.75f, ui_accent[3]);
    const ImVec4 accentHover = ImVec4(min(ui_accent[0] * 1.2f, 1.0f), min(ui_accent[1] * 1.2f, 1.0f), min(ui_accent[2] * 1.2f, 1.0f), ui_accent[3]);

    const ImVec4 tabActive = ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]);
    const ImVec4 tabHover = ImVec4(ui_accent2[0] * 0.9f, ui_accent2[1] * 0.9f, ui_accent2[2] * 0.9f, 0.8f);

    const ImVec4 surface0 = ImVec4(0.067f, 0.067f, 0.082f, 1.00f);
    const ImVec4 surface1 = ImVec4(0.090f, 0.090f, 0.110f, 1.00f);
    const ImVec4 surface2 = ImVec4(0.120f, 0.120f, 0.145f, 1.00f);
    const ImVec4 surface3 = ImVec4(0.155f, 0.155f, 0.185f, 1.00f);

    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);

    colors[ImGuiCol_WindowBg] = ImVec4(ui_bg_color[0], ui_bg_color[1], ui_bg_color[2], ui_bg_color[3]);
    colors[ImGuiCol_ChildBg] = surface1;
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.13f, 0.96f);

    colors[ImGuiCol_Border] = ImVec4(0.16f, 0.16f, 0.20f, 0.60f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg] = surface2;
    colors[ImGuiCol_FrameBgHovered] = surface3;
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);

    colors[ImGuiCol_TitleBg] = surface0;
    colors[ImGuiCol_TitleBgActive] = surface0;
    colors[ImGuiCol_TitleBgCollapsed] = surface0;

    colors[ImGuiCol_MenuBarBg] = surface1;

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.38f, 0.45f, 1.00f);

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accentDim;
    colors[ImGuiCol_SliderGrabActive] = accent;

    colors[ImGuiCol_Button] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.16f, 0.16f, 0.20f, 0.60f);
    colors[ImGuiCol_HeaderHovered] = tabHover;
    colors[ImGuiCol_HeaderActive] = tabActive;

    colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.16f, 0.20f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = accentDim;
    colors[ImGuiCol_SeparatorActive] = accent;

    colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_TabHovered] = tabHover;
    colors[ImGuiCol_TabActive] = tabActive;
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(tabActive.x * 0.6f, tabActive.y * 0.6f, tabActive.z * 0.6f, 1.00f);
}

void ProcessKeyBinding(int vk) {
    if (vk == 0) return;
    if (vk != VK_ESCAPE) {
        if (bindingmacrokey) {
            if (resetkey == vk) resetkey = 0;
            if (autoclickkey == vk) autoclickkey = 0;
            macrokey = vk;
            bindingmacrokey = false;
            SaveSettings();
        }
        else if (bindingresetkey) {
            if (macrokey == vk) macrokey = 0;
            if (autoclickkey == vk) autoclickkey = 0;
            resetkey = vk;
            bindingresetkey = false;
            SaveSettings();
        }
        else if (bindingautoclickkey) {
            if (macrokey == vk) macrokey = 0;
            if (resetkey == vk) resetkey = 0;
            autoclickkey = vk;
            bindingautoclickkey = false;
            SaveSettings();
        }
    }
    else {
        bindingmacrokey = false;
        bindingresetkey = false;
        bindingautoclickkey = false;
    }
}

void ProcessSlotBinding(int vk, int num) {
    if (vk == 0) return;
    if (vk != VK_ESCAPE && num != -1) {
        if (bindingshotgunslot) {
            if (spray == num) spray = shotgun;
            else if (katana == num) katana = shotgun;
            shotgun = num;
            bindingshotgunslot = false;
            SaveSettings();
        }
        else if (bindingsprayslot) {
            if (shotgun == num) shotgun = spray;
            else if (katana == num) katana = spray;
            spray = num;
            bindingsprayslot = false;
            SaveSettings();
        }
        else if (bindingkatanaslot) {
            if (shotgun == num) shotgun = katana;
            else if (spray == num) spray = katana;
            katana = num;
            bindingkatanaslot = false;
            SaveSettings();
        }
    }
    else if (vk == VK_ESCAPE) {
        bindingshotgunslot = false;
        bindingsprayslot = false;
        bindingkatanaslot = false;
    }
}

static void MacKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;

    int vk = 0;
    int num = -1;

    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) vk = 'A' + (key - GLFW_KEY_A);
    else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) { vk = '0' + (key - GLFW_KEY_0); num = key - GLFW_KEY_0; }
    else if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) { vk = VK_NUMPAD0 + (key - GLFW_KEY_KP_0); num = key - GLFW_KEY_KP_0; }
    else if (key == GLFW_KEY_ESCAPE) vk = VK_ESCAPE;
    else if (key == GLFW_KEY_ENTER) vk = VK_RETURN;
    else if (key == GLFW_KEY_TAB) vk = VK_TAB;
    else if (key == GLFW_KEY_SPACE) vk = VK_SPACE;
    else if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) vk = VK_CONTROL;
    else if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) vk = VK_SHIFT;
    else if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT) vk = VK_MENU;
    else if (key == GLFW_KEY_CAPS_LOCK) vk = VK_CAPITAL;
    else if (key == GLFW_KEY_UP) vk = VK_UP;
    else if (key == GLFW_KEY_DOWN) vk = VK_DOWN;
    else if (key == GLFW_KEY_LEFT) vk = VK_LEFT;
    else if (key == GLFW_KEY_RIGHT) vk = VK_RIGHT;
    else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F24) vk = VK_F1 + (key - GLFW_KEY_F1);

    if (bindingmacrokey || bindingresetkey || bindingautoclickkey) {
        ProcessKeyBinding(vk);
    }
    if (bindingshotgunslot || bindingsprayslot || bindingkatanaslot) {
        ProcessSlotBinding(vk, num);
    }
}

static void MacMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (action != GLFW_PRESS) return;

    int vk = 0;
    if (button == GLFW_MOUSE_BUTTON_LEFT) vk = VK_LBUTTON;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) vk = VK_RBUTTON;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) vk = VK_MBUTTON;
    else if (button == GLFW_MOUSE_BUTTON_4) vk = VK_XBUTTON1;
    else if (button == GLFW_MOUSE_BUTTON_5) vk = VK_XBUTTON2;

    if (bindingmacrokey || bindingresetkey || bindingautoclickkey) {
        ProcessKeyBinding(vk);
    }
}

int main(int argc, char** argv) {
    MacPlatform::CheckAccessibilityPermissions();

    AuthSettings::LoadSettings();
    LoadSettings();
    LoadPresets();

    strcpy_s(Username, AuthGlobals::SavedUsername.c_str());
    strcpy_s(Password, AuthGlobals::SavedPassword.c_str());

    if (strlen(Username) > 0 && strlen(Password) > 0) {
        std::thread(DoLoginThread).detach();
    }

    Webhook::LaunchEventThread();

    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    int width = 560;
    int height = 650;

    glfw_window = glfwCreateWindow(width, height, "neuro", NULL, NULL);
    if (!glfw_window) {
        glfwTerminate();
        return 1;
    }
    window_handle = (void*)glfw_window;

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    if (primary) {
        const GLFWvidmode* mode = glfwGetVideoMode(primary);
        if (mode) {
            glfwSetWindowPos(glfw_window, (mode->width - width) / 2, (mode->height - height) / 2);
        }
    }

    glfwMakeContextCurrent(glfw_window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(glfw_window, MacKeyCallback);
    glfwSetMouseButtonCallback(glfw_window, MacMouseButtonCallback);

    if (!ui_bg_path.empty()) {
        if (!LoadBGImage(ui_bg_path)) {
            ui_bg_path = "";
            SaveSettings();
        }
    }

    std::thread(MacroThread, nullptr).detach();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;

    bool font_loaded = false;
    const char* macFontPaths[] = {
        "/System/Library/Fonts/SFCompact.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc"
    };
    for (const char* p : macFontPaths) {
        if (fs::exists(p)) {
            ImFont* f1 = io.Fonts->AddFontFromFileTTF(p, 15.0f);
            ImFont* f2 = io.Fonts->AddFontFromFileTTF(p, 19.0f);
            if (f1 && f2) {
                font_bold = f2;
                font_loaded = true;
                break;
            }
        }
    }
    if (!font_loaded || !font_bold) {
        io.Fonts->AddFontDefault();
        font_bold = io.Fonts->AddFontDefault();
    }

    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    while (!glfwWindowShouldClose(glfw_window) && !exiting) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            static bool is_dragging = false;
            static double drag_start_x = 0, drag_start_y = 0;
            static int win_start_x = 0, win_start_y = 0;

            if (ImGui::IsMouseClicked(0)) {
                ImVec2 mpos = ImGui::GetMousePos();
                if (mpos.y >= 0 && mpos.y < 35 && mpos.x >= 0 && mpos.x < width - 100) {
                    is_dragging = true;
                    glfwGetCursorPos(glfw_window, &drag_start_x, &drag_start_y);
                    glfwGetWindowPos(glfw_window, &win_start_x, &win_start_y);
                }
            }
            if (is_dragging) {
                if (ImGui::IsMouseDown(0)) {
                    double cur_x, cur_y;
                    glfwGetCursorPos(glfw_window, &cur_x, &cur_y);
                    int new_x = win_start_x + (int)(cur_x - drag_start_x);
                    int new_y = win_start_y + (int)(cur_y - drag_start_y);
                    glfwSetWindowPos(glfw_window, new_x, new_y);
                } else {
                    is_dragging = false;
                }
            }
        }

        int clientWidth = width;
        int clientHeight = height;
        glfwGetWindowSize(glfw_window, &clientWidth, &clientHeight);

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2((float)clientWidth, (float)clientHeight));

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("MainPanel", nullptr, windowFlags);
        ImGui::PopStyleVar();

        {
            ImVec2 bgWinPos = ImGui::GetWindowPos();
            if (bg_is_gif && !bg_gif_texture_ids.empty()) {
                bg_gif_timer += ImGui::GetIO().DeltaTime * 1000.0f;
                int delay = bg_gif_delays[bg_gif_current_frame];
                if (delay <= 0) delay = 100;
                while (bg_gif_timer >= (float)delay) {
                    bg_gif_timer -= (float)delay;
                    bg_gif_current_frame = (bg_gif_current_frame + 1) % (int)bg_gif_texture_ids.size();
                    delay = bg_gif_delays[bg_gif_current_frame];
                    if (delay <= 0) delay = 100;
                }
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)(intptr_t)bg_gif_texture_ids[bg_gif_current_frame],
                    bgWinPos,
                    ImVec2(bgWinPos.x + (float)clientWidth, bgWinPos.y + (float)clientHeight)
                );
            }
            else if (bg_texture_id != 0) {
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)(intptr_t)bg_texture_id,
                    bgWinPos,
                    ImVec2(bgWinPos.x + (float)clientWidth, bgWinPos.y + (float)clientHeight)
                );
            }
        }

        if (ui_glow_enabled) {
            ImDrawList* glow_dl = ImGui::GetWindowDrawList();
            ImVec2 winPos = ImGui::GetWindowPos();
            float rounding = 10.0f;

            if (ui_glow_gradient) {

                int vtx_start = glow_dl->VtxBuffer.Size;

                ImU32 borderCol = IM_COL32(255, 255, 255, 200);
                glow_dl->AddRect(
                    ImVec2(winPos.x + 0.5f, winPos.y + 0.5f),
                    ImVec2(winPos.x + (float)clientWidth - 0.5f, winPos.y + (float)clientHeight - 0.5f),
                    borderCol, rounding, 0, 1.5f
                );

                for (int i = 0; i < 5; i++) {
                    float offset = (float)i * 0.5f;
                    float alpha = 0.12f * (1.0f - (float)i / 5.0f);
                    ImU32 glowCol = IM_COL32(255, 255, 255, (int)(255.0f * alpha));
                    glow_dl->AddRect(
                        ImVec2(winPos.x + offset, winPos.y + offset),
                        ImVec2(winPos.x + (float)clientWidth - offset, winPos.y + (float)clientHeight - offset),
                        glowCol, max(0.0f, rounding - offset), 0, 1.0f
                    );
                }

                int vtx_end = glow_dl->VtxBuffer.Size;

                for (int i = vtx_start; i < vtx_end; i++) {
                    ImDrawVert& v = glow_dl->VtxBuffer[i];
                    float t = (v.pos.y - winPos.y) / (float)clientHeight;
                    t = max(0.0f, min(t, 1.0f));

                    float rF = ui_glow_top[0] + (ui_glow_bottom[0] - ui_glow_top[0]) * t;
                    float gF = ui_glow_top[1] + (ui_glow_bottom[1] - ui_glow_top[1]) * t;
                    float bF = ui_glow_top[2] + (ui_glow_bottom[2] - ui_glow_top[2]) * t;

                    int r_val = (int)(rF * 255.0f);
                    int g_val = (int)(gF * 255.0f);
                    int b_val = (int)(bF * 255.0f);
                    int a_val = (v.col >> 24) & 0xFF;

                    v.col = IM_COL32(r_val, g_val, b_val, a_val);
                }
            }
            else {

                int r = (int)(ui_accent[0] * 255.0f);
                int g = (int)(ui_accent[1] * 255.0f);
                int b = (int)(ui_accent[2] * 255.0f);
                ImU32 borderCol = IM_COL32(r, g, b, 200);
                glow_dl->AddRect(
                    ImVec2(winPos.x + 0.5f, winPos.y + 0.5f),
                    ImVec2(winPos.x + (float)clientWidth - 0.5f, winPos.y + (float)clientHeight - 0.5f),
                    borderCol, rounding, 0, 1.5f
                );
                for (int i = 0; i < 5; i++) {
                    float offset = (float)i * 0.5f;
                    float alpha = 0.12f * (1.0f - (float)i / 5.0f);
                    ImU32 glowCol = IM_COL32(r, g, b, (int)(255.0f * alpha));
                    glow_dl->AddRect(
                        ImVec2(winPos.x + offset, winPos.y + offset),
                        ImVec2(winPos.x + (float)clientWidth - offset, winPos.y + (float)clientHeight - offset),
                        glowCol, max(0.0f, rounding - offset), 0, 1.0f
                    );
                }
            }
        }

        if (ui_stars_enabled) {
            struct Star {
                float x, y, vx, vy, brightness, size, twinklePhase, twinkleSpeed;
            };
            static std::vector<Star> bg_stars;
            static bool bg_init = false;
            if (!bg_init) {
                bg_stars.resize(70);
                srand(42);
                for (auto& s : bg_stars) {
                    s.x = (float)(rand() % 560);
                    s.y = (float)(rand() % 650);
                    s.vx = ((rand() % 200) - 100) / 22000.0f;
                    s.vy = ((rand() % 200) - 100) / 22000.0f;
                    s.brightness = 0.4f + (rand() % 55) / 100.0f;
                    s.size = 0.9f + (rand() % 20) / 10.0f;
                    s.twinklePhase = (float)(rand() % 628) / 100.0f;
                    s.twinkleSpeed = 0.5f + (rand() % 150) / 100.0f;
                }
                bg_init = true;
            }

            float bg_dt = ImGui::GetIO().DeltaTime;
            float bg_time = (float)ImGui::GetTime();
            ImVec2 bg_winPos = ImGui::GetWindowPos();
            float bg_W = (float)clientWidth;
            float bg_H = (float)clientHeight;
            ImDrawList* bg_dl = ImGui::GetWindowDrawList();

            for (auto& s : bg_stars) {
                s.x += s.vx * bg_dt * 1000.0f;
                s.y += s.vy * bg_dt * 1000.0f;
                if (s.x < 0) s.x += bg_W;
                if (s.x > bg_W) s.x -= bg_W;
                if (s.y < 0) s.y += bg_H;
                if (s.y > bg_H) s.y -= bg_H;
            }

            const float connect_dist = 145.0f;
            for (size_t i = 0; i < bg_stars.size(); i++) {
                for (size_t j = i + 1; j < bg_stars.size(); j++) {
                    float dx = bg_stars[i].x - bg_stars[j].x;
                    float dy = bg_stars[i].y - bg_stars[j].y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < connect_dist) {
                        float t = 1.0f - dist / connect_dist;
                        float ab = (bg_stars[i].brightness + bg_stars[j].brightness) * 0.5f;
                        float alpha = t * t * 0.55f * ab;
                        float thickness = 0.4f + t * 1.2f;
                        int r = (int)(90 + t * 20);
                        int g = (int)(120 + t * 40);
                        ImU32 lineCol = IM_COL32(r, g, 255, (int)(alpha * 255));
                        bg_dl->AddLine(
                            ImVec2(bg_winPos.x + bg_stars[i].x, bg_winPos.y + bg_stars[i].y),
                            ImVec2(bg_winPos.x + bg_stars[j].x, bg_winPos.y + bg_stars[j].y),
                            lineCol, thickness);
                    }
                }
            }

            for (const auto& s : bg_stars) {
                ImVec2 sp(bg_winPos.x + s.x, bg_winPos.y + s.y);
                float twinkle = 0.75f + 0.25f * sinf(bg_time * s.twinkleSpeed + s.twinklePhase);
                float a = s.brightness * twinkle;

                int r = (int)(ui_accent[0] * 255.0f);
                int g = (int)(ui_accent[1] * 255.0f);
                int b = (int)(ui_accent[2] * 255.0f);

                bg_dl->AddCircleFilled(sp, s.size * 7.0f, IM_COL32(r / 4, g / 4, b, (int)(a * 14)));
                bg_dl->AddCircleFilled(sp, s.size * 4.5f, IM_COL32(r / 2, g / 2, b, (int)(a * 32)));
                bg_dl->AddCircleFilled(sp, s.size * 2.5f, IM_COL32(r, g, b, (int)(a * 75)));
                bg_dl->AddCircleFilled(sp, s.size * 1.3f, IM_COL32(min(r + 50, 255), min(g + 50, 255), b, (int)(a * 150)));
                bg_dl->AddCircleFilled(sp, s.size * 0.65f, IM_COL32(230, 238, 255, (int)(a * 245)));
            }
        }

        if (ui_ripples_enabled) {
            if (ImGui::IsMouseClicked(0)) {
                click_ripples.push_back({ ImGui::GetMousePos(), 0.0f, 1.0f });
            }
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            for (auto it = click_ripples.begin(); it != click_ripples.end(); ) {
                it->radius += ImGui::GetIO().DeltaTime * 150.0f;
                it->alpha -= ImGui::GetIO().DeltaTime * 1.5f;
                if (it->alpha <= 0.0f) {
                    it = click_ripples.erase(it);
                }
                else {
                    int r = (int)(ui_accent[0] * 255.0f);
                    int g = (int)(ui_accent[1] * 255.0f);
                    int b = (int)(ui_accent[2] * 255.0f);
                    dl->AddCircle(it->pos, it->radius, IM_COL32(r, g, b, (int)(it->alpha * 255)), 0, 2.0f);
                    ++it;
                }
            }
        }

        if (ui_trail_enabled) {
            ImVec2 mpos = ImGui::GetMousePos();
            bool should_add = cursor_trail.empty();
            if (!should_add) {
                float dx = cursor_trail.back().pos.x - mpos.x;
                float dy = cursor_trail.back().pos.y - mpos.y;
                if ((dx * dx + dy * dy) > 4.0f) {
                    should_add = true;
                }
            }
            if (should_add) {
                cursor_trail.push_back({ mpos, 1.0f, 4.0f });
            }
            ImDrawList* dl = ImGui::GetForegroundDrawList();

            if (cursor_trail.size() >= 2) {
                int r = (int)(ui_accent[0] * 255.0f);
                int g = (int)(ui_accent[1] * 255.0f);
                int b = (int)(ui_accent[2] * 255.0f);
                for (size_t i = 1; i < cursor_trail.size(); ++i) {
                    float alpha = cursor_trail[i].alpha;
                    float thickness = cursor_trail[i].size * 1.5f;
                    dl->AddLine(cursor_trail[i - 1].pos, cursor_trail[i].pos, IM_COL32(r, g, b, (int)(alpha * 255)), thickness);
                    dl->AddLine(cursor_trail[i - 1].pos, cursor_trail[i].pos, IM_COL32(230, 238, 255, (int)(alpha * 150)), thickness * 0.4f);
                }
            }

            for (auto it = cursor_trail.begin(); it != cursor_trail.end(); ) {
                it->alpha -= ImGui::GetIO().DeltaTime * 2.5f;
                it->size -= ImGui::GetIO().DeltaTime * 10.0f;
                if (it->alpha <= 0.0f || it->size <= 0.0f) {
                    it = cursor_trail.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        ImGui::BeginGroup();
        ImGui::SetCursorPos(ImVec2(16.0f, 10.0f));
        ImGui::PushFont(font_bold);
        ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "neuro");
        ImGui::PopFont();

        float btnPaddingY = 7.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        ImGui::SetCursorPos(ImVec2((float)clientWidth - 80.0f, btnPaddingY));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.20f, 1.00f));
        if (ImGui::Button("-", ImVec2(32.0f, 26.0f))) {
            if (glfw_window) glfwIconifyWindow(glfw_window);
        }
        ImGui::PopStyleColor(3);

        ImGui::SetCursorPos(ImVec2((float)clientWidth - 42.0f, btnPaddingY));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.15f, 0.15f, 1.00f));
        if (ImGui::Button("X", ImVec2(32.0f, 26.0f))) {
            exiting = true;
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::EndGroup();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::SetCursorPos(ImVec2(16.0f, 40.0f));
        ImGui::BeginChild("MainContent", ImVec2((float)clientWidth - 32.0f, (float)clientHeight - 56.0f), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        if (app_state == STATE_LOGIN) {
            float childWidth = 360.0f;
            float childHeight = 330.0f;
            float contentWidth = (float)clientWidth - 32.0f;
            float contentHeight = (float)clientHeight - 56.0f;
            ImGui::SetCursorPos(ImVec2((contentWidth - childWidth) / 2.0f, (contentHeight - childHeight) / 2.0f));

            ImGui::BeginChild("LoginBox", ImVec2(childWidth, childHeight), true, ImGuiWindowFlags_NoScrollbar);

            ImGui::Spacing();
            ImGui::PushFont(font_bold);
            float titleWidth = ImGui::CalcTextSize("AUTHENTICATION").x;
            ImGui::SetCursorPosX((childWidth - titleWidth) / 2.0f);
            ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "AUTHENTICATION");
            ImGui::PopFont();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("Username");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##Username", Username, sizeof(Username));
            ImGui::Spacing();

            ImGui::TextDisabled("Password");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##Password", Password, sizeof(Password), ImGuiInputTextFlags_Password);
            ImGui::Spacing();

            ImGui::Checkbox("Remember Credentials", &RememberMe);
            ImGui::Spacing();

            if (!auth_error.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), auth_error.c_str());
                ImGui::Spacing();
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.28f, 0.65f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.35f, 0.80f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.24f, 0.55f, 1.00f));
            if (ImGui::Button("Log In", ImVec2(-1, 38))) {
                auth_error = "";
                if (strlen(Username) == 0 || strlen(Password) == 0) {
                    auth_error = "Credentials cannot be empty.";
                }
                else {
                    std::thread(DoLoginThread).detach();
                }
            }
            ImGui::PopStyleColor(3);

            ImGui::EndChild();
        }
        else if (app_state == STATE_LOGGING_IN) {
            float contentWidth = (float)clientWidth - 32.0f;
            float contentHeight = (float)clientHeight - 56.0f;

            float boxWidth = 360.0f;
            float boxHeight = 160.0f;
            ImGui::SetCursorPos(ImVec2((contentWidth - boxWidth) / 2.0f, (contentHeight - boxHeight) / 2.0f));
            ImGui::BeginChild("LoadingBox", ImVec2(boxWidth, boxHeight), true, ImGuiWindowFlags_NoScrollbar);

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::PushFont(font_bold);
            float textWidth = ImGui::CalcTextSize("Connecting...").x;
            ImGui::SetCursorPosX((boxWidth - textWidth) / 2.0f);
            ImGui::TextColored(ImVec4(0.38f, 0.48f, 1.00f, 1.00f), "Connecting...");
            ImGui::PopFont();

            ImGui::Spacing();
            float subtextWidth = ImGui::CalcTextSize("Checking credentials").x;
            ImGui::SetCursorPosX((boxWidth - subtextWidth) / 2.0f);
            ImGui::TextDisabled("Checking credentials");
            ImGui::Spacing();
            ImGui::Spacing();

            static float progress = 0.0f;
            progress += 0.015f;
            if (progress > 1.0f) progress = 0.0f;

            ImGui::SetCursorPosX(20.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.38f, 0.48f, 1.00f, 1.00f));
            ImGui::ProgressBar(progress, ImVec2(boxWidth - 40.0f, 6.0f), "");
            ImGui::PopStyleColor();

            ImGui::EndChild();
        }
        else if (app_state == STATE_SETTINGS) {
            ImGui::BeginGroup();
            ImGui::AlignTextToFramePadding();
            ImGui::PushFont(font_bold);
            ImGui::TextDisabled("User:");
            ImGui::SameLine();
            ImGui::Text("%s", AuthGlobals::AuthUsername.c_str());
            ImGui::PopFont();

            float btnWidth = 120.0f;
            ImGui::SameLine(ImGui::GetWindowWidth() - btnWidth);
            if (enabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.20f, 0.20f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.25f, 0.25f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.15f, 0.15f, 1.00f));
                if (ImGui::Button("DISABLE", ImVec2(btnWidth, 0.0f))) {
                    enabled = false;
                    SaveSettings();
                }
                ImGui::PopStyleColor(3);
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.20f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.25f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.55f, 0.15f, 1.00f));
                if (ImGui::Button("ENABLE", ImVec2(btnWidth, 0.0f))) {
                    enabled = true;
                    SaveSettings();
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static int activeTab = 0;

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);

            if (activeTab == 0) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ui_accent2[0] * 0.82f, ui_accent2[1] * 0.82f, ui_accent2[2] * 0.82f, 1.00f));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.13f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.12f, 0.15f, 1.00f));
            }
            if (ImGui::Button("Main", ImVec2(80.0f, 28.0f))) {
                activeTab = 0;
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            if (activeTab == 1) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ui_accent2[0] * 0.82f, ui_accent2[1] * 0.82f, ui_accent2[2] * 0.82f, 1.00f));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.13f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.12f, 0.15f, 1.00f));
            }
            if (ImGui::Button("Misc", ImVec2(80.0f, 28.0f))) {
                activeTab = 1;
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            if (activeTab == 2) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ui_accent2[0] * 0.82f, ui_accent2[1] * 0.82f, ui_accent2[2] * 0.82f, 1.00f));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.13f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.12f, 0.15f, 1.00f));
            }
            if (ImGui::Button("Presets", ImVec2(80.0f, 28.0f))) {
                activeTab = 2;
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            if (activeTab == 3) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ui_accent2[0] * 0.82f, ui_accent2[1] * 0.82f, ui_accent2[2] * 0.82f, 1.00f));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.13f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.12f, 0.15f, 1.00f));
            }
            if (ImGui::Button("Settings", ImVec2(80.0f, 28.0f))) {
                activeTab = 3;
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();

            if (activeTab == 4) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ui_accent2[0], ui_accent2[1], ui_accent2[2], ui_accent2[3]));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ui_accent2[0] * 0.82f, ui_accent2[1] * 0.82f, ui_accent2[2] * 0.82f, 1.00f));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.13f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.12f, 0.15f, 1.00f));
            }
            if (ImGui::Button("Background", ImVec2(100.0f, 28.0f))) {
                activeTab = 4;
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

            if (activeTab == 0) {
                ImGui::BeginChild("CardHotkeys", ImVec2(0, 115.0f), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Keybinds (Right Click to Unbind)");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Columns(3, "keybinds_cols", false);
                ImGui::TextDisabled("Macro Key");
                std::string macroLabel = bindingmacrokey ? "[ Press Key ]" : GetKeyName(macrokey);
                if (ImGui::Button(macroLabel.c_str(), ImVec2(-1, 30))) {
                    bindingmacrokey = true;
                    bindingresetkey = false;
                    bindingautoclickkey = false;
                    bindingshotgunslot = false;
                    bindingsprayslot = false;
                    bindingkatanaslot = false;
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    macrokey = 0;
                    bindingmacrokey = false;
                    SaveSettings();
                }
                ImGui::NextColumn();

                ImGui::TextDisabled("Reset Key");
                std::string resetLabel = bindingresetkey ? "[ Press Key ]" : GetKeyName(resetkey);
                if (ImGui::Button(resetLabel.c_str(), ImVec2(-1, 30))) {
                    bindingresetkey = true;
                    bindingmacrokey = false;
                    bindingautoclickkey = false;
                    bindingshotgunslot = false;
                    bindingsprayslot = false;
                    bindingkatanaslot = false;
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    resetkey = 0;
                    bindingresetkey = false;
                    SaveSettings();
                }
                ImGui::NextColumn();

                ImGui::TextDisabled("Autoclick Key");
                std::string autoclickKeyLabel = bindingautoclickkey ? "[ Press Key ]" : GetKeyName(autoclickkey);
                if (ImGui::Button(autoclickKeyLabel.c_str(), ImVec2(-1, 30))) {
                    bindingautoclickkey = true;
                    bindingmacrokey = false;
                    bindingresetkey = false;
                    bindingshotgunslot = false;
                    bindingsprayslot = false;
                    bindingkatanaslot = false;
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    autoclickkey = 0;
                    bindingautoclickkey = false;
                    SaveSettings();
                }
                ImGui::Columns(1);
                ImGui::EndChild();

                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::BeginChild("CardWeaponSlots", ImVec2(0, 115.0f), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Weapon Slots (0-9)");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Columns(3, "weapon_slots", false);
                ImGui::TextDisabled("SG/DB/TAC");
                std::string shotgunLabel = bindingshotgunslot ? "[ Press Slot ]" : std::to_string(shotgun);
                if (ImGui::Button(shotgunLabel.c_str(), ImVec2(-1, 30))) {
                    bindingshotgunslot = true;
                    bindingsprayslot = false;
                    bindingkatanaslot = false;
                    bindingmacrokey = false;
                    bindingresetkey = false;
                }
                ImGui::NextColumn();

                ImGui::TextDisabled("Spray/Mask");
                std::string sprayLabel = bindingsprayslot ? "[ Press Slot ]" : std::to_string(spray);
                if (ImGui::Button(sprayLabel.c_str(), ImVec2(-1, 30))) {
                    bindingsprayslot = true;
                    bindingshotgunslot = false;
                    bindingkatanaslot = false;
                    bindingmacrokey = false;
                    bindingresetkey = false;
                }
                ImGui::NextColumn();

                ImGui::TextDisabled("Katana/Mask");
                std::string katanaLabel = bindingkatanaslot ? "[ Press Slot ]" : std::to_string(katana);
                if (ImGui::Button(katanaLabel.c_str(), ImVec2(-1, 30))) {
                    bindingkatanaslot = true;
                    bindingshotgunslot = false;
                    bindingsprayslot = false;
                    bindingmacrokey = false;
                    bindingresetkey = false;
                }
                ImGui::Columns(1);
                ImGui::EndChild();

                ImGui::Spacing();

                ImGui::BeginChild("CardAutoclicker", ImVec2(0, 95.0f), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Auto Clicker");
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::AlignTextToFramePadding();
                if (ImGui::Checkbox("Enable Autoclicker (LMB)", &autoclick)) SaveSettings();
                ImGui::SameLine(220.0f);
                ImGui::SetNextItemWidth(50.0f);
                if (ImGui::InputInt("##autoclicksleep_input", &autoclicksleep, 0, 0)) SaveSettings();
                ImGui::SameLine(280.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##autoclicksleep_slider", &autoclicksleep, 1, 100, "%d ms")) SaveSettings();
                ImGui::EndChild();
            }
            else if (activeTab == 1) {
                ImGui::BeginChild("ReloadBox", ImVec2(0, 95.0f), true);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Auto Reload");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::AlignTextToFramePadding();
                if (ImGui::Checkbox("Auto Reload", &reload))
                    SaveSettings();

                ImGui::SameLine(220.0f);
                ImGui::SetNextItemWidth(50.0f);
                if (ImGui::InputInt("##reloadsleep_input", &reloadsleep, 0, 0))
                    SaveSettings();

                ImGui::SameLine(280.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##reloadsleep_slider", &reloadsleep, 1, 150, "%d ms"))
                    SaveSettings();

                ImGui::EndChild();

                ImGui::Spacing();

                ImGui::BeginChild("CrouchBox", ImVec2(0, 95.0f), true);
                ImGui::Text("Crouch");
                ImGui::Separator();

                if (ImGui::Checkbox("Enable Crouch", &crouch_macro))
                    SaveSettings();

                ImGui::EndChild();

                ImGui::Spacing();

                ImGui::BeginChild("PullOutBox", ImVec2(0, 95.0f), true);
                ImGui::Text("Pull Out");
                ImGui::Separator();

                if (ImGui::Checkbox("Pull Out Gun/Spray", &pull_out))
                    SaveSettings();

                ImGui::EndChild();
            }
            else if (activeTab == 2) {
                ImGui::BeginChild("CardPresets", ImVec2(0, 160.0f), true);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Presets");
                ImGui::Separator();
                ImGui::Spacing();

                static int editing_preset_idx = -1;
                static char editing_preset_name[128] = "";

                float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                for (size_t i = 0; i < presets.size(); i++) {
                    ImGui::PushID((int)i);

                    if (editing_preset_idx == (int)i) {
                        float edit_width = ImGui::CalcTextSize(editing_preset_name).x + ImGui::GetStyle().FramePadding.x * 2.0f + 20.0f;
                        if (edit_width < 75.0f) edit_width = 75.0f;

                        ImGui::SetNextItemWidth(edit_width);
                        if (ImGui::InputText("##edit_preset", editing_preset_name, sizeof(editing_preset_name), ImGuiInputTextFlags_EnterReturnsTrue)) {
                            if (strlen(editing_preset_name) > 12) {
                                editing_preset_name[12] = '\0';
                            }
                            if (strlen(editing_preset_name) > 0) {
                                RenamePresetFile(presets[i].name, editing_preset_name);
                                presets[i].name = editing_preset_name;
                            }
                            editing_preset_idx = -1;
                        }

                        if (editing_preset_idx == (int)i && !ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            if (strlen(editing_preset_name) > 12) {
                                editing_preset_name[12] = '\0';
                            }
                            if (strlen(editing_preset_name) > 0) {
                                RenamePresetFile(presets[i].name, editing_preset_name);
                                presets[i].name = editing_preset_name;
                            }
                            editing_preset_idx = -1;
                        }
                    }
                    else {
                        bool active = (activepreset == (int)i);
                        if (active) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.28f, 0.55f, 1.00f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.35f, 0.65f, 1.00f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.24f, 0.50f, 1.00f));
                        }

                        float button_width = ImGui::CalcTextSize(presets[i].name.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
                        if (ImGui::Button(presets[i].name.c_str(), ImVec2(button_width, 28.0f))) {
                            macrospeed = presets[i].macrospeed;
                            actionsleep = presets[i].actionsleep;
                            reloadsleep = presets[i].reloadsleep;
                            resetsleep = presets[i].resetsleep;
                            resetduration = presets[i].resetduration;
                            double_shoot_enabled = presets[i].double_shoot_enabled;
                            double_shoot_delay = presets[i].double_shoot_delay;
                            activepreset = (int)i;
                            SaveSettings();
                        }
                        if (active) {
                            ImGui::PopStyleColor(3);
                        }

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            editing_preset_idx = (int)i;
                            strcpy_s(editing_preset_name, presets[i].name.c_str());
                        }

                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem("Show Preset")) {
                                std::string dir = GetPresetsDir();
                                std::string filepath = dir + presets[i].name + ".json";
                                MacPlatform::RevealInFinder(filepath);
                            }
                            if (ImGui::MenuItem("Delete Preset")) {
                                DeletePresetFile(presets[i].name);
                                presets.erase(presets.begin() + i);
                                activepreset = -1;
                                ImGui::EndPopup();
                                ImGui::PopID();
                                break;
                            }
                            ImGui::EndPopup();
                        }
                    }

                    float last_button_x2 = ImGui::GetItemRectMax().x;
                    float next_button_width = 75.0f;
                    if (i + 1 < presets.size()) {
                        if (editing_preset_idx == (int)(i + 1)) {
                            next_button_width = ImGui::CalcTextSize(editing_preset_name).x + ImGui::GetStyle().FramePadding.x * 2.0f + 20.0f;
                        }
                        else {
                            next_button_width = ImGui::CalcTextSize(presets[i + 1].name.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
                        }
                    }
                    float next_button_x2 = last_button_x2 + ImGui::GetStyle().ItemSpacing.x + next_button_width;
                    if (i + 1 < presets.size() && next_button_x2 < window_visible_x2)
                        ImGui::SameLine();

                    ImGui::PopID();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                static char new_preset_name[128] = "";
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputText("##new_preset", new_preset_name, sizeof(new_preset_name));
                ImGui::SameLine();
                if (ImGui::Button("Save Preset", ImVec2(100.0f, 26.0f))) {
                    if (strlen(new_preset_name) > 12) {
                        new_preset_name[12] = '\0';
                    }
                    size_t len = strlen(new_preset_name);
                    if (len >= 1) {
                        bool found = false;
                        for (auto& pr : presets) {
                            if (pr.name == new_preset_name) {
                                pr.macrospeed = macrospeed;
                                pr.actionsleep = actionsleep;
                                pr.reloadsleep = reloadsleep;
                                pr.resetsleep = resetsleep;
                                pr.resetduration = resetduration;
                                pr.double_shoot_enabled = double_shoot_enabled;
                                pr.double_shoot_delay = double_shoot_delay;
                                SaveSinglePreset(GetPresetsDir(), pr);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            preset new_pr = { new_preset_name, macrospeed, actionsleep, reloadsleep, resetsleep, resetduration, double_shoot_enabled, double_shoot_delay };
                            presets.push_back(new_pr);
                            SaveSinglePreset(GetPresetsDir(), new_pr);
                        }
                        new_preset_name[0] = '\0';
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Presets", ImVec2(100.0f, 26.0f))) {
                    presets.clear();
                    presets.push_back({ "SG",       560, 10, 33, 50, 5000, false, 185 });
                    presets.push_back({ "DB",       817, 10, 33, 50, 5000, false,  185 });
                    presets.push_back({ "Tac",      480, 10, 33, 50, 5000, false, 185 });
                    presets.push_back({ "SG (ST)",  900, 35, 33, 50, 5000, true,  635 });
                    presets.push_back({ "DB (ST)",  761, 35, 33, 50, 5000, true,  325 });
                    presets.push_back({ "Tac (ST)", 875, 32, 33, 50, 5000, true,  530 });
                    SavePresets();
                    activepreset = -1;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(Right click to delete)");

                ImGui::EndChild();

                ImGui::Spacing();

                ImGui::BeginChild("CardMacroTimings", ImVec2(0, 270.0f), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Macro Speed");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Cycle Delay");
                ImGui::SameLine(160.0f);
                ImGui::SetNextItemWidth(50.0f);
                if (ImGui::InputInt("##macrospeed_input", &macrospeed, 0, 0)) { activepreset = -1; SaveSettings(); }
                ImGui::SameLine(220.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##macrospeed", &macrospeed, 100, 1000, "%d ms")) { activepreset = -1; SaveSettings(); }

                ImGui::TextDisabled("Switch Delay");
                ImGui::SameLine(160.0f);
                ImGui::SetNextItemWidth(50.0f);
                if (ImGui::InputInt("##actionsleep_input", &actionsleep, 0, 0)) { activepreset = -1; SaveSettings(); }
                ImGui::SameLine(220.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##actionsleep", &actionsleep, 1, 150, "%d ms")) { activepreset = -1; SaveSettings(); }

                ImGui::TextDisabled("Reset Delay");
                ImGui::SameLine(160.0f);
                ImGui::SetNextItemWidth(50.0f);
                if (ImGui::InputInt("##resetsleep_input", &resetsleep, 0, 0)) { activepreset = -1; SaveSettings(); }
                ImGui::SameLine(220.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##resetsleep", &resetsleep, 1, 150, "%d ms")) { activepreset = -1; SaveSettings(); }

                ImGui::TextDisabled("Katana Hold Time");
                ImGui::SameLine(160.0f);
                ImGui::SetNextItemWidth(50.0f);
                if (ImGui::InputInt("##resetduration_input", &resetduration, 0, 0)) { activepreset = -1; SaveSettings(); }
                ImGui::SameLine(220.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##resetduration", &resetduration, 100, 10000, "%d ms")) { activepreset = -1; SaveSettings(); }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::AlignTextToFramePadding();
                if (ImGui::Checkbox("Shoot Twice Before Reload", &double_shoot_enabled)) { activepreset = -1; SaveSettings(); }
                ImGui::SameLine(220.0f);
                ImGui::SetNextItemWidth(50.0f);
                if (ImGui::InputInt("##double_shoot_delay_input", &double_shoot_delay, 0, 0)) { activepreset = -1; SaveSettings(); }
                ImGui::SameLine(280.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##double_shoot_delay_slider", &double_shoot_delay, 10, 1000, "%d ms")) { activepreset = -1; SaveSettings(); }

                ImGui::EndChild();
            }
            else if (activeTab == 3) {
                ImGui::BeginChild("CardUISettings", ImVec2(0, 230.0f), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Appearance");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Cursor Trail");
                ImGui::SameLine(160.0f);
                if (ImGui::Checkbox("##trail", &ui_trail_enabled)) SaveSettings();

                ImGui::TextDisabled("Click Ripples");
                ImGui::SameLine(160.0f);
                if (ImGui::Checkbox("##ripples", &ui_ripples_enabled)) SaveSettings();

                ImGui::TextDisabled("Constellation BG");
                ImGui::SameLine(160.0f);
                if (ImGui::Checkbox("##stars", &ui_stars_enabled)) SaveSettings();

                ImGui::TextDisabled("Outline Glow");
                ImGui::SameLine(160.0f);
                if (ImGui::Checkbox("##glow", &ui_glow_enabled)) SaveSettings();

                ImGui::TextDisabled("Stream Proof");
                ImGui::SameLine(160.0f);
                if (ImGui::Checkbox("##streamproof", &ui_streamproof)) {
                    SaveSettings();
                }

                ImGui::EndChild();

                ImGui::Spacing();

                ImGui::BeginChild("CardColors", ImVec2(0, 190.0f), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Colors");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Background");
                ImGui::SameLine(160.0f);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit4("##bgcolor", ui_bg_color, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar)) {
                    SaveSettings();
                }

                ImGui::TextDisabled("Rainbow Mode");
                ImGui::SameLine(160.0f);
                if (ImGui::Checkbox("##rainbow", &ui_rainbow)) SaveSettings();

                if (ui_rainbow) {
                    ImGui::TextDisabled("Rainbow Speed");
                    ImGui::SameLine(160.0f);
                    ImGui::SetNextItemWidth(50.0f);
                    if (ImGui::InputFloat("##rainbow_spd_in", &ui_rainbow_speed, 0.0f, 0.0f, "%.2f")) {
                        if (ui_rainbow_speed < 0.01f) ui_rainbow_speed = 0.01f;
                        if (ui_rainbow_speed > 5.0f) ui_rainbow_speed = 5.0f;
                        SaveSettings();
                    }
                    ImGui::SameLine(220.0f);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##rainbow_spd", &ui_rainbow_speed, 0.01f, 5.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {
                        SaveSettings();
                    }
                }
                else {
                    ImGui::TextDisabled("Accent Color");
                    ImGui::SameLine(160.0f);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::ColorEdit4("##accent", ui_accent, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar)) {
                        SaveSettings();
                    }

                    ImGui::TextDisabled("Tab/Button Color");
                    ImGui::SameLine(160.0f);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::ColorEdit4("##accent2", ui_accent2, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar)) {
                        SaveSettings();
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Gradient Glow");
                ImGui::SameLine(160.0f);
                if (ImGui::Checkbox("##gradientglow", &ui_glow_gradient)) SaveSettings();

                if (ui_glow_gradient) {
                    ImGui::TextDisabled("Top Color");
                    ImGui::SameLine(160.0f);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::ColorEdit4("##glowtop", ui_glow_top, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar)) SaveSettings();

                    ImGui::TextDisabled("Bottom Color");
                    ImGui::SameLine(160.0f);
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::ColorEdit4("##glowbottom", ui_glow_bottom, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar)) SaveSettings();
                }

                ImGui::Spacing();
                ImGui::SetCursorPosX(160.0f);
                if (ImGui::Button("Reset Defaults", ImVec2(-1, 26.0f))) {
                    ui_accent[0] = 0.38f; ui_accent[1] = 0.48f; ui_accent[2] = 1.00f; ui_accent[3] = 1.00f;
                    ui_accent2[0] = 0.22f; ui_accent2[1] = 0.28f; ui_accent2[2] = 0.55f; ui_accent2[3] = 1.00f;
                    ui_rainbow = false;
                    ui_rainbow_speed = 0.5f;
                    ui_bg_color[0] = 0.067f; ui_bg_color[1] = 0.067f; ui_bg_color[2] = 0.082f; ui_bg_color[3] = 1.00f;
                    ui_glow_gradient = false;
                    ui_glow_top[0] = 0.80f; ui_glow_top[1] = 0.20f; ui_glow_top[2] = 0.20f; ui_glow_top[3] = 1.00f;
                    ui_glow_bottom[0] = 0.20f; ui_glow_bottom[1] = 0.20f; ui_glow_bottom[2] = 0.80f; ui_glow_bottom[3] = 1.00f;
                    SaveSettings();
                }

                ImGui::EndChild();
            }
            else if (activeTab == 4) {
                ImGui::BeginChild("CardBackground", ImVec2(0, 110.0f), true, ImGuiWindowFlags_NoScrollbar);
                ImGui::TextColored(ImVec4(ui_accent[0], ui_accent[1], ui_accent[2], ui_accent[3]), "Custom Background");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Image/GIF");
                ImGui::SameLine(160.0f);
                if (ImGui::Button("Browse...", ImVec2(100.0f, 0.0f))) {
                    OpenBGFilePicker();
                }
                if (!ui_bg_path.empty()) {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear", ImVec2(60.0f, 0.0f))) {
                        ClearBGTextures();
                        ui_bg_path = "";
                        SaveSettings();
                    }
                    std::string displayPath = ui_bg_path;
                    size_t lastSlash = displayPath.find_last_of("\\/");
                    if (lastSlash != std::string::npos) displayPath = displayPath.substr(lastSlash + 1);
                    ImGui::TextDisabled("Loaded: %s", displayPath.c_str());
                }

                ImGui::EndChild();
            }
            ImGui::PopStyleColor();

            float bottomY = ImGui::GetWindowHeight() - 35.0f;
            ImGui::SetCursorPosY(bottomY);
            ImGui::Separator();

            ImGui::SetCursorPosY(bottomY + 8.0f);
            ImGui::AlignTextToFramePadding();

            if (enabled) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[ACTIVE]");
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[DISABLED]");
            }

            ImGui::SameLine();
            ImGui::TextDisabled(" | ");
            ImGui::SameLine();
            if (robloxactive) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Roblox Active");
            }
            else {
                ImGui::TextDisabled("Roblox Inactive");
            }

            float trayButtonWidth = 140.0f;
            ImGui::SameLine(ImGui::GetWindowWidth() - trayButtonWidth - 8.0f);
            if (ImGui::Button("Minimize to Tray", ImVec2(trayButtonWidth, 0.0f))) {
                if (glfw_window) glfwIconifyWindow(glfw_window);
            }
        }

        ImGui::EndChild();
        ImGui::End();

        ImGui::Render();
        int fbW, fbH;
        glfwGetFramebufferSize(glfw_window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(glfw_window);
    }

    ClearBGTextures();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(glfw_window);
    glfwTerminate();

    return 0;
}