/*
 * ss - Socket Statistics for Apple platforms (macOS/iOS)
 * Socket collection using libproc API
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "libproc_compat.h"
#include "ss.h"

/* Maximum number of PIDs to enumerate */
#define MAX_PIDS 65536

/* Darwin TCP states from tcp_fsm.h */
#define TCPS_CLOSED         0
#define TCPS_LISTEN         1
#define TCPS_SYN_SENT       2
#define TCPS_SYN_RECEIVED   3
#define TCPS_ESTABLISHED    4
#define TCPS_CLOSE_WAIT     5
#define TCPS_FIN_WAIT_1     6
#define TCPS_CLOSING        7
#define TCPS_LAST_ACK       8
#define TCPS_FIN_WAIT_2     9
#define TCPS_TIME_WAIT      10

/* Convert Darwin TCP state to our state enum */
static ss_tcp_state_t darwin_to_ss_state(int state)
{
    switch (state) {
        case TCPS_CLOSED:       return SS_TCP_CLOSED;
        case TCPS_LISTEN:       return SS_TCP_LISTEN;
        case TCPS_SYN_SENT:     return SS_TCP_SYN_SENT;
        case TCPS_SYN_RECEIVED: return SS_TCP_SYN_RECV;
        case TCPS_ESTABLISHED:  return SS_TCP_ESTABLISHED;
        case TCPS_CLOSE_WAIT:   return SS_TCP_CLOSE_WAIT;
        case TCPS_FIN_WAIT_1:   return SS_TCP_FIN_WAIT1;
        case TCPS_CLOSING:      return SS_TCP_CLOSING;
        case TCPS_LAST_ACK:     return SS_TCP_LAST_ACK;
        case TCPS_FIN_WAIT_2:   return SS_TCP_FIN_WAIT2;
        case TCPS_TIME_WAIT:    return SS_TCP_TIME_WAIT;
        default:                return SS_TCP_UNKNOWN;
    }
}

/* Format IPv4 address with port */
static void format_addr_v4(struct in_addr *addr, uint16_t port, char *buf, size_t buflen)
{
    char ip_str[INET_ADDRSTRLEN];
    
    if (addr->s_addr == INADDR_ANY) {
        strcpy(ip_str, "*");
    } else {
        inet_ntop(AF_INET, addr, ip_str, sizeof(ip_str));
    }
    
    if (port == 0) {
        snprintf(buf, buflen, "%s:*", ip_str);
    } else {
        snprintf(buf, buflen, "%s:%u", ip_str, port);
    }
}

/* Format IPv6 address with port */
static void format_addr_v6(struct in6_addr *addr, uint16_t port, char *buf, size_t buflen)
{
    char ip_str[INET6_ADDRSTRLEN];
    
    if (IN6_IS_ADDR_UNSPECIFIED(addr)) {
        strcpy(ip_str, "*");
    } else {
        inet_ntop(AF_INET6, addr, ip_str, sizeof(ip_str));
    }
    
    if (port == 0) {
        snprintf(buf, buflen, "[%s]:*", ip_str);
    } else {
        snprintf(buf, buflen, "[%s]:%u", ip_str, port);
    }
}

/* Add socket to list */
static ss_sock_info_t *add_to_list(ss_sock_info_t *list, ss_sock_info_t *sock)
{
    ss_sock_info_t *new_sock = malloc(sizeof(ss_sock_info_t));
    if (!new_sock) {
        return list;
    }
    
    memcpy(new_sock, sock, sizeof(ss_sock_info_t));
    new_sock->next = list;
    return new_sock;
}

/* Check if socket already exists in list (avoid duplicates) */
static bool socket_exists(ss_sock_info_t *list, ss_sock_info_t *sock)
{
    for (ss_sock_info_t *s = list; s != NULL; s = s->next) {
        if (s->protocol == sock->protocol &&
            s->family == sock->family &&
            s->local_port == sock->local_port &&
            s->remote_port == sock->remote_port &&
            strcmp(s->local_addr, sock->local_addr) == 0 &&
            strcmp(s->remote_addr, sock->remote_addr) == 0) {
            return true;
        }
    }
    return false;
}

/* Check if socket should be included based on options */
static bool should_include(const ss_sock_info_t *sock, const ss_options_t *opts)
{
    /* Filter by protocol */
    if (sock->protocol == SS_PROTO_TCP && !opts->show_tcp) {
        return false;
    }
    if (sock->protocol == SS_PROTO_UDP && !opts->show_udp) {
        return false;
    }
    if ((sock->protocol == SS_PROTO_UNIX_STREAM || 
         sock->protocol == SS_PROTO_UNIX_DGRAM) && !opts->show_unix) {
        return false;
    }
    
    /* Filter by listening state */
    if (opts->show_listening) {
        if (sock->protocol == SS_PROTO_TCP && sock->state != SS_TCP_LISTEN) {
            return false;
        }
        /* For UDP, consider bound sockets as "listening" */
        if (sock->protocol == SS_PROTO_UDP && sock->remote_port != 0) {
            return false;
        }
    }
    
    /* If not showing all, filter to established/listening only */
    if (!opts->show_all && !opts->show_listening && sock->protocol == SS_PROTO_TCP) {
        if (sock->state != SS_TCP_ESTABLISHED && sock->state != SS_TCP_LISTEN) {
            return false;
        }
    }
    
    /* Filter by IP version */
    if (opts->ipv4_only && sock->family != SS_FAMILY_INET) {
        return false;
    }
    if (opts->ipv6_only && sock->family != SS_FAMILY_INET6) {
        return false;
    }
    
    return true;
}

/* Get process name by PID */
static void get_proc_name(pid_t pid, char *name, size_t name_len)
{
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    
    int ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
    if (ret > 0) {
        char *slash = strrchr(pathbuf, '/');
        if (slash) {
            strncpy(name, slash + 1, name_len - 1);
        } else {
            strncpy(name, pathbuf, name_len - 1);
        }
        name[name_len - 1] = '\0';
    } else {
        /* Fallback to proc_name */
        ret = proc_name(pid, name, (uint32_t)name_len);
        if (ret <= 0) {
            strncpy(name, "?", name_len);
        }
    }
}

/* Collect sockets from a single process */
static ss_sock_info_t *collect_process_sockets(pid_t pid, ss_sock_info_t *list, 
                                                const ss_options_t *opts)
{
    struct proc_fdinfo *fdinfo = NULL;
    int fd_bufsize, num_fds;
    char proc_name[MAX_PROC_NAME] = {0};
    bool got_proc_name = false;
    
    /* Get number of file descriptors */
    fd_bufsize = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, NULL, 0);
    if (fd_bufsize <= 0) {
        return list;
    }
    
    fdinfo = malloc(fd_bufsize);
    if (!fdinfo) {
        return list;
    }
    
    num_fds = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fdinfo, fd_bufsize);
    if (num_fds <= 0) {
        free(fdinfo);
        return list;
    }
    
    num_fds /= sizeof(struct proc_fdinfo);
    
    /* Iterate through file descriptors */
    for (int i = 0; i < num_fds; i++) {
        if (fdinfo[i].proc_fdtype != PROX_FDTYPE_SOCKET) {
            continue;
        }
        
        /* Get socket info - try different sizes for iOS compatibility */
        char si_buf[1024];  /* Large enough buffer */
        struct socket_fdinfo *si = (struct socket_fdinfo *)si_buf;
        
        /* Try PROC_PIDFDSOCKETINFO (value 3) */
        int ret = proc_pidfdinfo(pid, fdinfo[i].proc_fd, 3, si_buf, sizeof(si_buf));
        if (ret <= 0) {
            continue;
        }
        
        ss_sock_info_t sock = {0};
        sock.pid = pid;
        
        /* Determine socket type */
        int family = si->psi.soi_family;
        int type = si->psi.soi_type;
        int proto = si->psi.soi_protocol;
        
        if (family == AF_INET || family == AF_INET6) {
            sock.family = (family == AF_INET) ? SS_FAMILY_INET : SS_FAMILY_INET6;
            
            if (proto == IPPROTO_TCP) {
                sock.protocol = SS_PROTO_TCP;
                sock.state = darwin_to_ss_state(si->psi.soi_proto.pri_tcp.tcpsi_state);
            } else if (proto == IPPROTO_UDP) {
                sock.protocol = SS_PROTO_UDP;
                sock.state = SS_TCP_UNKNOWN;
            } else {
                continue;  /* Skip other protocols */
            }
            
            /* Get addresses */
            if (family == AF_INET) {
                struct in_addr laddr, faddr;
                laddr.s_addr = si->psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr4.s_addr;
                faddr.s_addr = si->psi.soi_proto.pri_in.insi_faddr.ina_46.i46a_addr4.s_addr;
                uint16_t lport = ntohs(si->psi.soi_proto.pri_in.insi_lport);
                uint16_t fport = ntohs(si->psi.soi_proto.pri_in.insi_fport);
                
                format_addr_v4(&laddr, lport, sock.local_addr, sizeof(sock.local_addr));
                format_addr_v4(&faddr, fport, sock.remote_addr, sizeof(sock.remote_addr));
                sock.local_port = lport;
                sock.remote_port = fport;
            } else {
#ifdef IOS_BUILD
                struct in6_addr *laddr6 = &si->psi.soi_proto.pri_in.insi_laddr.ina_46.i46a_addr6;
                struct in6_addr *faddr6 = &si->psi.soi_proto.pri_in.insi_faddr.ina_46.i46a_addr6;
#else
                struct in6_addr *laddr6 = &si->psi.soi_proto.pri_in.insi_laddr.ina_6;
                struct in6_addr *faddr6 = &si->psi.soi_proto.pri_in.insi_faddr.ina_6;
#endif
                uint16_t lport = ntohs(si->psi.soi_proto.pri_in.insi_lport);
                uint16_t fport = ntohs(si->psi.soi_proto.pri_in.insi_fport);
                
                format_addr_v6(laddr6, lport, sock.local_addr, sizeof(sock.local_addr));
                format_addr_v6(faddr6, fport, sock.remote_addr, sizeof(sock.remote_addr));
                sock.local_port = lport;
                sock.remote_port = fport;
            }
            
            /* Queue sizes */
            sock.recv_queue = si->psi.soi_rcv.sbi_cc;
            sock.send_queue = si->psi.soi_snd.sbi_cc;
            
        } else if (family == AF_UNIX) {
            sock.family = SS_FAMILY_UNIX;
            sock.protocol = (type == SOCK_STREAM) ? SS_PROTO_UNIX_STREAM : SS_PROTO_UNIX_DGRAM;
            sock.state = SS_TCP_UNKNOWN;
            
            /* Get UNIX socket path */
            char *sun_path = si->psi.soi_proto.pri_un.unsi_addr.ua_sun.sun_path;
            if (sun_path[0] != '\0') {
                strncpy(sock.unix_path, sun_path, MAX_PATH_LEN - 1);
                strncpy(sock.local_addr, sun_path, MAX_ADDR_LEN - 1);
            } else {
                strcpy(sock.local_addr, "*");
            }
            
            /* Check connection state */
            if (si->psi.soi_proto.pri_un.unsi_conn_so != 0) {
                strcpy(sock.remote_addr, "[connected]");
            } else {
                strcpy(sock.remote_addr, "*");
            }
            
            sock.recv_queue = si->psi.soi_rcv.sbi_cc;
            sock.send_queue = si->psi.soi_snd.sbi_cc;
        } else {
            continue;  /* Skip other families */
        }
        
        /* Check if should include this socket */
        if (!should_include(&sock, opts)) {
            continue;
        }
        
        /* Check for duplicates */
        if (socket_exists(list, &sock)) {
            continue;
        }
        
        /* Get process name if needed */
        if (opts->show_process && !got_proc_name) {
            get_proc_name(pid, proc_name, sizeof(proc_name));
            got_proc_name = true;
        }
        
        if (opts->show_process) {
            strncpy(sock.proc_name, proc_name, MAX_PROC_NAME - 1);
        }
        
        /* Add to list */
        list = add_to_list(list, &sock);
    }
    
    free(fdinfo);
    return list;
}

/* Collect all sockets from all processes */
ss_sock_info_t *collect_all_sockets(const ss_options_t *opts)
{
    ss_sock_info_t *list = NULL;
    pid_t *pids = NULL;
    int num_pids;
    
    /* Get list of all PIDs */
    int bufsize = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (bufsize <= 0) {
        perror("proc_listpids");
        return NULL;
    }
    
    pids = malloc(bufsize);
    if (!pids) {
        perror("malloc");
        return NULL;
    }
    
    num_pids = proc_listpids(PROC_ALL_PIDS, 0, pids, bufsize);
    if (num_pids <= 0) {
        perror("proc_listpids");
        free(pids);
        return NULL;
    }
    
    num_pids /= sizeof(pid_t);
    
    /* Collect sockets from each process */
    for (int i = 0; i < num_pids; i++) {
        if (pids[i] == 0) continue;
        list = collect_process_sockets(pids[i], list, opts);
    }
    
    free(pids);
    return list;
}

/* Free socket list */
void free_socket_list(ss_sock_info_t *list)
{
    ss_sock_info_t *next;
    while (list) {
        next = list->next;
        free(list);
        list = next;
    }
}
