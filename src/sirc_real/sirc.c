/**
 * @file sirc.c
 * @brief IRC module interface
 * @author Shengyu Zhang <lastavengers@outlook.com>
 * @version 1.0
 * @date 2016-03-02
 *
 */

#define __LOG_ON
#define __DBG_ON

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include "sirc/sirc.h"
#include "sirc_parse.h"
#include "sirc_event_hdr.h"
#include "socket.h"

#include "srain.h"
#include "log.h"

struct _SircSession {
    int fd;
    char buf[SIRC_BUF_LEN];
    GMutex mutex;  // Buffer lock

    SircEvents *events; // Event callbacks
    void *ctx;
};

typedef struct {
    SircSession *sirc;
    char *host;
    int port;
} ThreadData;

/* NOTE: The "th_" prefix means that this function invoked in a seprate thread.
 */
static void th_sirc_connect(ThreadData *td);
static void th_sirc_proc(SircSession *sirc);
static int th_sirc_recv(SircSession *sirc);

/* NOTE: The "idle_" prefix means that this function invoked as a idle function
 * in GLib main thread(main loop).
 */
static int idle_sirc_on_connect(SircSession *sirc);
static int idle_sirc_on_disconnect(SircSession *sirc);
static int idle_sirc_recv(SircSession *sirc);

SircSession* sirc_new_session(SircEvents *events){
    SircSession *sirc;

    g_return_val_if_fail(events, NULL);

    sirc = g_malloc0(sizeof(SircSession));

    sirc->fd = -1;
    sirc->events = events;

    g_mutex_init(&sirc->mutex);

    return sirc;
}

void sirc_free_session(SircSession *sirc){
    g_return_if_fail(sirc);

    if (sirc->fd != -1){
        sirc_disconnect(sirc);
    }

    g_free(sirc);
}

int sirc_get_fd(SircSession *sirc){
    g_return_val_if_fail(sirc, -1);

    return sirc->fd;
}

SircEvents *sirc_get_events(SircSession *sirc){
    g_return_val_if_fail(sirc, NULL);

    return sirc->events;
}

void sirc_set_ctx(SircSession *sirc, void *ctx){
    g_return_if_fail(sirc);

    sirc->ctx = ctx;
}

void* sirc_get_ctx(SircSession *sirc){
    g_return_val_if_fail(sirc, NULL);

    return sirc->ctx;
}

void sirc_connect(SircSession *sirc, const char *host, int port){
    g_return_if_fail(sirc);
    g_return_if_fail(host);
    g_return_if_fail(port > 0);

    ThreadData *td = g_malloc0(sizeof(ThreadData)); // Free by th_irc_connect

    td->sirc = sirc;
    td->host = g_strdup(host);
    td->port = port;

    g_thread_new(NULL, (GThreadFunc)th_sirc_connect, td);
}

void sirc_disconnect(SircSession *sirc){
    g_return_if_fail(sirc);

    close(sirc->fd);
    sirc->fd = -1;
}

/******************************************************************************
 * Functions runs in separate thread
 *****************************************************************************/

static void th_sirc_connect(ThreadData *td){
    SircSession *sirc = td->sirc;

    sirc->fd = sck_get_socket(td->host, td->port);

    if (sirc->fd < 0){
        ERR_FR("Failed to connect to %s:%d", td->host, td->port);
        g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                (GSourceFunc)idle_sirc_on_disconnect, sirc, NULL);
        return;
    }

    g_free(td->host);
    g_free(td);

    g_idle_add_full(G_PRIORITY_HIGH_IDLE,
            (GSourceFunc)idle_sirc_on_connect, sirc, NULL);

    th_sirc_proc(sirc);
}

/* This function runs on a separate thread, blocking read a line
 * (terminated with "\r\n") from socket and then pass it to sirc_recv(), which
 * runs on GLib main thread. */
static void th_sirc_proc(SircSession *sirc){
    for (;;){
        if (th_sirc_recv(sirc) == SRN_ERR){
            DBG_FR("SircSession thread exit because of read error");
            return;
        }
        if (sirc->fd == -1) {
            DBG_FR("SircSession thread exit because of fd closed");
            return;
        }
    }
}

static int th_sirc_recv(SircSession *sirc){
    g_mutex_lock(&sirc->mutex);
    int ret = sck_readline(sirc->fd, sirc->buf, sizeof(sirc->buf));

    if (ret != SRN_ERR){
        g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                (GSourceFunc)idle_sirc_recv, sirc, NULL);
        return SRN_OK;
    }

    ERR_FR("Socket error, connection close");
    g_idle_add_full(G_PRIORITY_HIGH_IDLE,
            (GSourceFunc)idle_sirc_on_disconnect, sirc, NULL);
    g_mutex_unlock(&sirc->mutex);

    return ret;
}

/******************************************************************************
 * Idle functions
 *****************************************************************************/

static int idle_sirc_recv(SircSession *sirc){
    SircMessage imsg;

    if (sirc_parse(sirc->buf, &imsg) == SRN_OK){
        sirc_event_hdr(sirc, &imsg);
    }

    g_mutex_unlock(&sirc->mutex);

    return FALSE;
}

static int idle_sirc_on_connect(SircSession *sirc){
    sirc->events->connect(sirc, "CONNECT");
    return FALSE;
}

static int idle_sirc_on_disconnect(SircSession *sirc){
    sirc->events->disconnect(sirc, sirc->ctx);
    return FALSE;
}