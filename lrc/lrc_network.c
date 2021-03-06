#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../log.h"
#include "../lre.h"
#include "../utils.h"

#define INVAILD_PORT 	(-1)

struct net_state {
    uint32_t localaddr;
    uint32_t remoteaddr;
    int localport;
    int remoteport;
    int state;
};

static struct net_state *tcp_stats = NULL;
int tcp_statcnt = 0;
static struct net_state *udp_stats = NULL;
int udp_statcnt = 0;
static int net_initialized = 0;

static inline int vaild_port(int port)
{
    return (port > 0 && port <= 65535);
}

static int tcp_state_load(void)
{
    int num;
    int entries = 0;
    int cnt = 0;
    FILE *fp;
    char buf[512];
    unsigned char local_addr[33], rem_addr[33];
    unsigned int local_port, rem_port, state;

    fp = fopen("/proc/net/tcp", "r");
    if (!fp) {
        loge("Failed to open /proc/net/tcp");
        return -EINVAL;
    }

    while (xgetline(buf, 511, fp)) {
        int len = strlen(buf);
        buf[len] = '\0';
        entries++;
    }

    tcp_stats = xzalloc(sizeof(struct net_state) * entries);

    logv("TCP state: ");
    fseek(fp, 0, SEEK_SET);

    /* discard the first line */
    xgetline(buf, 511, fp);
    while (xgetline(buf, 511, fp)) {
        int len = strlen(buf);
        buf[len] = '\0';

        if (cnt >= entries) {
            loge("Tried to process more entries than counted");
            break;
        }
        num = sscanf(buf,
                     "%*d: %32[0-9A-Fa-f]:%X "
                     "%32[0-9A-Fa-f]:%X %X "
                     "%*X:%*X %*X:%*X "
                     "%*X %*d %*d %*u ",
                     local_addr, &local_port,
                     rem_addr, &rem_port, &state);

        if (num != 5) {
            loge("parse /proc/net/tcp error. buf:%s", buf);
            continue;
        }

        tcp_stats[cnt].localaddr = str2hex(local_addr);
        tcp_stats[cnt].localport = local_port;
        tcp_stats[cnt].remoteaddr = str2hex(rem_addr);
        tcp_stats[cnt].remoteport = rem_port;
        tcp_stats[cnt].state = state;

        logv("local ip:%08x, port:%d, remote ip:%08x, port:%d, state:%d",
             tcp_stats[cnt].localaddr, tcp_stats[cnt].localport,
             tcp_stats[cnt].remoteaddr, tcp_stats[cnt].remoteport,
             tcp_stats[cnt].state);

        cnt++;
    }

    tcp_statcnt = cnt;
    return 0;
}

static int udp_state_load(void)
{
    int num;
    int entries = 0;
    int cnt = 0;
    FILE *fp;
    char buf[512];
    unsigned char local_addr[33], rem_addr[33];
    unsigned int local_port, rem_port, state;

    fp = fopen("/proc/net/udp", "r");
    if (!fp) {
        loge("Failed to open /proc/net/udp");
        return -EINVAL;
    }

    while (xgetline(buf, 511, fp)) {
        int len = strlen(buf);
        buf[len] = '\0';
        entries++;
    }

    udp_stats = xzalloc(sizeof(struct net_state) * entries);

    logv("UDP state: ");
    fseek(fp, 0, SEEK_SET);

    /* discard the first line */
    xgetline(buf, 511, fp);
    while (xgetline(buf, 511, fp)) {
        int len = strlen(buf);
        buf[len] = '\0';

        if (cnt >= entries) {
            loge("Tried to process more entries than counted");
            break;
        }
        num = sscanf(buf,
                     "%*d: %32[0-9A-Fa-f]:%X "
                     "%32[0-9A-Fa-f]:%X %X "
                     "%*X:%*X %*X:%*X "
                     "%*X %*d %*d %*u ",
                     local_addr, &local_port,
                     rem_addr, &rem_port, &state);

        if (num != 5) {
            loge("parse /proc/net/tcp error. buf:%s", buf);
            continue;
        }

        udp_stats[cnt].localaddr = str2hex(local_addr);
        udp_stats[cnt].localport = local_port;
        udp_stats[cnt].remoteaddr = str2hex(rem_addr);
        udp_stats[cnt].remoteport = rem_port;
        udp_stats[cnt].state = state;

        logv("local ip:%08x, port:%d, remote ip:%08x, port:%d, state:%d",
             udp_stats[cnt].localaddr, udp_stats[cnt].localport,
             udp_stats[cnt].remoteaddr, udp_stats[cnt].remoteport,
             udp_stats[cnt].state);

        cnt++;
    }

    udp_statcnt = cnt;
    return 0;
}

static int network_info_load(void)
{
    tcp_state_load();
    udp_state_load();
    net_initialized = 1;
    return 0;
}

static void network_info_release(void)
{
    net_initialized = 0;
    free(tcp_stats);
    free(udp_stats);
}


struct lrc_network {
    struct lrc_object base;
    char protocol[16];
    int port;
};

static int network_execute(lrc_obj_t *handle)
{
    struct lrc_network *network;

    network = (struct lrc_network *)handle;

    if (net_initialized == 0) {
        network_info_load();
    }

    if (strlen(network->protocol) == 0) {
        sprintf(network->protocol, "tcp");
    }

    {
        int len;
        char buf[DETAILS_UNIT_MAX] = {0};
        len = xsnprintf(buf, DETAILS_UNIT_MAX, "network ");
        len += xsnprintf(buf + len, DETAILS_UNIT_MAX - len, "protocol:%s ",
                         network->protocol);
        if (network->port != INVAILD_PORT)
            len += xsnprintf(buf + len, DETAILS_UNIT_MAX - len, "port:%d", network->port);
        network->base.output(handle, buf);
    }
    return 0;
}

static lrc_obj_t *network_constructor(void)
{
    struct lrc_network *network;

    network = malloc(sizeof(*network));
    if (!network) {
        return (lrc_obj_t *)0;
    }

    memset(network, 0, sizeof(*network));
    network->port = INVAILD_PORT;
    return (lrc_obj_t *)network;
}

static void network_destructor(lrc_obj_t *handle)
{
    struct lrc_network *network;

    network = (struct lrc_network *)handle;

    free(network);
}

static int arg_protocol_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
    const char *str;
    struct lrc_network *network;

    network = (struct lrc_network *)handle;
    if (!lreval || !lre_value_is_string(lreval)) {
        loge("lrc 'network' err: protocol must be string");
        return LRE_RET_ERROR;
    }
    str = lre_value_get_string(lreval);
    if (!str)
        return LRE_RET_ERROR;

    strncpy(network->protocol, str, 16);
    logd("lrc 'network' arg: protocol:%s", network->protocol);

    return LRE_RET_OK;
}


static int arg_port_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
    struct lrc_network *network;

    network = (struct lrc_network *)handle;

    if (!lreval || !lre_value_is_int(lreval)) {
        loge("lrc 'network': port arg err, range: 0~65535");
        return LRE_RET_ERROR;
    }

    network->port = lre_value_get_int(lreval);
    logd("lrc 'network' arg: port:%d", network->port);
    return LRE_RET_OK;
}

static int expr_listen_handler(lrc_obj_t *handle, int opt,
                               struct lre_value *lreval)
{
    int i;
    struct lrc_network *network;
    struct net_state *netstats;
    int statcnt;
    uint32_t listen_flags = 0;
    int val;
    int exist = 0;
    int ret;

    network = (struct lrc_network *)handle;

    if (!lreval || !lre_value_is_int(lreval)) {
        loge("lrc 'network': listen expr err, val must be '1' or '0'");
        return LRE_RET_ERROR;
    }

    if (!strcasecmp(network->protocol, "tcp")) {
        netstats = tcp_stats;
        statcnt = tcp_statcnt;
        listen_flags = 0x0A;
    } else if (!strcasecmp(network->protocol, "udp")) {
        netstats = tcp_stats;
        statcnt = tcp_statcnt;
        listen_flags = 0x07;
    } else {
        loge("Unsupport protocol:%s", network->protocol);
        return LRE_RET_ERROR;
    }

    if (!vaild_port(network->port)) {
        loge("Invaild port: %d", network->port);
        return LRE_RET_ERROR;
    }

    for (i = 0; i < statcnt; i++) {
        if (netstats[i].state == listen_flags &&
            netstats[i].localport == network->port)
            exist = 1;
    }

    val = lre_value_get_int(lreval);
    /*FIXME: verify val first */
    ret = lre_compare_int(val, exist, opt);
    if (vaild_lre_results(ret)) {
        char buf[DETAILS_UNIT_MAX] = {0};
        xsnprintf(buf, DETAILS_UNIT_MAX, "%slistening.", exist ? "" : "not ");
        network->base.output(handle, buf);
    }
    return ret;
}


static struct lrc_stub_arg network_args[] = {
    {
        .keyword  	 = "protocol",
        .description = "Network protocol. Support val: 'tcp', 'udp'(Ignore case), default:tcp. example: protocol=TCP",
        .handler 	 = arg_protocol_handler,
    }, {
        .keyword  	 = "port",
        .description = "Network port. port range: 0~65535. example: port=1234",
        .handler 	 = arg_port_handler,
    }
};

static struct lrc_stub_expr network_exprs[] = {
    {
        .keyword  	 = "listen",
        .description = "Check if the network is listening. example: listen==1, listen==0",
        .handler 	 = expr_listen_handler,
    }
};

static struct lrc_stub_func lrc_funcs[] = {
    {
        .keyword 	 = "network",
        .description = "Check network connect state, listen port, etc.",
        .constructor = network_constructor,
        .exec 		 = network_execute,
        .destructor  = network_destructor,

        .args 	   = network_args,
        .argcount  = ARRAY_SIZE(network_args),
        .exprs     = network_exprs,
        .exprcount = ARRAY_SIZE(network_exprs),
    }
};

static struct lrc_module lrc_network_mod = {
    .name = "lrc_network",
    .funcs = lrc_funcs,
    .funccount = ARRAY_SIZE(lrc_funcs),
};

int lrc_network_init(void)
{
    int ret;

    ret = lrc_module_register(&lrc_network_mod);
    if (ret)
        loge("Failed to register '%s' modules", lrc_network_mod.name);
    return ret;
}

void lrc_network_release(void)
{
    if (net_initialized)
        network_info_release();

    lrc_module_unregister(&lrc_network_mod);
}

