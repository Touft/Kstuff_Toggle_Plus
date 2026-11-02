#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/time.h>
#include <ps5/kernel.h>

#define PAYLOAD_NAME "Kstuff_Toggle_Plus.elf"
#define THREAD_NAME  "KTP"          /* Nom de thread court (kernel limite ~15 chars) */
#define APP_VERSION  "1.0"
#define CONFIG_PATH  "/data/kstuff_toggle_plus/autotoggle.ini"

typedef struct notify_request {
    char unused[45];
    char message[3075];
} notify_request_t;

typedef struct app_info {
    uint32_t app_id;
    uint64_t unknown1;
    char title_id[14];
    char unknown2[0x3c];
} app_info_t;

extern int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);
extern int sceKernelGetAppInfo(pid_t pid, app_info_t *info);
extern uint32_t kernel_get_fw_version(void);

/* Default timers */
static int g_ps5_disable_timer = 10;
static int g_ps5_enable_timer  = 3;
static int g_ps4_disable_timer = 10;
static int g_ps4_enable_timer  = 3;

static void notify(const char* fmt, ...) {
    notify_request_t req = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(req.message, sizeof(req.message) - 1, fmt, args);
    va_end(args);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

/* Read an integer from the config file, return default value if not found */
static int config_get_int(const char *key, int default_value) {
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) {
        printf("[INFO] Config file %s not found, using default values\n", CONFIG_PATH);
        return default_value;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#' || line[0] == ';') {
            continue;
        }

        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *name = line;
        char *value = eq + 1;

        while (*name == ' ' || *name == '\t') name++;
        char *end = name + strlen(name) - 1;
        while (end > name && (*end == ' ' || *end == '\t')) { *end = '\0'; end--; }

        if (strcmp(name, key) == 0) {
            while (*value == ' ' || *value == '\t') value++;
            int val = atoi(value);
            fclose(f);
            printf("[INFO] Config %s = %d\n", key, val);
            return val;
        }
    }

    fclose(f);
    return default_value;
}

/* Load configuration at startup */
static void load_config(void) {
    g_ps5_disable_timer = config_get_int("PS5_auto_disable_timer", 10);
    g_ps5_enable_timer  = config_get_int("PS5_auto_enable_timer", 3);
    g_ps4_disable_timer = config_get_int("PS4_auto_disable_timer", 10);
    g_ps4_enable_timer  = config_get_int("PS4_auto_enable_timer", 3);
    printf("[INFO] Timers PS5: disable=%ds enable=%ds\n", g_ps5_disable_timer, g_ps5_enable_timer);
    printf("[INFO] Timers PS4: disable=%ds enable=%ds\n", g_ps4_disable_timer, g_ps4_enable_timer);
}

/* =========================================================================
 * Integrated kstuff-toggle (option 3 = PS5 + PS4 sysentvec)
 * ========================================================================= */
static int kstuff_toggle(int option, int enable) {
    uintptr_t sysentvec = 0;
    uintptr_t sysentvec_ps4 = 0;

    uint32_t fw = kernel_get_fw_version();

    switch(fw & 0xffff0000) {
    case 0x1000000:
    case 0x1010000:
    case 0x1020000:
    case 0x1050000:
    case 0x1100000:
    case 0x1110000:
    case 0x1120000:
    case 0x1130000:
    case 0x1140000:
    case 0x2000000:
    case 0x2200000:
    case 0x2250000:
    case 0x2260000:
    case 0x2300000:
    case 0x2500000:
    case 0x2700000:
        /* probably running byepervisor instead of kstuff */
        notify("kstuff: byepervisor detected, skipping");
        return 0;

    case 0x3000000:
    case 0x3100000:
    case 0x3200000:
    case 0x3210000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xca0cd8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xca0e50;
        break;

    case 0x4000000:
    case 0x4020000:
    case 0x4030000:
    case 0x4500000:
    case 0x4510000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xd11bb8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xd11d30;
        break;

    case 0x5000000:
    case 0x5020000:
    case 0x5100000:
    case 0x5500000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xe00be8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xe00d60;
        break;

    case 0x6000000:
    case 0x6020000:
    case 0x6500000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xe210a8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xe21220;
        break;

    case 0x7000000:
    case 0x7010000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xe21ab8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xe21c30;
        break;
    case 0x7200000:
    case 0x7400000:
    case 0x7600000:
    case 0x7610000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xe21b78;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xe21cf0;
        break;

    case 0x8000000:
    case 0x8200000:
    case 0x8400000:
    case 0x8600000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xe21ca8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xe21e20;
        break;

    case 0x9000000:
    case 0x9050000:
    case 0x9200000:
    case 0x9400000:
    case 0x9600000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xdba648;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xdba7c0;
        break;

    case 0x10000000:
    case 0x10010000:
    case 0x10200000:
    case 0x10400000:
    case 0x10600000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xdba6d8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xdba850;
        break;

    case 0x11000000:
    case 0x11200000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xdcbc78;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xdcbdf0;
        break;

    case 0x11400000:
    case 0x11600000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xdcbc98;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xdcbe10;
        break;

    case 0x12000000:
    case 0x12020000:
    case 0x12100000:
    case 0x12200000:
    case 0x12400000:
    case 0x12600000:
    case 0x12700000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xdcc978;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xdccaf0;
        break;

    case 0x13000000:
    case 0x13200000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xddd8b8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xddda30;
        break;

    case 0x13400000:
    case 0x13420000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0xddd8f8;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0xddda70;
        break;

    /* PS5 Pro 13.60 offset dumped from my console */
    case 0x13600000:
        sysentvec     = KERNEL_ADDRESS_DATA_BASE + 0x0e6e5c0;
        sysentvec_ps4 = KERNEL_ADDRESS_DATA_BASE + 0x0e6e5c0;
        break;

    default:
        notify("kstuff: unsupported firmware");
        return -1;
    }

    uint16_t val = enable ? 0xdeb7 : 0xffff;
    uint16_t cur_ps5 = 0, cur_ps4 = 0;

    /* Read current values for debug */
    cur_ps5 = kernel_getshort(sysentvec + 14);
    cur_ps4 = kernel_getshort(sysentvec_ps4 + 14);
    printf("[DEBUG] kstuff fw=0x%x sysentvec=0x%lx (+14=0x%hx) sysentvec_ps4=0x%lx (+14=0x%hx)\n",
           fw, (unsigned long)sysentvec, cur_ps5, (unsigned long)sysentvec_ps4, cur_ps4);

    if (option == 1) {
        /* PS5 only */
        kernel_setshort(sysentvec + 14, val);
        notify("kstuff PS5 %s (was 0x%hx)", enable ? "enabled" : "disabled", cur_ps5);
    } else if (option == 2) {
        /* PS4 only */
        kernel_setshort(sysentvec_ps4 + 14, val);
        notify("kstuff PS4 %s (was 0x%hx)", enable ? "enabled" : "disabled", cur_ps4);
    } else if (option == 3) {
        /* Both at the same time */
        kernel_setshort(sysentvec + 14, val);
        kernel_setshort(sysentvec_ps4 + 14, val);
        notify("kstuff Both %s (PS5 was 0x%hx, PS4 was 0x%hx)",
               enable ? "enabled" : "disabled", cur_ps5, cur_ps4);
    } else {
        notify("kstuff: invalid option");
        return -1;
    }

    return 0;
}

/* Launch notification: "(CONSOLE) Game detected (ID) pausing Kstuff in (N)s" */
static void notify_launch(const char *console, const char *title_id, int seconds) {
    char msg[256];
    snprintf(msg, sizeof(msg), "%s Game detected %s pausing Kstuff in %ds", console, title_id, seconds);
    notify("%s", msg);
    printf("[INFO] %s\n", msg);
    sleep(seconds);

    /* Disable kstuff (PS5 + PS4) */
    if (kstuff_toggle(3, 0) != 0) {
        notify("kstuff toggle failed");
    }
}

/* Exit notification: "(ID) closed, resuming Kstuff in (N)s" */
static void notify_exit(const char *title_id, int seconds) {
    char msg[256];
    snprintf(msg, sizeof(msg), "%s closed, resuming Kstuff in %ds", title_id, seconds);
    notify("%s", msg);
    printf("[INFO] %s\n", msg);
    sleep(seconds);

    /* Enable kstuff (PS5 + PS4) */
    if (kstuff_toggle(3, 1) != 0) {
        notify("kstuff toggle failed");
    }
}

/* Find PID of a process by name (same principle as autokstuff) */
static pid_t find_pid(const char *name) {
    int mib[4] = {1, 14, 8, 0};
    pid_t mypid = getpid();
    pid_t pid = -1;
    size_t buf_size;
    uint8_t *buf;

    if (sysctl(mib, 4, 0, &buf_size, 0, 0)) {
        return -1;
    }

    if (!(buf = malloc(buf_size))) {
        return -1;
    }

    if (sysctl(mib, 4, buf, &buf_size, 0, 0)) {
        free(buf);
        return -1;
    }

    for (uint8_t *ptr = buf; ptr < (buf + buf_size);) {
        int ki_structsize = *(int *)ptr;
        pid_t ki_pid = *(pid_t *)&ptr[72];
        char *ki_tdname = (char *)&ptr[447];

        ptr += ki_structsize;
        if (!strcmp(name, ki_tdname) && ki_pid != mypid) {
            pid = ki_pid;
        }
    }

    free(buf);
    return pid;
}

/* Monitor game process exit and show countdown notification */
static void monitor_game(pid_t pid, const char *title_id, const char *type, int is_ps5) {
    int kq = kqueue();
    if (kq == -1) {
        perror("kqueue");
        return;
    }

    struct kevent kev;
    EV_SET(&kev, pid, EVFILT_PROC, EV_ADD | EV_ENABLE | EV_CLEAR, NOTE_EXIT, 0, NULL);

    if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1) {
        printf("[WARNING] kevent registration failed for pid %d: %s\n", pid, strerror(errno));
        close(kq);
        return;
    }

    printf("[INFO] Monitoring pid %d (%s %s) for exit...\n", pid, type, title_id);

    while (1) {
        struct kevent event;
        struct timespec tout = {1, 0};
        int nev = kevent(kq, NULL, 0, &event, 1, &tout);

        if (nev < 0) {
            printf("[WARNING] kevent wait failed: %s\n", strerror(errno));
            break;
        }

        if (nev > 0 && event.fflags & NOTE_EXIT) {
            printf("[INFO] Process %d exited\n", pid);
            break;
        }
    }

    close(kq);

    int enable_timer = is_ps5 ? g_ps5_enable_timer : g_ps4_enable_timer;
    notify_exit(title_id, enable_timer);
}

int main() {
    syscall(SYS_thr_set_name, -1, THREAD_NAME);

    /* Load configuration */
    load_config();

    /* Kill any previous instance of this payload */
    int pid;
    while ((pid = find_pid(THREAD_NAME)) > 0) {
        if (kill(pid, SIGKILL)) {
            return -1;
        }
        printf("[INFO] Killed old instance\n");
        sleep(1);
    }

    /* Find SceSysCore.elf to intercept process launches */
    pid_t syscore_pid = find_pid("SceSysCore.elf");
    if (syscore_pid == -1) {
        printf("[WARNING] Failed to find SceSysCore.elf pid\n");
        return -1;
    }

    int kq = kqueue();
    if (kq == -1) {
        perror("kqueue");
        return -1;
    }

    struct kevent kev;
    EV_SET(&kev, syscore_pid, EVFILT_PROC, EV_ADD | EV_ENABLE | EV_CLEAR,
           NOTE_FORK | NOTE_EXEC | NOTE_TRACK, 0, NULL);

    if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1) {
        perror("kevent");
        close(kq);
        return -1;
    }

    notify("Kstuff Toggle Plus V" APP_VERSION);
    printf("[INFO] Monitoring SceSysCore.elf (pid %d) for game launches...\n", syscore_pid);

    pid_t child_pid = -1;

    while (1) {
        struct kevent event;
        int nev = kevent(kq, NULL, 0, &event, 1, NULL);

        if (nev < 0) {
            perror("kevent");
            continue;
        }

        if (nev == 0) continue;

        if (event.fflags & NOTE_CHILD) {
            child_pid = event.ident;
        }

        if (event.fflags & NOTE_EXEC && child_pid != -1 && event.ident == child_pid) {
            app_info_t appinfo = {0};
            if (sceKernelGetAppInfo(child_pid, &appinfo) != 0) {
                child_pid = -1;
                continue;
            }

        char title_id[10] = {0};
        memcpy(title_id, appinfo.title_id, 9);

        /* Detect PS5 (PPSA) or PS4 (CUSA) */
        int is_ps5 = strncmp(title_id, "PPSA", 4) == 0 ? 1 : 0;
        int is_ps4 = strncmp(title_id, "CUSA", 4) == 0 ? 1 : 0;
        if (!is_ps5 && !is_ps4) {
            child_pid = -1;
            continue;
        }

        const char *type = is_ps5 ? "PS5 (PPSA)" : "PS4 (CUSA)";
        int disable_timer = is_ps5 ? g_ps5_disable_timer : g_ps4_disable_timer;
        notify_launch(type, title_id, disable_timer);

        /* Wait for game to exit before returning to waiting state */
        monitor_game(child_pid, title_id, type, is_ps5);

        child_pid = -1;
        }
    }

    close(kq);
    return 0;
}
