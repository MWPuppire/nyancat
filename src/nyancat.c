#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE 1
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define __BSD_VISIBLE 1
#include <ctype.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifndef TIOCGWINSZ
#include <termios.h>
#endif
#ifdef ECHO
#undef ECHO
#endif
#include <animation.h>
#include <mikmod.h>
#include <music.h>

const char *colors[256] = {NULL};
const char *output = "  ";
int show_counter = 1;
unsigned int frame_count = 0;
int clear_screen = 1;

static double diff_in_second(struct timespec t1, struct timespec t2) {
    struct timespec diff;
    if (t2.tv_nsec - t1.tv_nsec < 0) {
        diff.tv_sec = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec + diff.tv_nsec / 1000000000.0);
}

static struct timespec start, end;

pthread_mutex_t print_lock;

int digits(int val) {
    int d = 1, c;
    if (val >= 0) {
        for (c = 10; c <= val; c *= 10) {
            d++;
        }
    } else {
        for (c = -10; c >= val; c *= 10) {
            d++;
        }
    }
    return (c < 0) ? ++d : d;
}

int min_row = -1;
int max_row = -1;
int min_col = -1;
int max_col = -1;
char *term = NULL;
int terminal_width = 80;
int terminal_height = 24;
char using_automatic_width = 0;
char using_automatic_height = 0;

void finish() {
    if (clear_screen) {
        printf("\033[?25h\033[0m\033[H\033[2J");
    } else {
        printf("\033[0m\n");
    }
    pthread_mutex_destroy(&print_lock);
    exit(0);
}

void SIGINT_handler(int sig) {
    (void) sig;
    finish();
}

void update_window_size() {
    if (using_automatic_width) {
        min_col = (FRAME_WIDTH - terminal_width / 2) / 2;
        max_col = (FRAME_WIDTH + terminal_width / 2) / 2;
    }
    if (using_automatic_height) {
        min_row = (FRAME_HEIGHT - (terminal_height - 1)) / 2;
        max_row = (FRAME_HEIGHT + (terminal_height - 1)) / 2;
    }
}

void SIGWINCH_handler(int sig) {
    (void) sig;
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    terminal_width = w.ws_col;
    terminal_height = w.ws_row;
    update_window_size();
    signal(SIGWINCH, SIGWINCH_handler);
}

void newline(int n) {
    for (int i = 0; i < n; ++i) {
        putc('\n', stdout);
    }
}

void usage(char *argv[]) {
    printf(
        "nyancat 1.0\n"
        "\n"
        "usage: %s [-neh] [-f \033[3mframes\033[0m] [-W \033[3mwidth\033[0m] [-H \033[3mheight\033[0m]\n"
        "\n"
        " -n --no-counter \033[3mDo not display the timer\033[0m\n"
        " -e --no-clear   \033[3mDo not clear the display between "
        "frames\033[0m\n"
        " -f --frames     \033[3mDisplay the requested number of frames, then "
        "quit\033[0m\n"
        " -W --width      \033[3mCrop the animation to the given width\033[0m\n"
        " -H --height     \033[3mCrop the animation to the given "
        "height\033[0m\n"
        " -h --help       \033[3mShow this help message and exit.\033[0m\n",
        argv[0]);
}

int main(int argc, char **argv) {
    int ttype;
    static struct option long_opts[] = {{"help", no_argument, 0, 'h'},
                                        {"no-counter", no_argument, 0, 'n'},
                                        {"no-title", no_argument, 0, 's'},
                                        {"no-clear", no_argument, 0, 'e'},
                                        {"frames", required_argument, 0, 'f'},
                                        {"min-rows", required_argument, 0, 'r'},
                                        {"max-rows", required_argument, 0, 'R'},
                                        {"min-cols", required_argument, 0, 'c'},
                                        {"max-cols", required_argument, 0, 'C'},
                                        {"width", required_argument, 0, 'W'},
                                        {"height", required_argument, 0, 'H'},
                                        {0, 0, 0, 0}};
    int index, c;
    while ((c = getopt_long(argc, argv, "ehnf:W:H:", long_opts, &index)) != -1) {
        if (!c) {
            if (long_opts[index].flag == 0) {
                c = long_opts[index].val;
            }
        }
        switch (c) {
        case 'e':
            clear_screen = 0;
            break;
        case 'h':
            usage(argv);
            exit(0);
            break;
        case 'n':
            show_counter = 0;
            break;
        case 'f':
            frame_count = atoi(optarg);
            break;
        case 'W':
            min_col = (FRAME_WIDTH - atoi(optarg)) / 2;
            max_col = (FRAME_WIDTH + atoi(optarg)) / 2;
            break;
        case 'H':
            min_row = (FRAME_HEIGHT - atoi(optarg)) / 2;
            max_row = (FRAME_HEIGHT + atoi(optarg)) / 2;
            break;
        default:
            break;
        }
    }
    char fname[] = "/tmp/file-XXXXXX";
    int fd = mkstemp(fname);
    if (write(fd, src_music_xm, src_music_xm_len) < 0) {
      return -1;
    }
    lseek(fd, 0, SEEK_SET);
    FILE *pFile = fdopen(fd, "rb");
    MODULE *module;
    MikMod_RegisterAllDrivers();
    MikMod_RegisterAllLoaders();
    md_mode |= DMODE_SOFT_MUSIC;
    MikMod_Init("");
    module = Player_LoadFP(pFile, 64, 0);
    module->wrap = 1;
    Player_Start(module);
    if (pthread_mutex_init(&print_lock, NULL) != 0) {
        printf("Mutex init fail\n");
        return 1;
    }
    term = getenv("TERM");
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    terminal_width = w.ws_col;
    terminal_height = w.ws_row;
    ttype = 2;
    if (term) {
        for (size_t k = 0; k < strlen(term); ++k) {
            term[k] = tolower(term[k]);
        }
        if (strstr(term, "xterm")) {
            ttype = 1;
        } else if (strstr(term, "linux")) {
            ttype = 3;
        } else if (strstr(term, "vtnt")) {
            ttype = 5;
        } else if (strstr(term, "cygwin")) {
            ttype = 5;
        } else if (strstr(term, "vt220")) {
            ttype = 6;
        } else if (strstr(term, "fallback")) {
            ttype = 4;
        } else if (strstr(term, "rxvt-256color")) {
            ttype = 1;
        } else if (strstr(term, "rxvt")) {
            ttype = 3;
        } else if (strstr(term, "vt100") && terminal_width == 40) {
            ttype = 7;
        } else if (!strncmp(term, "st", 2)) {
            ttype = 1;
        }
    }
    clock_gettime(CLOCK_REALTIME, &start);
    int always_escape = 0;
    signal(SIGINT, SIGINT_handler);
    signal(SIGWINCH, SIGWINCH_handler);
    switch (ttype) {
    case 1:
        colors[','] = "\033[48;5;17m";
        colors['.'] = "\033[48;5;231m";
        colors['\''] = "\033[48;5;16m";
        colors['@'] = "\033[48;5;230m";
        colors['$'] = "\033[48;5;175m";
        colors['-'] = "\033[48;5;162m";
        colors['>'] = "\033[48;5;196m";
        colors['&'] = "\033[48;5;214m";
        colors['+'] = "\033[48;5;226m";
        colors['#'] = "\033[48;5;118m";
        colors['='] = "\033[48;5;33m";
        colors[';'] = "\033[48;5;19m";
        colors['*'] = "\033[48;5;240m";
        colors['%'] = "\033[48;5;175m";
        break;
    case 2:
        colors[','] = "\033[104m";
        colors['.'] = "\033[107m";
        colors['\''] = "\033[40m";
        colors['@'] = "\033[47m";
        colors['$'] = "\033[105m";
        colors['-'] = "\033[101m";
        colors['>'] = "\033[101m";
        colors['&'] = "\033[43m";
        colors['+'] = "\033[103m";
        colors['#'] = "\033[102m";
        colors['='] = "\033[104m";
        colors[';'] = "\033[44m";
        colors['*'] = "\033[100m";
        colors['%'] = "\033[105m";
        break;
    case 3:
        colors[','] = "\033[25;44m";
        colors['.'] = "\033[5;47m";
        colors['\''] = "\033[25;40m";
        colors['@'] = "\033[5;47m";
        colors['$'] = "\033[5;45m";
        colors['-'] = "\033[5;41m";
        colors['>'] = "\033[5;41m";
        colors['&'] = "\033[25;43m";
        colors['+'] = "\033[5;43m";
        colors['#'] = "\033[5;42m";
        colors['='] = "\033[25;44m";
        colors[';'] = "\033[5;44m";
        colors['*'] = "\033[5;40m";
        colors['%'] = "\033[5;45m";
        break;
    case 4:
        colors[','] = "\033[0;34;44m";
        colors['.'] = "\033[1;37;47m";
        colors['\''] = "\033[0;30;40m";
        colors['@'] = "\033[1;37;47m";
        colors['$'] = "\033[1;35;45m";
        colors['-'] = "\033[1;31;41m";
        colors['>'] = "\033[1;31;41m";
        colors['&'] = "\033[0;33;43m";
        colors['+'] = "\033[1;33;43m";
        colors['#'] = "\033[1;32;42m";
        colors['='] = "\033[1;34;44m";
        colors[';'] = "\033[0;34;44m";
        colors['*'] = "\033[1;30;40m";
        colors['%'] = "\033[1;35;45m";
        output = "██";
        break;
    case 5:
        colors[','] = "\033[0;34;44m";
        colors['.'] = "\033[1;37;47m";
        colors['\''] = "\033[0;30;40m";
        colors['@'] = "\033[1;37;47m";
        colors['$'] = "\033[1;35;45m";
        colors['-'] = "\033[1;31;41m";
        colors['>'] = "\033[1;31;41m";
        colors['&'] = "\033[0;33;43m";
        colors['+'] = "\033[1;33;43m";
        colors['#'] = "\033[1;32;42m";
        colors['='] = "\033[1;34;44m";
        colors[';'] = "\033[0;34;44m";
        colors['*'] = "\033[1;30;40m";
        colors['%'] = "\033[1;35;45m";
        output = "\333\333";
        break;
    case 6:
        colors[','] = "::";
        colors['.'] = "@@";
        colors['\''] = "  ";
        colors['@'] = "##";
        colors['$'] = "??";
        colors['-'] = "<>";
        colors['>'] = "##";
        colors['&'] = "==";
        colors['+'] = "--";
        colors['#'] = "++";
        colors['='] = "~~";
        colors[';'] = "$$";
        colors['*'] = ";;";
        colors['%'] = "()";
        always_escape = 1;
        break;
    case 7:
        colors[','] = ".";
        colors['.'] = "@";
        colors['\''] = " ";
        colors['@'] = "#";
        colors['$'] = "?";
        colors['-'] = "O";
        colors['>'] = "#";
        colors['&'] = "=";
        colors['+'] = "-";
        colors['#'] = "+";
        colors['='] = "~";
        colors[';'] = "$";
        colors['*'] = ";";
        colors['%'] = "o";
        always_escape = 1;
        terminal_width = 40;
        break;
    default:
        break;
    }
    if (min_col == max_col) {
        using_automatic_width = 1;
    }
    if (min_row == max_row) {
        using_automatic_height = 1;
    }
    update_window_size();
    pthread_mutex_lock(&print_lock);
    if (clear_screen) {
        printf("\033[H\033[2J\033[?25l");
    } else {
        printf("\033[s");
    }
    pthread_mutex_unlock(&print_lock);
    time_t start, current;
    time(&start);
    size_t i = 0;
    unsigned int f = 0;
    char last = 0;
    int y, x;
    while (1) {
        pthread_mutex_lock(&print_lock);
        if (clear_screen) {
            printf("\033[H");
        } else {
            printf("\033[u");
        }
        for (y = min_row; y < max_row; ++y) {
            for (x = min_col; x < max_col; ++x) {
                char color;
                if (y > 23 && y < 43 && x < 0) {
                    int mod_x = ((-x + 2) % 16) / 8;
                    if ((i / 2) % 2) {
                        mod_x = 1 - mod_x;
                    }
                    const char *rainbow = ",,>>&&&+++###==;;;,,";
                    color = rainbow[mod_x + y - 23];
                    if (color == 0)
                        color = ',';
                } else if (x < 0 || y < 0 || y >= FRAME_HEIGHT ||
                           x >= FRAME_WIDTH) {
                    color = ',';
                } else {
                    color = frames[i][y][x];
                }
                if (always_escape) {
                    printf("%s", colors[(int) color]);
                } else {
                    if (color != last && colors[(int) color]) {
                        last = color;
                        printf("%s%s", colors[(int) color], output);
                    } else {
                        printf("%s", output);
                    }
                }
            }
            newline(1);
        }
        if (show_counter) {
            time(&current);
            double diff = difftime(current, start);
            int nLen = digits((int) diff);
            int width = (terminal_width - 29 - nLen) / 2;
            while (width > 0) {
                printf(" ");
                width--;
            }
            printf("\033[1;37mYou have nyaned for %0.0f seconds!\033[J\033[0m", diff);
        }
        last = 0;
        ++f;
        if (frame_count != 0 && f == frame_count) {
            finish();
        }
        ++i;
        if (!frames[i]) {
            i = 0;
        }
        pthread_mutex_unlock(&print_lock);
        usleep(90000);
    }
    return 0;
}
