Dependencies

IX Bootstrap requires the following Termux packages:

Package| Purpose
clang| C compiler
ncurses| Terminal interface
android-tools| ADB commands
curl| APK downloads and GitHub API requests
jq| GitHub release JSON parsing

Install all dependencies

pkg update
pkg install clang ncurses android-tools curl jq

Build

clang -Wall -Wextra -O2 ix-bootstrap.c -o ix-bootstrap -lncurses
