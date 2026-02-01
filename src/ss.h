/*
 * ss - Socket Statistics for Apple platforms (macOS/iOS)
 * A Linux ss command clone for Darwin/XNU systems
 */

#ifndef SS_H
#define SS_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* Version */
#define SS_VERSION "1.0.0"

/* Maximum buffer sizes */
#define MAX_ADDR_LEN 128
#define MAX_STATE_LEN 32
#define MAX_PROC_NAME 256
#define MAX_PATH_LEN 1024

/* Socket family types */
typedef enum {
    SS_FAMILY_INET,    /* IPv4 */
    SS_FAMILY_INET6,   /* IPv6 */
    SS_FAMILY_UNIX,    /* UNIX domain */
    SS_FAMILY_UNKNOWN
} ss_family_t;

/* Socket protocol types */
typedef enum {
    SS_PROTO_TCP,
    SS_PROTO_UDP,
    SS_PROTO_UNIX_STREAM,
    SS_PROTO_UNIX_DGRAM,
    SS_PROTO_UNKNOWN
} ss_proto_t;

/* TCP connection states */
typedef enum {
    SS_TCP_UNKNOWN = 0,
    SS_TCP_CLOSED,
    SS_TCP_LISTEN,
    SS_TCP_SYN_SENT,
    SS_TCP_SYN_RECV,
    SS_TCP_ESTABLISHED,
    SS_TCP_CLOSE_WAIT,
    SS_TCP_FIN_WAIT1,
    SS_TCP_CLOSING,
    SS_TCP_LAST_ACK,
    SS_TCP_FIN_WAIT2,
    SS_TCP_TIME_WAIT
} ss_tcp_state_t;

/* Socket information structure */
typedef struct ss_sock_info {
    ss_family_t family;
    ss_proto_t protocol;
    ss_tcp_state_t state;
    
    /* Addresses */
    char local_addr[MAX_ADDR_LEN];
    uint16_t local_port;
    char remote_addr[MAX_ADDR_LEN];
    uint16_t remote_port;
    
    /* UNIX socket path */
    char unix_path[MAX_PATH_LEN];
    
    /* Queue sizes */
    uint32_t recv_queue;
    uint32_t send_queue;
    
    /* Process info (if available) */
    pid_t pid;
    int fd;                       /* File descriptor number */
    char proc_name[MAX_PROC_NAME];
    uid_t uid;
    
    /* Extended info */
    uint32_t inode;
    
    /* Linked list */
    struct ss_sock_info *next;
} ss_sock_info_t;

/* Command line options */
typedef struct {
    bool show_tcp;          /* -t: show TCP sockets */
    bool show_udp;          /* -u: show UDP sockets */
    bool show_unix;         /* -x: show UNIX sockets */
    bool show_listening;    /* -l: show listening sockets only */
    bool show_all;          /* -a: show all sockets */
    bool numeric;           /* -n: don't resolve names */
    bool show_process;      /* -p: show process info */
    bool extended;          /* -e: show extended info */
    bool summary;           /* -s: show summary statistics */
    bool ipv4_only;         /* -4: IPv4 only */
    bool ipv6_only;         /* -6: IPv6 only */
    bool version;           /* -V: show version */
    bool help;              /* -h: show help */
} ss_options_t;

/* Statistics summary */
typedef struct {
    uint32_t tcp_total;
    uint32_t tcp_established;
    uint32_t tcp_syn_sent;
    uint32_t tcp_syn_recv;
    uint32_t tcp_fin_wait1;
    uint32_t tcp_fin_wait2;
    uint32_t tcp_time_wait;
    uint32_t tcp_close_wait;
    uint32_t tcp_last_ack;
    uint32_t tcp_listen;
    uint32_t tcp_closing;
    uint32_t tcp_closed;
    
    uint32_t udp_total;
    uint32_t unix_stream_total;
    uint32_t unix_dgram_total;
} ss_stats_t;

/* Function declarations - Socket collection */
ss_sock_info_t *collect_all_sockets(const ss_options_t *opts);
void free_socket_list(ss_sock_info_t *list);

/* Function declarations - Output */
void print_header(const ss_options_t *opts);
void print_socket(const ss_sock_info_t *sock, const ss_options_t *opts);
void print_summary(const ss_stats_t *stats);
void print_help(const char *prog_name);
void print_version(void);

/* Utility functions */
const char *tcp_state_to_string(ss_tcp_state_t state);

#endif /* SS_H */
