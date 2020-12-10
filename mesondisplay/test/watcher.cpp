#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#define DEBUG_INFO(fmt, arg...) printf(fmt "\n", ##arg)

static const char* short_option = "F:";
static const struct option long_option[] = {
    {"file", required_argument, 0, 'F'},
    {0, 0, 0, 0}
};

static void print_usage(const char* name) {
    printf("Usage: %s [-FR]\n"
            "Watch the file.\n"
            "Options:\n"
            "       -F,--file         the file need watcher\n", name);
}


pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_t worker;

void* monitor_routine(void* file) {
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    int inotfd = inotify_init();
    char buf[256] __attribute__ ((aligned(__alignof__(struct inotify_event)))) = {0};
    const struct inotify_event * event;
    if (inotfd == -1) {
        DEBUG_INFO("Inotify error!");
        return NULL;
    }
    int watchfd = -1;
    DEBUG_INFO("Watching the %p\n", file);
    while (true) {
        if (watchfd == -1) {
            watchfd = inotify_add_watch(inotfd, (char*)file, IN_CLOSE_WRITE);
        }
        if (watchfd == -1) {
            DEBUG_INFO("file %s add watch error", (char*)file);
        } else {
            int len = read(inotfd, buf, sizeof(buf));
            if (len == -1) {
                DEBUG_INFO("read error: %s", strerror(errno));
            }
            char* ptr;
            for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
                event = (const struct inotify_event*) ptr;
                DEBUG_INFO("Get event info!");
                if (event->mask & IN_CLOSE_WRITE) {
                    DEBUG_INFO("file %p changed!", file);
                }
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int opt;
    const char* file_name = NULL;
    while ((opt = getopt_long_only(argc, argv, short_option, long_option, NULL)) != -1) {
        switch (opt) {
            case 'F':
                file_name = optarg;
                break;
            default:
                print_usage(argv[0]);
                return -1;
        }
    };
    if (!file_name) {
        print_usage(argv[0]);
        return -1;
    }
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

    pthread_create(&worker, NULL, monitor_routine, (void*)file_name);
    pthread_cond_wait(&cond, &mutex);

    pthread_join(worker, NULL);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return 0;
}

