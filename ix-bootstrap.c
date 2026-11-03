
#define _POSIX_C_SOURCE 200809L

#include <ncurses.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define APP_NAME "IX-BOOTSTRAP"
#define FOOTER "Made by precisescope (GitHub) | 8bit_binary (Instagram) | pls follow me"

#define IP_SIZE 128
#define PORT_SIZE 16

#define CP_NORMAL   1
#define CP_MENU     2
#define CP_SELECTED 3
#define CP_TITLE    4
#define CP_FOOTER   5
#define CP_FIELD    6

static char last_ip[IP_SIZE] = "";
static char status_text[256] = "";
static int colors_enabled = 0;

/* ---------- Configuration ---------- */

static void config_path(char *out, size_t size, const char *name)
{
    const char *home = getenv("HOME");

    if (!home || !*home)
        home = ".";

    snprintf(out, size, "%s/.config/ix-bootstrap/%s", home, name);
}

static void load_last_ip(void)
{
    char path[PATH_MAX];
    config_path(path, sizeof(path), "last_ip");

    FILE *f = fopen(path, "r");
    if (!f)
        return;

    if (fgets(last_ip, sizeof(last_ip), f))
        last_ip[strcspn(last_ip, "\r\n")] = '\0';

    fclose(f);
}

static void save_last_ip(const char *ip)
{
    char dir[PATH_MAX];
    char path[PATH_MAX];
    const char *home = getenv("HOME");

    if (!home || !*home)
        home = ".";

    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0700);

    snprintf(dir, sizeof(dir), "%s/.config/ix-bootstrap", home);
    mkdir(dir, 0700);

    config_path(path, sizeof(path), "last_ip");

    FILE *f = fopen(path, "w");
    if (!f) {
        snprintf(status_text, sizeof(status_text),
                 "Could not save the last device IP.");
        return;
    }

    fprintf(f, "%s\n", ip);
    fclose(f);

    snprintf(last_ip, sizeof(last_ip), "%s", ip);
}

static void set_status(const char *message)
{
    snprintf(status_text, sizeof(status_text), "%s",
             message ? message : "");
}

/* ---------- Colors and layout ---------- */

static void init_theme(void)
{
    if (!has_colors())
        return;

    if (start_color() == ERR)
        return;

    colors_enabled = 1;

    /*
     * Use a real black background instead of the terminal's
     * default background whenever the terminal supports it.
     */
    init_pair(CP_NORMAL,   COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_MENU,     COLOR_BLACK, COLOR_WHITE);
    init_pair(CP_SELECTED, COLOR_BLACK, COLOR_CYAN);
    init_pair(CP_TITLE,    COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_FOOTER,   COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_FIELD,    COLOR_WHITE, COLOR_BLACK);

    bkgd(COLOR_PAIR(CP_NORMAL));
}

static int panel_width(void)
{
    int width = COLS - 4;

    if (width > 68)
        width = 68;
    if (width < 10)
        width = 10;

    return width;
}

static int panel_x(int width)
{
    int x = (COLS - width) / 2;
    return x < 0 ? 0 : x;
}

static void draw_footer(void)
{
    if (LINES < 1)
        return;

    int y = LINES - 1;

    attron(COLOR_PAIR(CP_FOOTER) | A_DIM);
    mvhline(y, 0, ' ', COLS);
    mvaddnstr(y, 0, FOOTER, COLS > 1 ? COLS - 1 : 0);
    attroff(COLOR_PAIR(CP_FOOTER) | A_DIM);
}

static void draw_header(const char *title)
{
    erase();
    bkgd(COLOR_PAIR(CP_NORMAL));

    int width = panel_width();
    int x = panel_x(width);

    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvaddnstr(1, x, APP_NAME, width);
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    attron(COLOR_PAIR(CP_TITLE));
    mvaddnstr(2, x, title, width);
    mvhline(3, x, ACS_HLINE, width);
    attroff(COLOR_PAIR(CP_TITLE));

    if (status_text[0] && LINES > 5) {
        attron(COLOR_PAIR(CP_FOOTER) | A_DIM);
        mvaddnstr(LINES - 3, x, status_text,
                  COLS - x > 1 ? COLS - x - 1 : 0);
        attroff(COLOR_PAIR(CP_FOOTER) | A_DIM);
    }

    draw_footer();
    refresh();
}

/* ---------- Command helpers ---------- */

static int have_command(const char *name)
{
    const char *path_env = getenv("PATH");

    if (!path_env || !*path_env)
        return 0;

    char *copy = strdup(path_env);
    if (!copy)
        return 0;

    int found = 0;
    char *saveptr = NULL;

    for (char *dir = strtok_r(copy, ":", &saveptr);
         dir != NULL;
         dir = strtok_r(NULL, ":", &saveptr)) {

        char candidate[PATH_MAX];
        snprintf(candidate, sizeof(candidate), "%s/%s", dir, name);

        if (access(candidate, X_OK) == 0) {
            found = 1;
            break;
        }
    }

    free(copy);
    return found;
}

static int run_command(char *const argv[])
{
    int result = -1;
    pid_t pid = fork();

    if (pid < 0) {
        set_status("Could not start the command.");
        return -1;
    }

    if (pid == 0) {
        endwin();
        execvp(argv[0], argv);
        perror("execvp");
        _exit(127);
    }

    while (waitpid(pid, &result, 0) < 0) {
        if (errno != EINTR) {
            result = -1;
            break;
        }
    }

    reset_prog_mode();
    refresh();
    clear();
    refresh();

    if (result >= 0 && WIFEXITED(result))
        return WEXITSTATUS(result);

    return -1;
}

/* ---------- Messages ---------- */

static void pause_key(void)
{
    if (LINES < 2)
        return;

    attron(COLOR_PAIR(CP_FOOTER) | A_DIM);
    mvaddnstr(LINES - 2, 2, "Press any key to continue...",
              COLS > 4 ? COLS - 4 : 1);
    attroff(COLOR_PAIR(CP_FOOTER) | A_DIM);

    refresh();
    getch();
}

static void show_message(const char *title, const char *message)
{
    draw_header(title);

    int width = panel_width();
    int x = panel_x(width);
    int text_width = width - 4;

    if (text_width < 1)
        text_width = 1;

    int y = 5;
    int bottom = LINES - 4;

    const char *p = message ? message : "";

    while (*p && y < bottom) {
        if (*p == '\n') {
            y++;
            p++;
            continue;
        }

        while (*p == ' ')
            p++;

        if (!*p)
            break;

        int len = 0;
        while (p[len] && p[len] != '\n' && len < text_width)
            len++;

        if (p[len] && p[len] != '\n') {
            int j = len;
            while (j > 0 && p[j] != ' ')
                j--;

            if (j > 0)
                len = j;
        }

        if (len == 0)
            len = 1;

        mvaddnstr(y++, x + 2, p, len);
        p += len;

        while (*p == ' ')
            p++;
    }

    pause_key();
}

/* ---------- Validation and forms ---------- */

static int valid_host(const char *host)
{
    if (!host || !*host)
        return 0;

    for (const unsigned char *p = (const unsigned char *)host; *p; p++) {
        if (!(isalnum(*p) || *p == '.' || *p == '-' || *p == ':'))
            return 0;
    }

    return 1;
}

static int valid_port(const char *port)
{
    if (!port || !*port)
        return 0;

    for (const unsigned char *p = (const unsigned char *)port; *p; p++) {
        if (!isdigit(*p))
            return 0;
    }

    char *end = NULL;
    long value = strtol(port, &end, 10);

    return end && *end == '\0' && value >= 1 && value <= 65535;
}

static void edit_field(const char *label, char *buffer, size_t capacity,
                       int y, int x, int field_width)
{
    if (capacity < 2 || field_width < 1)
        return;

    size_t length = strlen(buffer);
    size_t cursor = length;

    curs_set(1);

    for (;;) {
        int label_len = (int)strlen(label);
        int input_x = x + label_len;
        int visible = field_width;

        if (input_x >= COLS - 1)
            break;

        if (visible > COLS - input_x - 1)
            visible = COLS - input_x - 1;

        if (visible < 1)
            visible = 1;

        mvaddnstr(y, x, label, label_len);

        attron(COLOR_PAIR(CP_FIELD));
        mvhline(y, input_x, ' ', visible);

        size_t start = 0;
        if (cursor >= (size_t)visible && visible > 0)
            start = cursor - (size_t)visible + 1;

        mvaddnstr(y, input_x, buffer + start, visible);
        attroff(COLOR_PAIR(CP_FIELD));

        int cursor_x = input_x + (int)(cursor - start);
        if (cursor_x >= input_x + visible)
            cursor_x = input_x + visible - 1;

        move(y, cursor_x);
        refresh();

        int ch = getch();

        if (ch == '\n' || ch == KEY_ENTER ||
            ch == KEY_UP || ch == KEY_DOWN || ch == '\t') {
            break;
        } else if (ch == 27) {
            break;
        } else if (ch == KEY_LEFT) {
            if (cursor > 0)
                cursor--;
        } else if (ch == KEY_RIGHT) {
            if (cursor < length)
                cursor++;
        } else if (ch == KEY_HOME || ch == 1) {
            cursor = 0;
        } else if (ch == KEY_END || ch == 5) {
            cursor = length;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (cursor > 0) {
                memmove(buffer + cursor - 1, buffer + cursor,
                        length - cursor + 1);
                cursor--;
                length--;
            }
        } else if (ch == KEY_DC) {
            if (cursor < length) {
                memmove(buffer + cursor, buffer + cursor + 1,
                        length - cursor);
                length--;
            }
        } else if (ch >= 32 && ch <= 126 && length + 1 < capacity) {
            memmove(buffer + cursor + 1, buffer + cursor,
                    length - cursor + 1);
            buffer[cursor++] = (char)ch;
            length++;
        }
    }

    curs_set(0);
}

static int form(const char *title,
                const char *host_label, char *host, size_t host_size,
                const char *port_label, char *port, size_t port_size)
{
    int focus = 0;

    for (;;) {
        draw_header(title);

        int width = panel_width();
        int x = panel_x(width);

        mvaddnstr(5, x, "Arrow keys: navigate   Enter: edit/select",
                  width);
        mvaddnstr(6, x, "Esc: cancel", width);

        int label_len = (int)strlen(host_label);
        int port_len = (int)strlen(port_label);
        int field_x = x + 2;

        if (focus == 0)
            attron(A_BOLD | A_UNDERLINE);
        mvaddnstr(8, field_x, host_label, width - 4);
        if (focus == 0)
            attroff(A_BOLD | A_UNDERLINE);

        attron(COLOR_PAIR(CP_FIELD));
        mvhline(9, field_x, ' ', width - 4);
        mvaddnstr(9, field_x, host, width - 4);
        attroff(COLOR_PAIR(CP_FIELD));

        if (focus == 1)
            attron(A_BOLD | A_UNDERLINE);
        mvaddnstr(11, field_x, port_label, width - 4);
        if (focus == 1)
            attroff(A_BOLD | A_UNDERLINE);

        attron(COLOR_PAIR(CP_FIELD));
        mvhline(12, field_x, ' ', width - 4);
        mvaddnstr(12, field_x, port, width - 4);
        attroff(COLOR_PAIR(CP_FIELD));

        (void)label_len;
        (void)port_len;

        if (focus == 2) {
            attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
            mvaddstr(15, x + 2, "[ Continue ]");
            attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        } else {
            attron(COLOR_PAIR(CP_MENU));
            mvaddstr(15, x + 2, "             ");
            mvaddstr(15, x + 2, "[ Continue ]");
            attroff(COLOR_PAIR(CP_MENU));
        }

        if (focus == 3) {
            attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
            mvaddstr(15, x + 19, "[ Cancel ]");
            attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        } else {
            attron(COLOR_PAIR(CP_MENU));
            mvaddstr(15, x + 19, "[ Cancel ]");
            attroff(COLOR_PAIR(CP_MENU));
        }

        draw_footer();
        refresh();

        int ch = getch();

        if (ch == 27)
            return 0;

        if (ch == KEY_UP) {
            focus = (focus + 3) % 4;
        } else if (ch == KEY_DOWN) {
            focus = (focus + 1) % 4;
        } else if (ch == KEY_LEFT || ch == KEY_RIGHT) {
            if (focus == 2)
                focus = 3;
            else if (focus == 3)
                focus = 2;
        } else if (ch == '\t') {
            focus = (focus + 1) % 4;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            if (focus == 0) {
                edit_field(host_label, host, host_size,
                           9, field_x, width - 4);
            } else if (focus == 1) {
                edit_field(port_label, port, port_size,
                           12, field_x, width - 4);
            } else if (focus == 3) {
                return 0;
            } else {
                if (!valid_host(host)) {
                    set_status("Enter a valid IP address or hostname.");
                } else if (!valid_port(port)) {
                    set_status("Enter a port from 1 to 65535.");
                } else {
                    set_status("");
                    return 1;
                }
            }
        }
    }
}

/* ---------- ADB pairing and connection ---------- */

static int adb_available(void)
{
    return have_command("adb");
}

static void pair_device(void)
{
    char host[IP_SIZE] = "";
    char port[PORT_SIZE] = "";

    snprintf(host, sizeof(host), "%s", last_ip);

    if (!form("Pair device",
              "Device IP: ", host, sizeof(host),
              "Pairing port: ", port, sizeof(port)))
        return;

    save_last_ip(host);

    char address[IP_SIZE + PORT_SIZE + 4];
    snprintf(address, sizeof(address), "%s:%s", host, port);

    char *const argv[] = {"adb", "pair", address, NULL};
    int rc = run_command(argv);

    show_message("Pair device",
        rc == 0
        ? "ADB pairing command completed. Follow any pairing-code prompt in the terminal."
        : "Pairing failed. Check the address, pairing port and pairing code.");
}

static void connect_device(void)
{
    char host[IP_SIZE] = "";
    char port[PORT_SIZE] = "";

    snprintf(host, sizeof(host), "%s", last_ip);

    if (!form("Connect device",
              "Device IP: ", host, sizeof(host),
              "Connection port: ", port, sizeof(port)))
        return;

    save_last_ip(host);

    char address[IP_SIZE + PORT_SIZE + 4];
    snprintf(address, sizeof(address), "%s:%s", host, port);

    char *const argv[] = {"adb", "connect", address, NULL};
    int rc = run_command(argv);

    show_message("Connect device",
        rc == 0
        ? "ADB connection command completed. Check its output for device status."
        : "Connection failed. Check the IP and connection port.");
}

/* ---------- APK downloading and installation ---------- */

static int download_file(const char *url, char *path, size_t path_size)
{
    const char *tmp = getenv("TMPDIR");

    if (!tmp || !*tmp)
        tmp = "/data/data/com.termux/files/usr/tmp";

    snprintf(path, path_size, "%s/ix-bootstrap-XXXXXX", tmp);

    int fd = mkstemp(path);
    if (fd < 0) {
        set_status("Could not create a temporary download file.");
        return 0;
    }
    close(fd);

    char *const argv[] = {
        "curl", "-fL", "--retry", "2", "--connect-timeout", "15",
        "-o", path, (char *)url, NULL
    };

    if (run_command(argv) != 0) {
        unlink(path);
        set_status("Download failed. Check your internet connection.");
        return 0;
    }

    return 1;
}

static int install_apk(const char *path)
{
    char *const argv[] = {"adb", "install", "-r", (char *)path, NULL};
    return run_command(argv) == 0;
}

static void install_fdroid(void)
{
    char path[PATH_MAX];

    set_status("Downloading F-Droid...");
    draw_header("Install / Upgrade F-Droid");

    if (!download_file("https://f-droid.org/F-Droid.apk",
                       path, sizeof(path))) {
        show_message("F-Droid", status_text);
        return;
    }

    set_status("Installing / upgrading F-Droid...");
    draw_header("Install / Upgrade F-Droid");

    int ok = install_apk(path);
    unlink(path);

    show_message("F-Droid",
        ok
        ? "ADB completed the install command. Review the terminal output for confirmation."
        : "Installation failed. Check that the device is connected and authorized.");
}

static int get_installerx_url(char *url, size_t url_size)
{
    FILE *p = popen(
        "curl -fsSL --connect-timeout 15 "
        "'https://api.github.com/repos/wxxsfxyzm/InstallerX-Revived/releases/latest' "
        "| jq -r '.assets[]? | select(.name | endswith(\".apk\")) | .browser_download_url' "
        "| head -n 1",
        "r");

    if (!p)
        return 0;

    if (!fgets(url, (int)url_size, p)) {
        pclose(p);
        return 0;
    }

    url[strcspn(url, "\r\n")] = '\0';
    int rc = pclose(p);

    return rc == 0 && strncmp(url, "https://", 8) == 0;
}

static void install_installerx(void)
{
    char url[2048] = "";
    char path[PATH_MAX];

    show_message("InstallerX",
        "Looking up the latest InstallerX Revived APK from GitHub...");

    if (!get_installerx_url(url, sizeof(url))) {
        show_message("InstallerX",
            "Could not find a downloadable APK in the latest release.\n"
            "Check your connection, curl and jq, then try again.");
        return;
    }

    if (!download_file(url, path, sizeof(path))) {
        show_message("InstallerX", status_text);
        return;
    }

    set_status("Installing / upgrading InstallerX Revived...");
    draw_header("Install / Upgrade InstallerX");

    int ok = install_apk(path);
    unlink(path);

    show_message("InstallerX",
        ok
        ? "ADB completed the install command. Review the terminal output for confirmation."
        : "Installation failed. Check that the device is connected and authorized.");
}

/* ---------- Help and About ---------- */

static void help_developer(void)
{
    show_message("Developer options",
        "1. Open Android Settings.\n"
        "2. Open About phone.\n"
        "3. Tap Build number repeatedly until Developer options are enabled.\n"
        "4. Return to Settings and open Developer options.\n"
        "5. Enable Wireless debugging.\n"
        "6. Use Pair device with the pairing port shown on your phone.\n\n"
        "Menu names may vary by Android version and manufacturer.");
}

static void help_shizuku(void)
{
    show_message("Shizuku help",
        "Shizuku allows supported apps to access certain system APIs.\n\n"
        "Wireless debugging setup:\n"
        "1. Enable Developer options and Wireless debugging.\n"
        "2. Open Shizuku and follow its instructions.\n"
        "3. Pair with the pairing port and code if requested.\n"
        "4. Start the service and check Shizuku's status screen.\n\n"
        "Pairing and connection ports are different. Use the correct port "
        "shown by Android. Disable wireless debugging when you no longer need it.");
}

static void about(void)
{
    show_message("About IX Bootstrap",
        "Its just a fkn third party apk installer and a f-droid store "
        "installer for android cause google thought it would be funny to "
        "put out yet another major f*ckup update....so anyways hope you "
        "guys like this program I made after a shower thought also pls "
        "follow me in instagram - 8bit_binary\n\n"
        "Made by precisescope (GitHub) and 8bit_binary (Instagram).");
}

/* ---------- Main menu ---------- */

static int menu(const char *title, const char *items[], int count)
{
    int selected = 0;

    for (;;) {
        draw_header(title);

        int width = panel_width();
        int x = panel_x(width);
        int y = 5;

        for (int i = 0; i < count; i++) {
            if (y + i >= LINES - 3)
                break;

            /*
             * Draw a white menu panel with black text.
             * The selected option gets a light-blue background.
             */
            if (colors_enabled) {
                attron(COLOR_PAIR(CP_MENU));
                mvhline(y + i, x, ' ', width);

                if (i == selected) {
                    attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
                    mvaddnstr(y + i, x + 2, items[i], width - 4);
                    attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
                } else {
                    mvaddnstr(y + i, x + 2, items[i], width - 4);
                }

                attroff(COLOR_PAIR(CP_MENU));
            } else {
                mvhline(y + i, x, ' ', width);

                if (i == selected)
                    attron(A_REVERSE | A_BOLD);

                mvaddnstr(y + i, x + 2, items[i], width - 4);

                if (i == selected)
                    attroff(A_REVERSE | A_BOLD);
            }
        }

        draw_footer();
        refresh();

        int ch = getch();

        if (ch == KEY_UP) {
            selected = (selected - 1 + count) % count;
        } else if (ch == KEY_DOWN) {
            selected = (selected + 1) % count;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            return selected;
        } else if (ch == 'q' || ch == 'Q' || ch == 27) {
            return count - 1;
        } else if (ch == KEY_RESIZE) {
            clear();
        }
    }
}

/* ---------- Entry point ---------- */

int main(void)
{
    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    intrflush(stdscr, FALSE);
    curs_set(0);

    init_theme();
    load_last_ip();

    const char *items[] = {
        "Pair device",
        "Connect device",
        "Install / Upgrade F-Droid",
        "Install / Upgrade InstallerX",
        "Developer options help",
        "Shizuku help",
        "About",
        "Exit"
    };

    int running = 1;
    int count = (int)(sizeof(items) / sizeof(items[0]));

    while (running) {
        int choice = menu("Main menu", items, count);

        switch (choice) {
        case 0:
            if (!adb_available()) {
                show_message("ADB missing",
                    "ADB was not found.\n\n"
                    "Install it in Termux with:\n"
                    "pkg install android-tools");
            } else {
                pair_device();
            }
            break;

        case 1:
            if (!adb_available()) {
                show_message("ADB missing",
                    "ADB was not found.\n\n"
                    "Install it in Termux with:\n"
                    "pkg install android-tools");
            } else {
                connect_device();
            }
            break;

        case 2:
            if (!adb_available()) {
                show_message("ADB missing",
                    "Install ADB with: pkg install android-tools");
            } else if (!have_command("curl")) {
                show_message("Dependency missing",
                    "Install curl with: pkg install curl");
            } else {
                install_fdroid();
            }
            break;

        case 3:
            if (!adb_available()) {
                show_message("ADB missing",
                    "Install ADB with: pkg install android-tools");
            } else if (!have_command("curl") || !have_command("jq")) {
                show_message("Dependencies missing",
                    "Install the required tools with:\n\n"
                    "pkg install curl jq");
            } else {
                install_installerx();
            }
            break;

        case 4:
            help_developer();
            break;

        case 5:
            help_shizuku();
            break;

        case 6:
            about();
            break;

        default:
            running = 0;
            break;
        }
    }

    endwin();
    return 0;
}

