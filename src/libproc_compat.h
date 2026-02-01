/*
 * libproc compatibility header for iOS
 * These definitions are not included in the iOS SDK but the functions
 * exist on jailbroken devices.
 */

#ifndef LIBPROC_COMPAT_H
#define LIBPROC_COMPAT_H

#ifdef IOS_BUILD

#include <sys/types.h>
#include <stdint.h>

/* Process list types */
#define PROC_ALL_PIDS       1
#define PROC_PGRP_ONLY      2
#define PROC_TTY_ONLY       3
#define PROC_UID_ONLY       4
#define PROC_RUID_ONLY      5

/* File descriptor types */
#define PROX_FDTYPE_VNODE   1
#define PROX_FDTYPE_SOCKET  2
#define PROX_FDTYPE_PSHM    3
#define PROX_FDTYPE_PSEM    4
#define PROX_FDTYPE_KQUEUE  5
#define PROX_FDTYPE_PIPE    6
#define PROX_FDTYPE_FSEVENTS 7

/* Process info flavors */
#define PROC_PIDLISTFDS     1
#define PROC_PIDTBSDINFO    3
#define PROC_PIDFDSOCKETINFO 8

/* Path info max size */
#define PROC_PIDPATHINFO_MAXSIZE 4096

/* File descriptor info structure */
struct proc_fdinfo {
    int32_t  proc_fd;
    uint32_t proc_fdtype;
};

/* BSD info structure */
struct proc_bsdinfo {
    uint32_t pbi_flags;
    uint32_t pbi_status;
    uint32_t pbi_xstatus;
    uint32_t pbi_pid;
    uint32_t pbi_ppid;
    uid_t    pbi_uid;
    gid_t    pbi_gid;
    uid_t    pbi_ruid;
    gid_t    pbi_rgid;
    uid_t    pbi_svuid;
    gid_t    pbi_svgid;
    uint32_t rfu_1;
    char     pbi_comm[16];
    char     pbi_name[32];
    uint32_t pbi_nfiles;
    uint32_t pbi_pgid;
    uint32_t pbi_pjobc;
    uint32_t e_tdev;
    uint32_t e_tpgid;
    int32_t  pbi_nice;
    uint64_t pbi_start_tvsec;
    uint64_t pbi_start_tvusec;
};

/* Socket buffer info */
struct sockbuf_info {
    uint32_t sbi_cc;
    uint32_t sbi_hiwat;
    uint32_t sbi_mbcnt;
    uint32_t sbi_mbmax;
    uint32_t sbi_lowat;
    short    sbi_flags;
    short    sbi_timeo;
};

/* In address union */
union in_addr_4_6 {
    struct in_addr  i46a_addr4;
    struct in6_addr i46a_addr6;
};

/* Internet address info */
struct in_addr_info {
    int                  ina_family;
    union in_addr_4_6    ina_46;
};

/* TCP socket info */
struct tcp_sockinfo {
    struct in_addr_info tcpsi_ini;
    int                 tcpsi_state;
    int                 tcpsi_timer[4];
    uint32_t            tcpsi_mss;
    uint32_t            tcpsi_flags;
    uint32_t            rfu_1;
    uint64_t            tcpsi_tp;
};

/* Internet socket info */
struct in_sockinfo {
    struct in_addr_info insi_laddr;
    struct in_addr_info insi_faddr;
    int                 insi_v4;
    int                 insi_v6;
    int                 insi_vflag;
    int                 insi_lport;
    int                 insi_fport;
    uint32_t            insi_flags;
    uint32_t            insi_flow;
    uint8_t             insi_ip_ttl;
    uint32_t            rfu_1;
};

/* UNIX socket address */
struct un_sockaddr_un {
    uint8_t  sun_len;
    uint8_t  sun_family;
    char     sun_path[104];
};

/* UNIX address */
union un_addr {
    struct un_sockaddr_un ua_sun;
    char                  ua_dummy[256];
};

/* UNIX socket info */
struct un_sockinfo {
    uint64_t        unsi_conn_so;
    uint64_t        unsi_conn_pcb;
    union un_addr   unsi_addr;
    union un_addr   unsi_caddr;
};

/* Protocol info union */
union proto_info {
    struct in_sockinfo  pri_in;
    struct tcp_sockinfo pri_tcp;
    struct un_sockinfo  pri_un;
};

/* Socket info kinds */
#define SOCKINFO_GENERIC    0
#define SOCKINFO_IN         1
#define SOCKINFO_TCP        2
#define SOCKINFO_UN         3

/* Socket info structure */
struct socket_info {
    struct sockbuf_info soi_rcv;
    struct sockbuf_info soi_snd;
    int                 soi_so;
    int                 soi_type;
    int                 soi_protocol;
    int                 soi_family;
    short               soi_options;
    short               soi_linger;
    short               soi_state;
    short               soi_qlen;
    short               soi_incqlen;
    short               soi_qlimit;
    short               soi_timeo;
    uint16_t            soi_error;
    uint32_t            soi_oobmark;
    uint32_t            rfu_1;
    int                 soi_kind;
    union proto_info    soi_proto;
};

/* Socket FD info structure */
struct socket_fdinfo {
    struct proc_fdinfo pfi;
    struct socket_info psi;
};

/* Function declarations */
extern int proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize);
extern int proc_pidinfo(int pid, int flavor, uint64_t arg, void *buffer, int buffersize);
extern int proc_pidfdinfo(int pid, int fd, int flavor, void *buffer, int buffersize);
extern int proc_pidpath(int pid, void *buffer, uint32_t buffersize);
extern int proc_name(int pid, void *buffer, uint32_t buffersize);

#else /* !IOS_BUILD */

#include <libproc.h>
#include <sys/proc_info.h>

#endif /* IOS_BUILD */

#endif /* LIBPROC_COMPAT_H */
