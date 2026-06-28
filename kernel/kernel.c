#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../commands/sys_cmd.h"
#include "../lib/types.h"

/* ── utils ───────────────────────────────────────────── */
static void sleep(uint32_t ms) {
    /* ~1ms per iteration at ~10MHz effective loop on QEMU */
    volatile uint32_t i;
    for (i = 0; i < ms * 5000; i++);
}

static size_t kstrlen(const char *s) {
    size_t n = 0; while (s[n]) n++; return n;
}

static int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

static int kstrncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    return n == (size_t)-1 ? 0 : *a - *b;
}

/* ── boot splash ─────────────────────────────────────── */
/*  ASCII art DEADROOM centered on 80x25 VGA              */
static const char *logo[] = {
    "mmmm   mmmmmm   mm   mmmm   mmmmm   mmmm   mmmm  m    m",
    "#   \"m #        ##   #   \"m #   \"# m\"  \"m m\"  \"m ##  ##",
    "#    # #mmmmm  #  #  #    # #mmmm\" #    # #    # # ## #",
    "#    # #       #mm#  #    # #   \"m #    # #    # # \"\" #",
    "#mmm\"  #mmmmm #    # #mmm\"  #    \"  #mm#   #mm#  #    #",
    NULL
};

static void draw_splash(void) {
    vga_clear();
    vga_set_color(COLOR_LGREEN, COLOR_BLACK);

    int logo_lines = 0;
    while (logo[logo_lines]) logo_lines++;

    int start_y = (VGA_HEIGHT - logo_lines - 2) / 2;

    for (int i = 0; logo[i]; i++) {
        int len = (int)kstrlen(logo[i]);
        int x = (VGA_WIDTH - len) / 2;
        if (x < 0) x = 0;
        vga_puts_at(x, start_y + i, logo[i]);
    }

    vga_set_color(COLOR_DGRAY, COLOR_BLACK);
    const char *sub = "METRO-OS  v0.1  BUILD: D34DR00M-1";
    int sx = (VGA_WIDTH - (int)kstrlen(sub)) / 2;
    vga_puts_at(sx, start_y + logo_lines + 1, sub);

    /* keep the splash visible for 50 seconds */
    vga_set_color(COLOR_LGREEN, COLOR_BLACK);
    sleep(50000);
}

/* ── shell ───────────────────────────────────────────── */
#define CMD_BUF 128

static void shell_prompt(void) {
    vga_set_color(COLOR_LGREEN, COLOR_BLACK);
    vga_puts("METRO");
    vga_set_color(COLOR_WHITE, COLOR_BLACK);
    vga_puts("> ");
}

static void shell_help(void) {
    vga_set_color(COLOR_YELLOW, COLOR_BLACK);
    vga_puts("Available commands:\n");
    vga_puts("  help        - show this help menu\n");
    vga_puts("  mem         - show memory information\n");
    vga_puts("  time        - show current RTC time\n");
    vga_puts("  cpuinfo     - show CPU vendor information\n");
    vga_puts("  shutdown    - halt the system\n");
    vga_puts("  dir         - list available entries\n");
    vga_puts("  cd <dir>    - change directory\n");
    vga_puts("  cat <file>  - show file contents\n");
    vga_puts("  restart     - reboot the system\n");
    vga_puts("  echo <text> - print text\n");
    vga_puts("  touch <file>- create a dummy file\n");
    vga_puts("  mkdir <dir> - create a directory\n");
    vga_puts("  rmdir <dir> - remove a directory\n");
    vga_puts("  rm <file>   - remove a file\n");
    vga_puts("  cmdlocate   - print current directory\n");
    vga_puts("  copy <text> - print the given text\n");
    vga_puts("  cls         - clear the screen\n");
    vga_puts("  ver         - show OS version\n");
    vga_set_color(COLOR_WHITE, COLOR_BLACK);
}

static void run_command(char *buf) {
    /* strip trailing spaces */
    int len = (int)kstrlen(buf);
    while (len > 0 && buf[len-1] == ' ') buf[--len] = 0;
    if (len == 0) return;

    if (kstrcmp(buf, "help") == 0)        shell_help();
    else if (kstrcmp(buf, "cls") == 0)    vga_clear();
    else if (kstrcmp(buf, "mem") == 0)    cmd_mem();
    else if (kstrcmp(buf, "time") == 0)   cmd_time();
    else if (kstrcmp(buf, "cpuinfo") == 0) cmd_cpuinfo();
    else if (kstrcmp(buf, "shutdown") == 0) cmd_shutdown();
    else if (kstrcmp(buf, "dir") == 0) cmd_dir();
    else if (kstrncmp(buf, "cd ", 3) == 0) cmd_cd(buf + 3);
    else if (kstrncmp(buf, "cat ", 4) == 0) cmd_cat(buf + 4);
    else if (kstrncmp(buf, "echo ", 5) == 0) cmd_echo(buf + 5);
    else if (kstrncmp(buf, "touch ", 6) == 0) cmd_touch(buf + 6);
    else if (kstrncmp(buf, "mkdir ", 6) == 0) cmd_mkdir(buf + 6);
    else if (kstrncmp(buf, "rmdir ", 6) == 0) cmd_rmdir(buf + 6);
    else if (kstrncmp(buf, "rm ", 3) == 0) cmd_rm(buf + 3);
    else if (kstrcmp(buf, "cmdlocate") == 0) cmd_locate();
    else if (kstrcmp(buf, "restart") == 0) cmd_restart();
    else if (kstrcmp(buf, "ver") == 0) {
        vga_set_color(COLOR_LCYAN, COLOR_BLACK);
        vga_puts("METRO-OS v0.1 BUILD D34DR00M-1\n");
        vga_set_color(COLOR_WHITE, COLOR_BLACK);
    }
    else if (kstrncmp(buf, "copy ", 5) == 0) cmd_copy(buf + 5);
    else {
        vga_set_color(COLOR_LRED, COLOR_BLACK);
        vga_puts("Unknown command: ");
        vga_puts(buf);
        vga_putchar('\n');
        vga_set_color(COLOR_WHITE, COLOR_BLACK);
    }
}

static void shell_run(void) {
    vga_clear();
    vga_set_color(COLOR_LGREEN, COLOR_BLACK);
    vga_puts("Welcome to METRO-OS  |  type 'help' for commands\n\n");
    vga_set_color(COLOR_WHITE, COLOR_BLACK);

    char buf[CMD_BUF];
    int pos = 0;

    shell_prompt();

    while (1) {
        char c = keyboard_getchar();
        if (!c) continue;

        if (c == '\n') {
            vga_putchar('\n');
            buf[pos] = 0;
            run_command(buf);
            pos = 0;
            shell_prompt();
        } else if (c == '\b') {
            if (pos > 0) { pos--; vga_putchar('\b'); }
        } else if (pos < CMD_BUF - 1) {
            buf[pos++] = c;
            vga_putchar(c);
        }
    }
}

/* ── entry ───────────────────────────────────────────── */
void kernel_main(uint32_t magic, void *mboot) {
    (void)magic; (void)mboot;
    vga_init();
    keyboard_init();
    draw_splash();
    shell_run();
}
