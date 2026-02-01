/*
 * ss_proc - Fast port-to-process mapper for iOS
 * Replaces lsof for ss command, ~400x faster
 * 
 * Output format: port cmd pid fd
 * Example: 22 sshd 1234 4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>

/* libproc API declarations (not in iOS SDK headers) */
#define PROC_ALL_PIDS 1
#define PROC_PIDLISTFDS 1
#define PROC_PIDTBSDINFO 3
#define PROX_FDTYPE_SOCKET 2
#define PROC_PIDFDSOCKETINFO 3

struct proc_fdinfo {
    int32_t proc_fd;
    uint32_t proc_fdtype;
};

struct proc_bsdinfo {
    uint32_t pbi_flags;
    uint32_t pbi_status;
    uint32_t pbi_xstatus;
    uint32_t pbi_pid;
    uint32_t pbi_ppid;
    uid_t pbi_uid;
    gid_t pbi_gid;
    uid_t pbi_ruid;
    gid_t pbi_rgid;
    uid_t pbi_svuid;
    gid_t pbi_svgid;
    uint32_t rfu_1;
    char pbi_comm[16];
    char pbi_name[32];
    uint32_t pbi_nfiles;
    uint32_t pbi_pgid;
    uint32_t pbi_pjobc;
    uint32_t e_tdev;
    uint32_t e_tpgid;
    int32_t pbi_nice;
    uint64_t pbi_start_tvsec;
    uint64_t pbi_start_tvusec;
};

extern int proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize);
extern int proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer, int buffersize);
extern int proc_pidfdinfo(int pid, int fd, int flavor, void *buffer, int buffersize);
extern int proc_name(int pid, void *buffer, uint32_t buffersize);

/* Structure to hold port->process mapping */
typedef struct {
    uint16_t port;
    pid_t pid;
    int fd;
    char cmd[17];
    int is_launchd;  /* Flag to deprioritize launchd */
} port_proc_t;

/* Hash table for port->process mapping (simple array, max 65536 ports) */
static port_proc_t port_map[65536];

int main(int argc, char *argv[]) {
    /* Initialize port map */
    memset(port_map, 0, sizeof(port_map));
    
    /* Get all PIDs */
    int bufsize = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (bufsize <= 0) {
        fprintf(stderr, "Failed to get process list\n");
        return 1;
    }
    
    pid_t *pids = malloc(bufsize);
    if (!pids) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    int count = proc_listpids(PROC_ALL_PIDS, 0, pids, bufsize);
    int num_pids = count / sizeof(pid_t);
    
    /* Iterate all processes */
    for (int i = 0; i < num_pids; i++) {
        pid_t pid = pids[i];
        if (pid == 0) continue;
        
        /* Get process name */
        char proc_name_buf[256];
        proc_name_buf[0] = '\0';
        proc_name(pid, proc_name_buf, sizeof(proc_name_buf));
        
        /* Fallback: get from bsdinfo if proc_name failed */
        if (proc_name_buf[0] == '\0') {
            struct proc_bsdinfo bsdinfo;
            if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo)) > 0) {
                strncpy(proc_name_buf, bsdinfo.pbi_comm, sizeof(proc_name_buf) - 1);
            }
        }
        
        int is_launchd = (strcmp(proc_name_buf, "launchd") == 0);
        
        /* Get FD list for this process */
        struct proc_fdinfo fds[4096];
        int fd_count = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fds, sizeof(fds));
        if (fd_count <= 0) continue;
        
        int num_fds = fd_count / sizeof(struct proc_fdinfo);
        
        for (int j = 0; j < num_fds; j++) {
            if (fds[j].proc_fdtype != PROX_FDTYPE_SOCKET) continue;
            
            /* Get socket info - use large buffer for compatibility */
            char si_buf[2048];
            memset(si_buf, 0, sizeof(si_buf));
            int ret = proc_pidfdinfo(pid, fds[j].proc_fd, PROC_PIDFDSOCKETINFO, si_buf, sizeof(si_buf));
            if (ret <= 0) continue;
            
            /* Parse socket info - find family and port */
            /* The structure varies between iOS versions, so we search for patterns */
            int *vals = (int *)si_buf;
            int family = 0;
            uint16_t local_port = 0;
            
            /* First int values typically contain family */
            for (int k = 0; k < 8; k++) {
                if (vals[k] == AF_INET || vals[k] == AF_INET6) {
                    family = vals[k];
                    break;
                }
            }
            
            if (family == 0) continue;  /* Not a network socket */
            
            /* Extract port from socket_fdinfo structure */
            /* On iOS 13, local port is at offset 0x10c (268) in big-endian */
            uint8_t *bytes = (uint8_t *)si_buf;
            
            /* Primary offset: 0x10c for iOS 13 */
            if (ret > 0x10e) {
                uint16_t port_be = *(uint16_t *)(bytes + 0x10c);
                local_port = ntohs(port_be);
            }
            
            /* Fallback: scan nearby offsets if primary fails */
            if (local_port == 0 || local_port >= 65535) {
                int fallback_offsets[] = {0x10a, 0x10e, 0x110, 0x108, -1};
                for (int k = 0; fallback_offsets[k] >= 0; k++) {
                    int off = fallback_offsets[k];
                    if (off + 2 > ret) continue;
                    uint16_t port_be = *(uint16_t *)(bytes + off);
                    uint16_t port = ntohs(port_be);
                    if (port > 0 && port < 65535) {
                        local_port = port;
                        break;
                    }
                }
            }
            
            if (local_port == 0 || local_port >= 65535) continue;
            
            /* Store in port map - prefer non-launchd processes */
            port_proc_t *entry = &port_map[local_port];
            
            if (entry->port == 0 || 
                (entry->is_launchd && !is_launchd)) {
                entry->port = local_port;
                entry->pid = pid;
                entry->fd = fds[j].proc_fd;
                strncpy(entry->cmd, proc_name_buf, sizeof(entry->cmd) - 1);
                entry->cmd[sizeof(entry->cmd) - 1] = '\0';
                entry->is_launchd = is_launchd;
            }
        }
    }
    
    /* Output all port mappings */
    for (int port = 1; port < 65536; port++) {
        if (port_map[port].port > 0) {
            printf("%d %s %d %d\n", 
                   port_map[port].port,
                   port_map[port].cmd,
                   port_map[port].pid,
                   port_map[port].fd);
        }
    }
    
    free(pids);
    return 0;
}
