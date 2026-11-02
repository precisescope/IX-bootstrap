IX Bootstrap

A lightweight, ncurses-based Android ADB setup utility written in C, designed to run in Termux.

IX Bootstrap provides an interactive terminal interface for wireless ADB pairing, device connections, APK downloads, and Android setup guidance.

Features

- Interactive TUI — Keyboard-driven interface using ncurses.
- ADB Pairing — Pair Android devices over Wireless debugging.
- ADB Connection — Connect to devices using their IP address and connection port.
- F-Droid — Download the F-Droid APK.
- InstallerX Revived — Retrieve and download the latest stable APK from GitHub Releases.
- APK Installation — Install downloaded applications through ADB.
- Developer Options Guide — Instructions for enabling Android Developer options.
- Shizuku Guide — Instructions for pairing Shizuku through Wireless debugging.

Requirements

- Android
- Termux
- Clang
- ncurses
- Android platform tools ("adb")
- curl
- jq

Dependencies

Install the required packages in Termux:

pkg install clang ncurses android-tools curl jq

Compilation

Clone the repository:

git clone https://github.com/precisescope/ix-bootstrap.git
cd ix-bootstrap

Compile:

clang -Wall -Wextra -O2 ix-bootstrap.c -o ix-bootstrap -lncurses

Running

./ix-bootstrap

ADB Usage

IX Bootstrap supports Android Wireless debugging.

Pairing

1. Enable Wireless debugging.
2. Select Pair device with pairing code.
3. Enter the device IP, pairing port, and pairing code in IX Bootstrap.

Connection

1. Find the connection IP and port in Wireless debugging.
2. Select Connect device with ADB.
3. Enter the IP address and connection port.

Pairing and connection ports are separate.

APK Downloads

Downloaded APK files are stored in the Termux home directory:

- "F-Droid.apk"
- "InstallerX-Revived.apk"

Installation is performed using:

adb install -r

License

This project is licensed under the MIT License. See "LICENSE" (LICENSE) for details.

Project Information

- Name: IX Bootstrap
- Language: C
- Interface: ncurses
- Platform: Android / Termux
- Version: 1.0

---

Made for Android users who prefer getting things done from the terminal.
