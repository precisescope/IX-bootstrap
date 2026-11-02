#include <locale.h>
#define _POSIX_C_SOURCE 200809L
#include <ncurses.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>

#define W 54
#define BUF 256

static char ip[BUF] = "";
static char pairport[16] = "";
static char connport[16] = "";
static const char *cache = NULL;

static void drawbox(int y, int x, int h, int w) {
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x+w-1, ACS_URCORNER);
    mvaddch(y+h-1, x, ACS_LLCORNER);
    mvaddch(y+h-1, x+w-1, ACS_LRCORNER);
    for (int i=1; i<w-1; i++) {
        mvaddch(y, x+i, ACS_HLINE);
        mvaddch(y+h-1, x+i, ACS_HLINE);
    }
    for (int i=1; i<h-1; i++) {
        mvaddch(y+i, x, ACS_VLINE);
        mvaddch(y+i, x+w-1, ACS_VLINE);
    }
}

static void screen(const char *title) {
    erase();
    int x = (COLS-W)/2;
    if (x < 0) x = 0;
    drawbox(1, x, 4, W);
    mvprintw(2, x+(W-(int)strlen(title))/2, "%s", title);
    mvprintw(3, x+(W-24)/2, "Android ADB Setup Utility");
    refresh();
}

static void message(const char *title, const char *body) {
    screen(title);
    mvprintw(7, 3, "%s", body);
    mvprintw(LINES-2, 3, "Press any key to continue...");
    refresh();
    getch();
}

static int menu(const char *title, const char **items, int count) {
    int selected=0, key;
    while (1) {
        screen(title);
        mvprintw(6, 3, "Use UP/DOWN and Enter.");
        for (int i=0; i<count; i++) {
            if (i==selected) attron(A_REVERSE);
            mvprintw(8+i, 4, "%s", items[i]);
            if (i==selected) attroff(A_REVERSE);
        }
        mvprintw(LINES-2, 3, "Enter: select   Esc: exit/back");
        refresh();
        key=getch();
        if (key==KEY_UP) selected=(selected+count-1)%count;
        else if (key==KEY_DOWN) selected=(selected+1)%count;
        else if (key=='\n' || key==KEY_ENTER || key==KEY_RIGHT)
            return selected;
        else if (key==27) return count-1;
    }
}

static void edit(char *buf, size_t cap) {
    int c;
    size_t n=strlen(buf);
    curs_set(1);
    while ((c=getch()) != '\n' && c != KEY_ENTER) {
        if (c==27) { buf[0]='\0'; break; }
        if (c==KEY_BACKSPACE || c==127 || c==8) {
            if (n) buf[--n]='\0';
        } else if (c>=32 && c<127 && n+1<cap) {
            buf[n++]=(char)c;
            buf[n]='\0';
        }
        addch('\r');
        clrtoeol();
        addnstr(buf, (int)cap-1);
        refresh();
    }
    curs_set(0);
}

static int valid_host(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!(isalnum((unsigned char)*s) ||
              *s=='.' || *s==':' || *s=='-' || *s=='_'))
            return 0;
    return 1;
}

static int valid_port(const char *s) {
    char *end;
    long p;
    if (!*s) return 0;
    for (const char *q=s; *q; q++)
        if (!isdigit((unsigned char)*q)) return 0;
    p=strtol(s,&end,10);
    return *end=='\0' && p>=1 && p<=65535;
}

static int form(const char *title, const char *portlabel,
                char *host, char *port) {
    int focus=0, button=1, key;
    for (;;) {
        screen(title);
        drawbox(6, 3, 8, W);
        mvprintw(7, 6, "%c Device IP address", focus==0?'>':' ');
        mvprintw(8, 6, "[ %-42.42s ]", host);
        mvprintw(10, 6, "%c %s", focus==1?'>':' ', portlabel);
        mvprintw(11, 6, "[ %-42.42s ]", port);
        if (button==0) attron(A_REVERSE);
        mvprintw(16, 12, " Cancel ");
        if (button==0) attroff(A_REVERSE);
        if (button==1) attron(A_REVERSE);
        mvprintw(16, 28, " Continue ");
        if (button==1) attroff(A_REVERSE);
        mvprintw(19, 3, "UP/DOWN: field  LEFT/RIGHT: button");
        mvprintw(20, 3, "Enter: edit/select  Esc: cancel");
        refresh();

        key=getch();
        if (key==27) return 0;
        if (key==KEY_UP) focus=0;
        else if (key==KEY_DOWN) focus=1;
        else if (key==KEY_LEFT) button=0;
        else if (key==KEY_RIGHT) button=1;
        else if (key=='\t') focus=(focus+1)%2;
        else if (key=='\n' || key==KEY_ENTER) {
            if (button==0) return 0;
            if (!valid_host(host)) {
                message("Invalid IP", "Enter a valid IP address or hostname.");
                continue;
            }
            if (!valid_port(port)) {
                message("Invalid port", "Port must be between 1 and 65535.");
                continue;
            }
            return 1;
        } else if (key==KEY_BACKSPACE || key==127) {
            char *s=focus?port:host;
            size_t n=strlen(s);
            if(n) s[n-1]='\0';
        } else if (key>=32 && key<127) {
            char *s=focus?port:host;
            size_t cap=focus?16:BUF;
            size_t n=strlen(s);
            if(n+1<cap) { s[n]=(char)key; s[n+1]='\0'; }
        }
    }
}

static int run(char *const argv[], int quiet) {
    pid_t pid=fork();
    if(pid<0) return -1;
    if(pid==0) {
        if(quiet) {
            freopen("/dev/null","w",stdout);
            freopen("/dev/null","w",stderr);
        } else {
            def_prog_mode();
            endwin();
        }
        execvp(argv[0],argv);
        _exit(127);
    }
    int status;
    waitpid(pid,&status,0);
    if(!quiet) {
        reset_prog_mode();
        refresh();
    }
    return WIFEXITED(status)?WEXITSTATUS(status):-1;
}

static int have_adb(void) {
    char *a[]={"adb","version",NULL};
    if(run(a,1)!=0) {
        message("ADB missing","Install it with: pkg install android-tools");
        return 0;
    }
    return 1;
}

static int connected(void) {
    char *a[]={"adb","devices",NULL};
    return run(a,0)==0;
}

static void pair_device(void) {
    if(!have_adb()) return;
    if(!form("Pair device with ADB","Pairing port",ip,pairport)) return;

    screen("ADB pairing");
    mvprintw(6,3,"Enter the pairing code shown by Android.");
    mvprintw(7,3,"Wireless debugging > Pair device with pairing code");
    mvprintw(9,3,"Pairing code: ");
    refresh();

    char code[64]="";
    echo();
    curs_set(1);
    getnstr(code, (int)sizeof(code)-1);
    noecho();
    curs_set(0);
    if(!*code) return;

    char addr[BUF+24];
    snprintf(addr,sizeof(addr),"%s:%s",ip,pairport);
    char *a[]={"adb","pair",addr,code,NULL};
    int r=run(a,0);
    memset(code,0,sizeof(code));
    message(r==0?"Pairing complete":"Pairing failed",
            r==0?"If successful, use option 2 with the connection port."
                :"Check the pairing port and temporary pairing code.");
}

static void connect_device(void) {
    if(!have_adb()) return;
    if(!form("Connect device with ADB","Connection port",ip,connport))
        return;
    char addr[BUF+24];
    snprintf(addr,sizeof(addr),"%s:%s",ip,connport);
    char *a[]={"adb","connect",addr,NULL};
    int r=run(a,0);
    message(r==0?"ADB connection command finished":"Connection failed",
            r==0?"Check adb devices to confirm authorization."
                :"Check the IP, connection port and Wireless debugging.");
}

static int download_url(const char *url, const char *path) {
    char *a[]={"curl","-fL","--proto","=https","--tlsv1.2",
               "--connect-timeout","20","--max-time","180",
               "-o",(char *)path,(char *)url,NULL};
    return run(a,0);
}

static void install_app(int fdroid) {
    if(!have_adb()) return;

    char *d[]={"adb","devices",NULL};
    run(d,0);

    const char *name=fdroid?"F-Droid":"InstallerX Revived";
    char path[PATH_MAX];
    snprintf(path,sizeof(path),"%s/%s.apk",
             getenv("HOME"),fdroid?"F-Droid":"InstallerX-Revived");

    screen(name);
    mvprintw(6,3,"Download the latest stable APK?");
    mvprintw(8,3,"Source: %s",
        fdroid?"https://f-droid.org/F-Droid.apk":
        "InstallerX Revived GitHub Releases");
    mvprintw(11,3,"Press Y to continue, any other key to cancel.");
    refresh();
    int c=getch();
    if(c!='y' && c!='Y') return;

    if(fdroid) {
        if(download_url("https://f-droid.org/F-Droid.apk",path)!=0) {
            message("Download failed","Check your internet connection.");
            return;
        }
    } else {
        char *a[]={"sh","-c",
          "curl -fsSL --proto '=https' "
          "https://api.github.com/repos/wxxsfxyzm/InstallerX-Revived/releases/latest "
          "| jq -er '[.assets[] | select(.name|endswith(\".apk\")) | .browser_download_url] | first'",
          NULL};
        FILE *fp=popen("curl -fsSL --proto '=https' https://api.github.com/repos/wxxsfxyzm/InstallerX-Revived/releases/latest | jq -er '[.assets[] | select(.name|endswith(\".apk\")) | .browser_download_url] | first'","r");
        char url[2048]="";
        if(!fp || !fgets(url,sizeof(url),fp)) {
            if(fp) pclose(fp);
            message("Release lookup failed","Could not find a stable APK URL.");
            return;
        }
        int pr=pclose(fp);
        url[strcspn(url,"\r\n")]=0;
        if(pr!=0 || strncmp(url,"https://",8)!=0) {
            message("Release lookup failed","GitHub returned an invalid APK URL.");
            return;
        }
        (void)a;
        if(download_url(url,path)!=0) {
            message("Download failed","Could not download the release APK.");
            return;
        }
    }

    struct stat st;
    if(stat(path,&st)!=0 || st.st_size<100000) {
        unlink(path);
        message("Invalid APK","The downloaded file is missing or too small.");
        return;
    }

    screen("Confirm installation");
    mvprintw(6,3,"App: %s",name);
    mvprintw(7,3,"File: %s",path);
    mvprintw(9,3,"Android will check the APK signature.");
    mvprintw(11,3,"Install now? Press Y to continue.");
    refresh();
    c=getch();
    if(c!='y' && c!='Y') return;

    char *ins[]={"adb","install","-r",path,NULL};
    int r=run(ins,0);
    message(r==0?"Installation successful":"Installation failed",
            r==0?"ADB reported success."
                :"Review the ADB error output and verify the APK source.");
}

static void help_developer(void) {
    screen("How to enable Developer options");
    mvprintw(6,3,"1. Open Settings > About phone.");
    mvprintw(7,3,"2. Find Build number (possibly Software info).");
    mvprintw(8,3,"3. Tap Build number seven times.");
    mvprintw(9,3,"4. Enter your screen lock if requested.");
    mvprintw(10,3,"5. Return to Settings > Developer options.");
    mvprintw(11,3,"6. Enable Wireless debugging.");
    mvprintw(13,3,"Menu names vary by phone manufacturer.");
    mvprintw(LINES-2,3,"Press any key to return...");
    refresh();
    getch();
}

static void help_shizuku(void) {
    screen("How to pair Shizuku with your phone");
    mvprintw(6,3,"1. Install and open Shizuku.");
    mvprintw(7,3,"2. Enable Developer options and Wireless debugging.");
    mvprintw(8,3,"3. In Shizuku, select Start via Wireless debugging.");
    mvprintw(9,3,"4. Follow Shizuku's pairing instructions.");
    mvprintw(10,3,"5. Enter Android's pairing code in Shizuku.");
    mvprintw(11,3,"6. Return to Shizuku and tap Start.");
    mvprintw(13,3,"Guide: shizuku.rikka.app/guide/setup/");
    mvprintw(LINES-2,3,"Press any key to return...");
    refresh();
    getch();
}

int main(void) {
    const char *options[]={
        "Pair device with ADB",
        "Connect device with ADB",
        "Install F-Droid (latest stable)",
        "Install InstallerX (latest stable)",
        "How to enable Developer options",
        "How to pair Shizuku with your phone",
        "Exit"
    };

    setlocale(LC_ALL,"");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr,TRUE);
    curs_set(0);

    while(1) {
        int c=menu("IX Bootstrap",options,7);
        switch(c) {
            case 0: pair_device(); break;
            case 1: connect_device(); break;
            case 2: install_app(1); break;
            case 3: install_app(0); break;
            case 4: help_developer(); break;
            default:
                if(c==5) help_shizuku();
                else { endwin(); return 0; }
        }
    }
}
