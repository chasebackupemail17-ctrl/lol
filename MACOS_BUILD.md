# neuro — macOS Build & Deployment Guide

This project has been ported to macOS with **Dear ImGui + GLFW + OpenGL 3**, native macOS event taps, and Apple Security frameworks.

You can develop and maintain everything on your Windows PC and use **GitHub Actions** to automatically build a native macOS `.app` bundle using real macOS runners in the cloud.

---

## 1. How to Build Using GitHub Actions (From Windows)

You do **not** need a Mac or Apple tools installed locally. GitHub provides free macOS runners that compile the application automatically.

### Step 1: Push the repository to GitHub
1. Create a new repository on [GitHub](https://github.com) (can be Private or Public).
2. Initialize and push this folder:
```bash
git init
git add .
git commit -m "Port neuro to macOS with GLFW and GitHub Actions"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/YOUR_REPO.git
git push -u origin main
```

### Step 2: Download the built macOS App
1. Go to your repository on GitHub.
2. Click the **Actions** tab at the top.
3. You will see a workflow named **"Build macOS Application"** running.
4. Once completed (usually 1-2 minutes), click on the workflow run.
5. Under **Artifacts** at the bottom of the page, download:
   - **`neuro-mac-AppleSilicon-arm64`** (for all modern M1 / M2 / M3 / M4 Macs)
   - **`neuro-mac-Intel-x86_64`** (for older Intel Macs)

---

## 2. Sending & Running on macOS (Instructions for Recipient)

When you send the `.zip` to someone with a Mac, they must do two one-time steps on their Mac:

### 1. Gatekeeper Bypass (First Launch Only)
Because the app is built without a paid Apple Developer certificate ($99/year), macOS Gatekeeper will show a warning ("Apple cannot check it for malicious software").
- **Option A (GUI):**
  1. Right-click (or Control-click) `neuro.app`.
  2. Select **Open** from the menu.
  3. Click **Open** on the confirmation dialog.
- **Option B (Terminal command):**
  Open Terminal and run:
  ```bash
  xattr -cr /path/to/neuro.app
  ```

### 2. Grant Accessibility Permissions (Required for Key / Mouse Simulation)
macOS requires explicit permission for apps to simulate keyboard and mouse input into games like Roblox:
1. Launch `neuro.app`. A prompt will appear asking for Accessibility permissions.
2. Click **Open System Settings**.
3. Go to **Privacy & Security** → **Accessibility**.
4. Enable the toggle next to **neuro**.

---

## 3. File & Preset Locations on macOS

On Windows, settings and presets were stored next to the `.exe`. On macOS, applications are self-contained bundles, so settings and presets are stored in standard macOS Application Support:

- **Directory:** `~/Library/Application Support/neuro/`
- **Settings file:** `neuro_settings.ini`
- **Presets directory:** `~/Library/Application Support/neuro/presets/`
- **Auth settings:** `neuro_auth.ini`

---

## 4. Keybind Mapping Reference

The app uses standard keyboard codes matching your existing presets:
- **Left Mouse Button (LMB):** Automatically monitored and mapped.
- **Keys A–Z, 0–9:** Mapped 1:1 with standard keyboard layout.
- **Modifiers:** Escape, Enter, Tab, Space, Shift, Control, Alt, CapsLock, Arrow keys.
- **Custom drag bar:** You can drag the borderless window anywhere by clicking and dragging the top 35px title bar.
