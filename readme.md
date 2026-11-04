# IX Bootstrap

just a really really normal ass way of installing InstallerX and F-droid store in your android to start downloading APKs like normal gaain without what update google has put out

Features

- Wireless ADB pairing — Pair with an Android device using its pairing code.
- ADB connection — Connect to a device using its IP address and connection port.
- F-Droid — Download the F-Droid APK.
- InstallerX Revived — Fetch the latest release APK from GitHub Releases.
- APK installation — Install the APKs you downloaded with installerX
- Developer Options guide — just how to enable developer options without talking in alien language-ish way
- Shizuku guide — Instructions for pairing Shizuku through Wireless debug but although try to google how to do it on your phones model 
- Terminal interface — An interactive ncurses-based menu that doesn't look prehistoric era

pros and cons
Pros :3
- vibecode free
- made to be simple as possible
- not a corporate slop

Cons (·n·)
- needs basic common sense and brain to use (bad for vibecoders and power user LARPers
Requirements

- Android device running Termux
- "android-tools" (ADB)
- "curl"
- "jq"

For compiling from source, you'll also need "clang" and "ncurses".

Installation

Option 1: Clone and run

Install the required runtime packages:
```
pkg update
pkg install git android-tools curl jq
```
Clone the repository:
```
git clone https://github.com/precisescope/IX-bootstrap.git
cd IX-bootstrap
```
Make the executable runnable and launch it:
```
chmod +x ix-bootstrap
./ix-bootstrap
```
«Note: The included executable is a precompiled binary. If it doesn't run on your device because of architecture or build compatibility, use Option 2 to compile it locally.»

Option 2: Compile from source (if your phone is throwing a unknown tantrum) 

Install the build dependencies:
```
pkg update
pkg install git clang ncurses android-tools curl jq
```
Clone the repository:
```
git clone https://github.com/precisescope/IX-bootstrap.git
cd IX-bootstrap
```
Compile:
```
clang -Wall -Wextra -O2 ix-bootstrap.c -o ix-bootstrap -lncurses
```
Run:
```
./ix-bootstrap
```
Using Wireless ADB

Pairing a device

1. Enable Developer options and Wireless debugging on the Android device.
2. Open Wireless debugging and select Pair device with pairing code.
3. In IX Bootstrap, choose the pairing option.
4. Enter the device IP address, pairing port, and pairing code when prompted.

Connecting a device

1. Find the IP address and connection port in Wireless debugging.
2. Choose the ADB connection option.
3. Enter the IP address and connection port.

Important: The pairing port and connection port are different. Use the appropriate port for each operation.

APK downloads

IX Bootstrap downloads APK files into your Termux home directory:

- "F-Droid.apk"
- "InstallerX-Revived.apk"

Downloaded applications are installed through ADB. Review downloaded applications and their sources before installing them.

Troubleshooting

- "Permission denied": Run "chmod +x ix-bootstrap".
- "No such file or directory" or execution failure: The binary may be incompatible with your device or build environment. Try compiling from source.
- "adb: command not found": Install "android-tools" in Termux.
- Pairing fails: Check that Wireless debugging is enabled and that you're using the pairing port and pairing code.
- Connection fails: Check the device IP, connection port, and that the devices can communicate over the network.
- Download fails: Check your internet connection and retry.

Project structure

File| Purpose
"ix-bootstrap"| Precompiled executable
"ix-bootstrap.c"| C source code
"DEPENDENCIES.md"| Dependency information
"LICENSE"| MIT license

Limitations

IX Bootstrap is a helper utility. It does not root your device, automatically grant every permission, or guarantee that every APK can be installed. Android version, device configuration, and application requirements may affect installation.

License

Released under the "MIT License" (LICENSE).

Repository

"https://github.com/precisescope/IX-bootstrap" (https://github.com/precisescope/IX-bootstrap)

Made for Android users who prefer getting shit done from the terminal.
