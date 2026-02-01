/*
 * ss - Socket Statistics for Apple platforms (macOS/iOS)
 * Output formatting and display
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ss.h"

/* Column widths for formatting */
#define COL_NETID   6
#define COL_STATE   12
#define COL_RECVQ   8
#define COL_SENDQ   8
#define COL_LOCAL   40
#define COL_REMOTE  40
#define COL_PROCESS 30

/* Convert TCP state to string */
const char *tcp_state_to_string(ss_tcp_state_t state)
{
    switch (state) {
        case SS_TCP_CLOSED:      return "CLOSED";
        case SS_TCP_LISTEN:      return "LISTEN";
        case SS_TCP_SYN_SENT:    return "SYN-SENT";
        case SS_TCP_SYN_RECV:    return "SYN-RECV";
        case SS_TCP_ESTABLISHED: return "ESTAB";
        case SS_TCP_CLOSE_WAIT:  return "CLOSE-WAIT";
        case SS_TCP_FIN_WAIT1:   return "FIN-WAIT-1";
        case SS_TCP_CLOSING:     return "CLOSING";
        case SS_TCP_LAST_ACK:    return "LAST-ACK";
        case SS_TCP_FIN_WAIT2:   return "FIN-WAIT-2";
        case SS_TCP_TIME_WAIT:   return "TIME-WAIT";
        case SS_TCP_UNKNOWN:
        default:                 return "UNCONN";
    }
}

/* Get protocol name string (Linux ss uses tcp/udp for both IPv4 and IPv6) */
static const char *get_proto_name(const ss_sock_info_t *sock)
{
    switch (sock->protocol) {
        case SS_PROTO_TCP:
            return "tcp";
        case SS_PROTO_UDP:
            return "udp";
        case SS_PROTO_UNIX_STREAM:
            return "u_str";
        case SS_PROTO_UNIX_DGRAM:
            return "u_dgr";
        default:
            return "???";
    }
}

/* Print table header */
void print_header(const ss_options_t *opts)
{
    printf("%-*s ", COL_NETID, "Netid");
    printf("%-*s ", COL_STATE, "State");
    printf("%*s ", COL_RECVQ, "Recv-Q");
    printf("%*s ", COL_SENDQ, "Send-Q");
    printf("%-*s ", COL_LOCAL, "Local Address:Port");
    printf("%-*s", COL_REMOTE, "Peer Address:Port");
    
    if (opts->show_process) {
        printf(" %-*s", COL_PROCESS, "Process");
    }
    
    if (opts->extended) {
        printf(" %-8s", "PID");
    }
    
    printf("\n");
}

/* Print a single socket entry */
void print_socket(const ss_sock_info_t *sock, const ss_options_t *opts)
{
    const char *proto = get_proto_name(sock);
    const char *state = tcp_state_to_string(sock->state);
    
    /* Format local address */
    char local_str[MAX_ADDR_LEN];
    if (sock->protocol == SS_PROTO_UNIX_STREAM || 
        sock->protocol == SS_PROTO_UNIX_DGRAM) {
        if (sock->unix_path[0] != '\0') {
            snprintf(local_str, sizeof(local_str), "%s", sock->unix_path);
        } else {
            snprintf(local_str, sizeof(local_str), "%s", sock->local_addr);
        }
    } else {
        snprintf(local_str, sizeof(local_str), "%s", sock->local_addr);
    }
    
    /* Format remote address */
    char remote_str[MAX_ADDR_LEN];
    snprintf(remote_str, sizeof(remote_str), "%s", sock->remote_addr);
    
    /* Print main columns */
    printf("%-*s ", COL_NETID, proto);
    printf("%-*s ", COL_STATE, state);
    printf("%*u ", COL_RECVQ, sock->recv_queue);
    printf("%*u ", COL_SENDQ, sock->send_queue);
    printf("%-*s ", COL_LOCAL, local_str);
    printf("%-*s", COL_REMOTE, remote_str);
    
    /* Print process info if requested (Linux ss compatible format) */
    if (opts->show_process) {
        if (sock->pid > 0 && sock->proc_name[0] != '\0') {
            /* Linux ss format: users:(("name",pid=123,fd=4)) */
            printf(" users:((\"%s\",pid=%d,fd=%d))", 
                   sock->proc_name, sock->pid, sock->fd);
        } else if (sock->pid > 0) {
            printf(" users:((\"?\",pid=%d,fd=%d))", sock->pid, sock->fd);
        } else {
            printf(" ");
        }
    }
    
    /* Print extended info if requested */
    if (opts->extended) {
        if (sock->pid > 0) {
            printf(" %-8d", sock->pid);
        } else {
            printf(" %-8s", "-");
        }
    }
    
    printf("\n");
}

/* Print summary statistics */
void print_summary(const ss_stats_t *stats)
{
    uint32_t total = stats->tcp_total + stats->udp_total + 
                     stats->unix_stream_total + stats->unix_dgram_total;
    
    printf("Total: %u\n\n", total);
    
    if (stats->tcp_total > 0) {
        printf("TCP:   %u (estab %u, closed %u, timewait %u, listen %u)\n",
               stats->tcp_total,
               stats->tcp_established,
               stats->tcp_closed,
               stats->tcp_time_wait,
               stats->tcp_listen);
        
        if (stats->tcp_syn_sent > 0 || stats->tcp_syn_recv > 0 ||
            stats->tcp_fin_wait1 > 0 || stats->tcp_fin_wait2 > 0 ||
            stats->tcp_close_wait > 0 || stats->tcp_last_ack > 0 ||
            stats->tcp_closing > 0) {
            printf("       syn-sent: %u, syn-recv: %u\n",
                   stats->tcp_syn_sent, stats->tcp_syn_recv);
            printf("       fin-wait1: %u, fin-wait2: %u\n",
                   stats->tcp_fin_wait1, stats->tcp_fin_wait2);
            printf("       close-wait: %u, last-ack: %u, closing: %u\n",
                   stats->tcp_close_wait, stats->tcp_last_ack, stats->tcp_closing);
        }
    }
    
    if (stats->udp_total > 0) {
        printf("UDP:   %u\n", stats->udp_total);
    }
    
    if (stats->unix_stream_total > 0 || stats->unix_dgram_total > 0) {
        printf("UNIX:  %u (stream: %u, dgram: %u)\n",
               stats->unix_stream_total + stats->unix_dgram_total,
               stats->unix_stream_total,
               stats->unix_dgram_total);
    }
}

/* Print help message */
void print_help(const char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\nSocket Statistics for Apple platforms (macOS/iOS)\n");
    printf("A Linux ss command clone for Darwin/XNU systems\n");
    printf("\nOptions:\n");
    printf("  -t, --tcp          Display TCP sockets\n");
    printf("  -u, --udp          Display UDP sockets\n");
    printf("  -x, --unix         Display UNIX domain sockets\n");
    printf("  -l, --listening    Display only listening sockets\n");
    printf("  -a, --all          Display all sockets (including non-established)\n");
    printf("  -n, --numeric      Do not resolve service names\n");
    printf("  -p, --processes    Show process using socket\n");
    printf("  -e, --extended     Show extended socket information\n");
    printf("  -s, --summary      Show socket usage summary\n");
    printf("  -4, --ipv4         Display only IPv4 sockets\n");
    printf("  -6, --ipv6         Display only IPv6 sockets\n");
    printf("  -V, --version      Show version information\n");
    printf("  -h, --help         Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s -tuln           Show TCP/UDP listening sockets (numeric)\n", prog_name);
    printf("  %s -ta             Show all TCP sockets\n", prog_name);
    printf("  %s -s              Show summary statistics\n", prog_name);
    printf("  %s -tlp            Show listening TCP with process info\n", prog_name);
    printf("\nNote: Process information (-p) may require root privileges.\n");
}

/* Print version */
void print_version(void)
{
    printf("ss (Darwin) version %s\n", SS_VERSION);
    printf("Socket Statistics for Apple platforms (macOS/iOS)\n");
    printf("A Linux ss command clone for Darwin/XNU systems\n");
}
