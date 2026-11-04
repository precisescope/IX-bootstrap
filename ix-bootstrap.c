
#define _POSIX_C_SOURCE 200809L

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#define APP_NAME "IX-BOOTSTRAP"
#define APP_VERSION "2.0"
#define FDROID_URL "https://f-droid.org/F-Droid.apk"
#define INSTALLERX_API \
    "https://api.github.com/repos/wxxsfxyzm/InstallerX-Revived/releases/latest"

#define CONFIG_DIR ".config/ix-bootstrap"
#define IP_FILE ".config/ix-bootstrap/last_ip"

#define HOST_SIZE 256
#define PORT_SIZE 16

enum {
    CP_NORMAL = 1,
    CP_PANEL,
    CP_TITLE,
    CP_SELECTED,
    CP_MUTED,
    CP_ACCENT,
    CP_BORDER,
    CP_INPUT
};

static const char *menu_items[] = {
    "Pair device",
    "Connect device",
    "Install / Upgrade F-Droid",
    "Install / Upgrade InstallerX",
    "Developer options help",
    "Shizuku help",
    "About",
    "Exit"
};

#define MENU_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))

static int use_colors = 0;

/* ---------------------------------------------------------
   Configuration
   --------------------------------------------------------- */

static void get_config_path(char *out, size_t size,
                            const char *filename)
{
    const char *home = getenv("HOME");

    if (!home)
        home = ".";

    snprintf(out, size, "%s/%s/%s",
             home, CONFIG_DIR, filename);
}

static void ensure_config_dir(void)
{
    char path[PATH_MAX];
    const char *home = getenv("HOME");

    if (!home)
        home = ".";

    snprintf(path, sizeof(path), "%s/.config", home);
    mkdir(path, 0700);

    snprintf(path, sizeof(path), "%s/.config/ix-bootstrap", home);
    mkdir(path, 0700);
}

static void save_last_ip(const char *ip)
{
    char path[PATH_MAX];
    FILE *fp;

    ensure_config_dir();
    get_config_path(path, sizeof(path), "last_ip");

    fp = fopen(path, "w");

    if (!fp)
        return;

    fprintf(fp, "%s\n", ip);
    fclose(fp);
}

static void load_last_ip(char *ip, size_t size)
{
    char path[PATH_MAX];
    FILE *fp;

    ip[0] = '\0';

    get_config_path(path, sizeof(path), "last_ip");

    fp = fopen(path, "r");

    if (!fp)
        return;

    if (fgets(ip, (int)size, fp)) {
        ip[strcspn(ip, "\r\n")] = '\0';
    }

    fclose(fp);
}

/* ---------------------------------------------------------
   Terminal theme
   --------------------------------------------------------- */

static void init_theme(void)
{
    if (!has_colors())
        return;

    start_color();
    use_default_colors();

    init_pair(CP_NORMAL, COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_PANEL, COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_TITLE, COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_SELECTED, COLOR_BLACK, COLOR_CYAN);
    init_pair(CP_MUTED, COLOR_CYAN, COLOR_BLACK);
    init_pair(CP_ACCENT, COLOR_CYAN, COLOR_BLACK);
    init_pair(CP_BORDER, COLOR_BLUE, COLOR_BLACK);
    init_pair(CP_INPUT, COLOR_WHITE, COLOR_BLUE);

    bkgd(COLOR_PAIR(CP_NORMAL));

    use_colors = 1;
}

static void set_color(int pair)
{
    if (use_colors)
        attron(COLOR_PAIR(pair));
}

static void unset_color(int pair)
{
    if (use_colors)
        attroff(COLOR_PAIR(pair));
}

static void draw_centered(int y, const char *text, int pair)
{
    int rows, cols;
    int x;

    getmaxyx(stdscr, rows, cols);

    (void)rows;

    x = (cols - (int)strlen(text)) / 2;

    if (x < 0)
        x = 0;

    set_color(pair);
    mvaddnstr(y, x, text, cols - x);
    unset_color(pair);
}

static void draw_box(int y, int x, int height, int width)
{
    int i;

    if (height < 2 || width < 2)
        return;

    set_color(CP_BORDER);

    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + width - 1, ACS_URCORNER);
    mvaddch(y + height - 1, x, ACS_LLCORNER);
    mvaddch(y + height - 1, x + width - 1, ACS_LRCORNER);

    for (i = 1; i < width - 1; i++) {
        mvaddch(y, x + i, ACS_HLINE);
        mvaddch(y + height - 1, x + i, ACS_HLINE);
    }

    for (i = 1; i < height - 1; i++) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + width - 1, ACS_VLINE);
    }

    unset_color(CP_BORDER);
}

static void draw_header(void)
{
    int rows, cols;

    getmaxyx(stdscr, rows, cols);

    erase();

    draw_centered(1, APP_NAME, CP_TITLE);
    draw_centered(2, "ANDROID DEVICE MANAGEMENT TOOL", CP_MUTED);

    if (rows > 4 && cols > 10) {
        set_color(CP_BORDER);
        mvhline(4, 2, ACS_HLINE, cols - 4);
        unset_color(CP_BORDER);
    }
}

static void draw_footer(void)
{
    int rows, cols;

    getmaxyx(stdscr, rows, cols);

    if (rows < 3)
        return;

    set_color(CP_MUTED);
    mvaddnstr(rows - 3, 2,
        "ARROWS: Navigate   ENTER: Select   ESC: Back",
        cols - 4);
    mvaddnstr(rows - 2, 2,
        "Made by precisescope (GitHub) | 8bit_binary (Instagram)",
        cols - 4);
    unset_color(CP_MUTED);
}

/* ---------------------------------------------------------
   Input and utility functions
   --------------------------------------------------------- */

static void pause_key(void)
{
    int rows, cols;

    getmaxyx(stdscr, rows, cols);

    set_color(CP_MUTED);

    if (rows > 2)
        mvaddstr(rows - 4, 2, "Press any key to continue...");

    unset_color(CP_MUTED);

    refresh();
    getch();
}

static void show_message(const char *message)
{
    int rows, cols;

    getmaxyx(stdscr, rows, cols);

    draw_header();

    if (rows > 8)
        draw_centered(rows / 2, message, CP_ACCENT);

    draw_footer();
    refresh();
    pause_key();
}

static int have_command(const char *command)
{
    char *path = getenv("PATH");
    char *copy;
    char *token;

    if (!path)
        return 0;

    copy = strdup(path);

    if (!copy)
        return 0;

    token = strtok(copy, ":");

    while (token) {
        char candidate[PATH_MAX];

        snprintf(candidate, sizeof(candidate),
                 "%s/%s", token, command);

        if (access(candidate, X_OK) == 0) {
            free(copy);
            return 1;
        }

        token = strtok(NULL, ":");
    }

    free(copy);
    return 0;
}

static int valid_port(const char *port)
{
    char *end;
    long value;

    if (!port || !*port)
        return 0;

    errno = 0;
    value = strtol(port, &end, 10);

    return errno == 0 &&
           *end == '\0' &&
           value >= 1 &&
           value <= 65535;
}

static int valid_host(const char *host)
{
    size_t i;

    if (!host || !*host)
        return 0;

    if (strlen(host) >= HOST_SIZE)
        return 0;

    for (i = 0; host[i]; i++) {
        unsigned char c = (unsigned char)host[i];

        if (!(c == '.' || c == ':' || c == '-' ||
              c == '_' ||
              (c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z')))
            return 0;
    }

    return 1;
}

static int ask_field(const char *label, char *buffer,
                     size_t size, const char *default_value)
{
    int rows, cols;
    int y;

    getmaxyx(stdscr, rows, cols);

    draw_header();

    y = rows / 2 - 2;

    set_color(CP_TITLE);
    mvaddstr(y, 4, label);
    unset_color(CP_TITLE);

    set_color(CP_INPUT);
    mvaddstr(y + 2, 4, "> ");
    unset_color(CP_INPUT);

    if (default_value && *default_value)
        snprintf(buffer, size, "%s", default_value);
    else
        buffer[0] = '\0';

    echo();
    curs_set(1);

    mvaddnstr(y + 2, 6, buffer, cols - 10);
    move(y + 2, 6 + (int)strlen(buffer));

    getnstr(buffer, (int)size - 1);

    noecho();
    curs_set(0);

    if (buffer[0] == '\0' && default_value)
        snprintf(buffer, size, "%s", default_value);

    return 1;
}

/* ---------------------------------------------------------
   External commands
   --------------------------------------------------------- */

static int run_command(char *const argv[])
{
    pid_t pid;
    int status;

    def_prog_mode();
    endwin();

    pid = fork();

    if (pid < 0) {
        reset_prog_mode();
        refresh();
        show_message("Could not start process.");
        return -1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        _exit(127);
    }

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            break;
    }

    reset_prog_mode();
    clear();
    refresh();

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return -1;
}

static int check_adb(void)
{
    if (!have_command("adb")) {
        show_message("ADB not found. Install android-tools first.");
        return 0;
    }

    return 1;
}

/* ---------------------------------------------------------
   ADB pairing and connection
   --------------------------------------------------------- */

static void pair_device(void)
{
    char ip[HOST_SIZE];
    char port[PORT_SIZE];
    char address[HOST_SIZE + PORT_SIZE + 4];
    char previous[HOST_SIZE];

    char *argv[4];

    if (!check_adb())
        return;

    load_last_ip(previous, sizeof(previous));

    ask_field("Device IP address", ip, sizeof(ip), previous);

    if (!valid_host(ip)) {
        show_message("Invalid device address.");
        return;
    }

    ask_field("Pairing port", port, sizeof(port), "");

    if (!valid_port(port)) {
        show_message("Invalid pairing port.");
        return;
    }

    snprintf(address, sizeof(address), "%s:%s", ip, port);

    argv[0] = "adb";
    argv[1] = "pair";
    argv[2] = address;
    argv[3] = NULL;

    save_last_ip(ip);

    run_command(argv);
    show_message("Pairing command finished.");
}

static void connect_device(void)
{
    char ip[HOST_SIZE];
    char port[PORT_SIZE];
    char address[HOST_SIZE + PORT_SIZE + 4];
    char previous[HOST_SIZE];

    char *argv[4];

    if (!check_adb())
        return;

    load_last_ip(previous, sizeof(previous));

    ask_field("Device IP address", ip, sizeof(ip), previous);

    if (!valid_host(ip)) {
        show_message("Invalid device address.");
        return;
    }

    ask_field("Connection port", port, sizeof(port), "");

    if (!valid_port(port)) {
        show_message("Invalid connection port.");
        return;
    }

    snprintf(address, sizeof(address), "%s:%s", ip, port);

    argv[0] = "adb";
    argv[1] = "connect";
    argv[2] = address;
    argv[3] = NULL;

    save_last_ip(ip);

    run_command(argv);
    show_message("Connection command finished.");
}

/* ---------------------------------------------------------
   APK downloading and installation
   --------------------------------------------------------- */

static int download_file(const char *url, char *output,
                         size_t output_size)
{
    char template_path[PATH_MAX];
    int fd;
    char *argv[6];

    const char *tmp = getenv("TMPDIR");

    if (!tmp || !*tmp)
        tmp = "/data/data/com.termux/files/usr/tmp";

    snprintf(template_path, sizeof(template_path),
             "%s/ix-bootstrap-XXXXXX", tmp);

    fd = mkstemp(template_path);

    if (fd < 0) {
        snprintf(template_path, sizeof(template_path),
                 "/tmp/ix-bootstrap-XXXXXX");

        fd = mkstemp(template_path);
    }

    if (fd < 0) {
        show_message("Could not create temporary file.");
        return 0;
    }

    close(fd);

    argv[0] = "curl";
    argv[1] = "-fL";
    argv[2] = "--progress-bar";
    argv[3] = "-o";
    argv[4] = template_path;
    argv[5] = (char *)url;

    {
        char *command[] = {
            "curl", "-fL", "--progress-bar",
            "-o", template_path, (char *)url, NULL
        };

        if (run_command(command) != 0) {
            unlink(template_path);
            show_message("Download failed.");
            return 0;
        }
    }

    snprintf(output, output_size, "%s", template_path);
    return 1;
}

static void install_apk(const char *url)
{
    char path[PATH_MAX];
    char *argv[5];

    if (!check_adb())
        return;

    if (!have_command("curl")) {
        show_message("curl is not installed.");
        return;
    }

    if (!download_file(url, path, sizeof(path)))
        return;

    argv[0] = "adb";
    argv[1] = "install";
    argv[2] = "-r";
    argv[3] = path;
    argv[4] = NULL;

    run_command(argv);

    unlink(path);

    show_message("APK installation command finished.");
}

static void install_fdroid(void)
{
    install_apk(FDROID_URL);
}

static int get_installerx_url(char *url, size_t size)
{
    FILE *pipe;
    char line[2048];

    const char *command =
        "curl -fsSL '" INSTALLERX_API "' | "
        "jq -r '.assets[] | "
        "select(.name | endswith(\".apk\")) | "
        ".browser_download_url' | head -n 1";

    pipe = popen(command, "r");

    if (!pipe)
        return 0;

    if (!fgets(line, sizeof(line), pipe)) {
        pclose(pipe);
        return 0;
    }

    pclose(pipe);

    line[strcspn(line, "\r\n")] = '\0';

    if (strncmp(line, "https://", 8) != 0)
        return 0;

    snprintf(url, size, "%s", line);

    return 1;
}

static void install_installerx(void)
{
    char url[2048];

    if (!have_command("curl") || !have_command("jq")) {
        show_message("Install curl and jq first.");
        return;
    }

    draw_header();
    draw_centered(7, "Checking latest InstallerX release...",
                  CP_ACCENT);
    draw_footer();
    refresh();

    if (!get_installerx_url(url, sizeof(url))) {
        show_message("Could not find an InstallerX APK release.");
        return;
    }

    install_apk(url);
}

/* ---------------------------------------------------------
   Information pages
   --------------------------------------------------------- */

static void information_page(const char *heading,
                             const char *const lines[],
                             int count)
{
    int rows, cols;
    int i;

    draw_header();

    getmaxyx(stdscr, rows, cols);

    set_color(CP_TITLE);
    mvaddnstr(6, 4, heading, cols - 8);
    unset_color(CP_TITLE);

    for (i = 0; i < count && i < rows - 12; i++) {
        set_color(CP_NORMAL);
        mvaddnstr(8 + i, 4, lines[i], cols - 8);
        unset_color(CP_NORMAL);
    }

    draw_footer();
    refresh();
    pause_key();
}

static void developer_help(void)
{
    const char *lines[] = {
        "1. Open Android Settings.",
        "2. Find About phone.",
        "3. Open Software information if needed.",
        "4. Tap Build number repeatedly.",
        "5. Return to Settings.",
        "6. Open Developer options.",
        "7. Enable Wireless debugging.",
        "",
        "Pairing and connection use separate ports.",
        "Keep your device and Termux on the same network."
    };

    information_page("DEVELOPER OPTIONS HELP",
                     lines,
                     sizeof(lines) / sizeof(lines[0]));
}

static void shizuku_help(void)
{
    const char *lines[] = {
        "Shizuku allows supported apps to use system APIs.",
        "",
        "1. Install Shizuku on your Android device.",
        "2. Enable Developer options.",
        "3. Enable Wireless debugging.",
        "4. Open Shizuku and choose Wireless debugging.",
        "5. Follow its pairing instructions.",
        "6. Start the Shizuku service.",
        "",
        "Some apps require Shizuku to be running.",
        "ADB pairing and Shizuku setup are separate steps."
    };

    information_page("SHIZUKU HELP",
                     lines,
                     sizeof(lines) / sizeof(lines[0]));
}

static void about_page(void)
{
    const char *lines[] = {
        "IX Bootstrap 2.0",
        "",
        "Its just a fkn third party apk installer and a",
        "f-droid store installer for android cause google",
        "thought it would be funny to put out yet another",
        "major f*ckup update....so anyways hope you guys",
        "like this program I made after a shower thought",
        "also pls follow me in instagram - 8bit_binary",
        "",
        "Made by precisescope (GitHub)."
    };

    information_page("ABOUT IX BOOTSTRAP",
                     lines,
                     sizeof(lines) / sizeof(lines[0]));
}

/* ---------------------------------------------------------
   Main menu
   --------------------------------------------------------- */

static void draw_menu(int selected)
{
    int rows, cols;
    int panel_width;
    int panel_x;
    int panel_y = 6;
    int i;

    getmaxyx(stdscr, rows, cols);

    draw_header();

    panel_width = cols - 8;

    if (panel_width > 66)
        panel_width = 66;

    if (panel_width < 20)
        panel_width = 20;

    panel_x = (cols - panel_width) / 2;

    if (rows < 17 || cols < 28) {
        draw_centered(rows / 2,
            "Terminal too small - resize to continue.",
            CP_ACCENT);

        draw_footer();
        refresh();
        return;
    }

    draw_box(panel_y, panel_x, 12, panel_width);

    set_color(CP_MUTED);
    mvaddnstr(panel_y + 1, panel_x + 3,
              "MAIN MENU", panel_width - 6);
    unset_color(CP_MUTED);

    for (i = 0; i < (int)MENU_COUNT; i++) {
        int y = panel_y + 3 + i;

        if (i == selected) {
            set_color(CP_SELECTED);
            mvhline(y, panel_x + 2, ' ', panel_width - 4);
            mvaddnstr(y, panel_x + 4,
                      "> ", 2);
            mvaddnstr(y, panel_x + 6,
                      menu_items[i], panel_width - 10);
            unset_color(CP_SELECTED);
        } else {
            set_color(CP_NORMAL);
            mvaddnstr(y, panel_x + 4,
                      "  ", 2);
            mvaddnstr(y, panel_x + 6,
                      menu_items[i], panel_width - 10);
            unset_color(CP_NORMAL);
        }
    }

    draw_footer();
    refresh();
}

static int menu(void)
{
    int selected = 0;
    int ch;
    int rows, cols;

    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, NULL);
    mouseinterval(150);

    while (1) {
        draw_menu(selected);

        ch = getch();

        switch (ch) {
        case KEY_UP:
            selected--;

            if (selected < 0)
                selected = (int)MENU_COUNT - 1;

            break;

        case KEY_DOWN:
            selected++;

            if (selected >= (int)MENU_COUNT)
                selected = 0;

            break;

        case '\n':
        case KEY_ENTER:
            return selected;

        case 27:
            return (int)MENU_COUNT - 1;

        case KEY_RESIZE:
            clear();
            break;

        case KEY_MOUSE:
            {
                MEVENT event;

                if (getmouse(&event) == OK &&
                    (event.bstate & BUTTON1_CLICKED)) {

                    getmaxyx(stdscr, rows, cols);

                    (void)cols;

                    if (rows >= 17) {
                        int y = event.y;
                        int index = y - 9;

                        if (index >= 0 &&
                            index < (int)MENU_COUNT) {
                            selected = index;
                            return selected;
                        }
                    }
                }
            }
            break;

        default:
            if (ch == 'q' || ch == 'Q')
                return (int)MENU_COUNT - 1;

            break;
        }
    }
}

/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */

int main(void)
{
    int choice;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    init_theme();
    ensure_config_dir();

    while (1) {
        choice = menu();

        switch (choice) {
        case 0:
            pair_device();
            break;

        case 1:
            connect_device();
            break;

        case 2:
            install_fdroid();
            break;

        case 3:
            install_installerx();
            break;

        case 4:
            developer_help();
            break;

        case 5:
            shizuku_help();
            break;

        case 6:
            about_page();
            break;

        case 7:
        default:
            endwin();
            printf("%s %s\n", APP_NAME, APP_VERSION);
            printf("Thanks for using IX Bootstrap!\n");
            return 0;
        }
    }
}

