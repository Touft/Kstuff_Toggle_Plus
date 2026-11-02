#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>

#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <ps5/kernel.h>

#define PAYLOAD_NAME "Kstuff_Toggle_Plus-unload.elf"
#define TARGET_THREAD  "KTP"          /* Nom de thread du daemon cible */
#define UNLOAD_THREAD  "KTPu"         /* Nom de thread de cet unloader */
#define APP_VERSION  "1.0"

typedef struct notify_request {
    char unused[45];
    char message[3075];
} notify_request_t;

extern int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);

static void notify(const char* fmt, ...) {
    notify_request_t req = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(req.message, sizeof(req.message) - 1, fmt, args);
    va_end(args);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

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

int main() {
    syscall(SYS_thr_set_name, -1, UNLOAD_THREAD);

    /* Kill any previous instance of this unloader */
    int pid;
    while ((pid = find_pid(UNLOAD_THREAD)) > 0) {
        if (kill(pid, SIGKILL)) {
            return -1;
        }
        printf("[INFO] Killed old unloader instance\n");
        sleep(1);
    }

    notify("Kstuff_Toggle_Plus unload v" APP_VERSION);
    printf("[INFO] Kstuff_Toggle_Plus unload v%s\n", APP_VERSION);

    int found = 0;

    /* Kill all instances of the daemon */
    while ((pid = find_pid(TARGET_THREAD)) > 0) {
        if (kill(pid, SIGKILL) == 0) {
            notify("Kstuff_Toggle_Plus (pid %d) killed", pid);
            printf("[INFO] Killed Kstuff_Toggle_Plus (pid %d)\n", pid);
            found = 1;
        }
        sleep(1);
    }

    /* Verify the daemon is dead */
    sleep(1);
    pid = find_pid(TARGET_THREAD);
    if (pid > 0) {
        notify("WARNING: Kstuff_Toggle_Plus still active (pid %d)", pid);
        printf("[WARNING] Kstuff_Toggle_Plus still active (pid %d)\n", pid);
    } else {
        if (found) {
            notify("Kstuff_Toggle_Plus unloaded successfully");
            printf("[INFO] Kstuff_Toggle_Plus unloaded successfully\n");
        } else {
            notify("Kstuff_Toggle_Plus was not running");
            printf("[INFO] Kstuff_Toggle_Plus was not running\n");
        }
    }

    return 0;
}
