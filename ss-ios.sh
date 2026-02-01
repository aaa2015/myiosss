#!/bin/bash
# ss - Socket Statistics wrapper for iOS (jailbroken)
# Parses netstat/lsof output to provide ss-like interface

VERSION="1.0.1"

# Default options
SHOW_TCP=false
SHOW_UDP=false
SHOW_UNIX=false
SHOW_LISTENING=false
SHOW_ALL=false
SHOW_NUMERIC=true
SHOW_PROCESS=false
SHOW_SUMMARY=false
SHOW_HELP=false
SHOW_VERSION=false
IPV4_ONLY=false
IPV6_ONLY=false
NO_HEADER=false
STATE_FILTER=""

# Convert long options to short options
ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --tcp)        ARGS+=("-t") ;;
        --udp)        ARGS+=("-u") ;;
        --unix)       ARGS+=("-x") ;;
        --listening)  ARGS+=("-l") ;;
        --all)        ARGS+=("-a") ;;
        --numeric)    ARGS+=("-n") ;;
        --processes)  ARGS+=("-p") ;;
        --summary)    ARGS+=("-s") ;;
        --ipv4)       ARGS+=("-4") ;;
        --ipv6)       ARGS+=("-6") ;;
        --version)    ARGS+=("-V") ;;
        --help)       ARGS+=("-h") ;;
        --no-header)  ARGS+=("-H") ;;
        -*)           ARGS+=("$1") ;;
        *)            ARGS+=("$1") ;;
    esac
    shift
done
set -- "${ARGS[@]}"

# Parse arguments
while getopts "tulaxnps46VhH" opt; do
    case $opt in
        t) SHOW_TCP=true ;;
        u) SHOW_UDP=true ;;
        l) SHOW_LISTENING=true ;;
        a) SHOW_ALL=true ;;
        x) SHOW_UNIX=true ;;
        n) SHOW_NUMERIC=true ;;
        p) SHOW_PROCESS=true ;;
        s) SHOW_SUMMARY=true ;;
        4) IPV4_ONLY=true ;;
        6) IPV6_ONLY=true ;;
        V) SHOW_VERSION=true ;;
        h) SHOW_HELP=true ;;
        H) NO_HEADER=true ;;
        *) SHOW_HELP=true ;;
    esac
done

# Shift past the options to get positional arguments (state filter)
shift $((OPTIND-1))

# Parse state filter: "state XXXX" or just state name
if [[ "$1" == "state" ]] && [[ -n "$2" ]]; then
    STATE_FILTER="$2"
elif [[ -n "$1" ]] && [[ "$1" != "state" ]]; then
    # Direct state name
    STATE_FILTER="$1"
fi

# Normalize state filter to uppercase
STATE_FILTER=$(echo "$STATE_FILTER" | tr '[:lower:]' '[:upper:]' | tr '-' '_')

# Show version
if $SHOW_VERSION; then
    echo "ss (iOS wrapper) version $VERSION"
    echo "Socket Statistics for iOS (jailbroken devices)"
    echo "Uses netstat/lsof backend"
    exit 0
fi

# Show help
if $SHOW_HELP; then
    echo "Usage: ss [OPTIONS] [state STATE-FILTER]"
    echo ""
    echo "Socket Statistics for iOS (jailbroken)"
    echo ""
    echo "Options:"
    echo "  -t, --tcp        Display TCP sockets"
    echo "  -u, --udp        Display UDP sockets"
    echo "  -x, --unix       Display UNIX sockets (limited)"
    echo "  -l, --listening  Display listening sockets only"
    echo "  -a, --all        Display all sockets"
    echo "  -n, --numeric    Numeric output (default)"
    echo "  -p, --processes  Show process using socket"
    echo "  -s, --summary    Summary statistics"
    echo "  -4, --ipv4       IPv4 only"
    echo "  -6, --ipv6       IPv6 only"
    echo "  -H, --no-header  Suppress header line"
    echo "  -V, --version    Show version"
    echo "  -h, --help       Show help"
    echo ""
    echo "STATE-FILTER:"
    echo "  established, syn-sent, syn-recv, fin-wait-1, fin-wait-2,"
    echo "  time-wait, close-wait, last-ack, listening, closing, closed"
    echo "  all, connected"
    echo ""
    echo "Examples:"
    echo "  ss -tulnp                    # Show listening TCP/UDP with process"
    echo "  ss --tcp --listening         # Same as ss -tl"
    echo "  ss -t state established      # Show established TCP connections"
    echo "  ss -ta state time-wait       # Show TCP TIME-WAIT sockets"
    echo "  ss -H -tulnp                 # No header output"
    exit 0
fi

# Default: show TCP and UDP if nothing specified
if ! $SHOW_TCP && ! $SHOW_UDP && ! $SHOW_UNIX; then
    SHOW_TCP=true
    SHOW_UDP=true
fi

# Summary mode
if $SHOW_SUMMARY; then
    echo "Total: $(netstat -an 2>/dev/null | grep -c '^tcp\|^udp')"
    echo ""
    
    tcp_total=$(netstat -anp tcp 2>/dev/null | grep -c '^tcp')
    tcp_estab=$(netstat -anp tcp 2>/dev/null | grep -c 'ESTABLISHED')
    tcp_listen=$(netstat -anp tcp 2>/dev/null | grep -c 'LISTEN')
    tcp_timewait=$(netstat -anp tcp 2>/dev/null | grep -c 'TIME_WAIT')
    tcp_closewait=$(netstat -anp tcp 2>/dev/null | grep -c 'CLOSE_WAIT')
    
    echo "TCP:   $tcp_total (estab $tcp_estab, listen $tcp_listen, timewait $tcp_timewait, closewait $tcp_closewait)"
    
    udp_total=$(netstat -anp udp 2>/dev/null | grep -c '^udp')
    echo "UDP:   $udp_total"
    exit 0
fi

# Build process lookup table
PROC_FILE="/tmp/ss_proc_$$"
if $SHOW_PROCESS; then
    # Use fast native ss_proc if available, fallback to lsof
    if [[ -x /usr/local/bin/ss_proc ]]; then
        /usr/local/bin/ss_proc > "$PROC_FILE" 2>/dev/null
    else
        # Fallback: Use awk to parse lsof output in one pass
        lsof -i -P -n 2>/dev/null | awk 'NR>1 {
            cmd=$1; pid=$2; fd=$4
            name=$9
            gsub(/[^0-9]/, "", fd)
            if (index(name, "->") > 0) {
                split(name, parts, "->")
                n = split(parts[1], addr, ":")
                port = addr[n]
            } else {
                n = split(name, addr, ":")
                port = addr[n]
                gsub(/[^0-9]/, "", port)
            }
            if (port ~ /^[0-9]+$/) {
                priority = (cmd == "launchd") ? 1 : 0
                print priority, port, cmd, pid, fd
            }
        }' | sort -k1,1n -k2,2n | awk '!seen[$2]++ {print $2, $3, $4, $5}' > "$PROC_FILE"
    fi
fi

# Print header (unless -H specified)
if ! $NO_HEADER; then
    if $SHOW_PROCESS; then
        printf "%-7s %-8s %8s %8s %40s %20s %s\n" \
            "Netid" "State" "Recv-Q" "Send-Q" "Local Address:Port" "Peer Address:Port" "Process"
    else
        printf "%-6s %-12s %8s %8s %-40s %-40s\n" \
            "Netid" "State" "Recv-Q" "Send-Q" "Local Address:Port" "Peer Address:Port"
    fi
fi

# Main processing using awk for speed (avoids shell loops and external commands)
{
    # Collect UDP sockets first, then TCP
    if $SHOW_UDP; then
        netstat -anp udp 2>/dev/null | tail -n +3
    fi
    if $SHOW_TCP; then
        netstat -anp tcp 2>/dev/null | tail -n +3
    fi
} | awk -v show_process="$SHOW_PROCESS" \
       -v show_listening="$SHOW_LISTENING" \
       -v show_all="$SHOW_ALL" \
       -v ipv4_only="$IPV4_ONLY" \
       -v ipv6_only="$IPV6_ONLY" \
       -v state_filter="$STATE_FILTER" \
       -v proc_file="$PROC_FILE" \
       -v show_tcp="$SHOW_TCP" \
       -v show_udp="$SHOW_UDP" '
BEGIN {
    # Load process info into array if file exists
    if (show_process == "true" && proc_file != "") {
        while ((getline line < proc_file) > 0) {
            split(line, parts, " ")
            port = parts[1]
            cmd = parts[2]
            pid = parts[3]
            fd = parts[4]
            if (!(port in proc_cmd)) {
                proc_cmd[port] = cmd
                proc_pid[port] = pid
                proc_fd[port] = fd
            }
        }
        close(proc_file)
    }
}

function convert_addr(addr, proto) {
    is_ipv6 = (proto ~ /6$/)
    
    if (addr == "*.*") {
        return is_ipv6 ? "[::]:*" : "0.0.0.0:*"
    }
    
    # Check if IPv6 (contains multiple colons before the last dot)
    n = split(addr, parts, ".")
    port = parts[n]
    
    # Rebuild IP (all parts except last)
    ip = ""
    for (i = 1; i < n; i++) {
        ip = ip (i > 1 ? "." : "") parts[i]
    }
    
    if (ip == "*") {
        return is_ipv6 ? "[::]:" port : "0.0.0.0:" port
    }
    
    # Check if IPv6 by looking for colons
    if (index(ip, ":") > 0) {
        return "[" ip "]:" port
    }
    
    return ip ":" port
}

function map_state(state) {
    if (state == "ESTABLISHED") return "ESTAB"
    if (state == "LISTEN") return "LISTEN"
    if (state == "TIME_WAIT") return "TIME-WAIT"
    if (state == "CLOSE_WAIT") return "CLOSE-WAIT"
    if (state == "FIN_WAIT_1") return "FIN-WAIT-1"
    if (state == "FIN_WAIT_2") return "FIN-WAIT-2"
    if (state == "SYN_SENT") return "SYN-SENT"
    if (state == "SYN_RCVD") return "SYN-RECV"
    if (state == "LAST_ACK") return "LAST-ACK"
    if (state == "CLOSING") return "CLOSING"
    if (state == "CLOSED") return "CLOSED"
    if (state == "") return "UNCONN"
    return state
}

function match_filter(state, filter) {
    if (filter == "") return 1
    
    # Normalize
    gsub(/-/, "_", state)
    state = toupper(state)
    filter = toupper(filter)
    gsub(/-/, "_", filter)
    
    if (filter == "ALL") return 1
    if (filter == "CONNECTED") {
        return (state ~ /^(ESTABLISHED|SYN_SENT|SYN_RECV|SYN_RCVD|FIN_WAIT_1|FIN_WAIT_2|TIME_WAIT|CLOSE_WAIT|LAST_ACK|CLOSING)$/)
    }
    if (filter == "LISTENING" || filter == "LISTEN") return (state == "LISTEN")
    if (filter == "ESTABLISHED" || filter == "ESTAB") return (state == "ESTABLISHED")
    return (state == filter)
}

function get_port(addr) {
    n = split(addr, parts, ".")
    return parts[n]
}

function get_process(port) {
    if (port in proc_cmd) {
        return "users:((\"" proc_cmd[port] "\",pid=" proc_pid[port] ",fd=" proc_fd[port] "))"
    }
    return ""
}

{
    proto = $1
    recvq = $2
    sendq = $3
    local = $4
    foreign = $5
    state = $6
    
    # Determine if TCP or UDP
    is_tcp = (proto ~ /^tcp/)
    is_udp = (proto ~ /^udp/)
    
    # Skip if protocol not requested
    if (is_tcp && show_tcp != "true") next
    if (is_udp && show_udp != "true") next
    
    # Filter by IP version
    if (ipv4_only == "true" && proto ~ /6$/) next
    if (ipv6_only == "true" && proto ~ /4$/) next
    
    # Get local port
    local_port = get_port(local)
    if (local_port == "*" || local_port == "") next
    
    # UDP handling
    if (is_udp) {
        if (show_listening == "true" && foreign != "*.*") next
        ss_state = "UNCONN"
        dedup_key = proto ":" local_port
    }
    # TCP handling
    else {
        if (show_listening == "true" && state != "LISTEN") next
        
        if (state_filter != "") {
            if (!match_filter(state, state_filter)) next
        } else if (show_all != "true" && show_listening != "true") {
            if (state != "ESTABLISHED" && state != "LISTEN") next
        }
        
        ss_state = map_state(state)
        dedup_key = proto ":" local_port ":" ss_state
    }
    
    # Deduplication
    if (dedup_key in seen) next
    seen[dedup_key] = 1
    
    # Convert addresses
    local_addr = convert_addr(local, proto)
    foreign_addr = convert_addr(foreign, proto)
    
    # Netid
    netid = is_udp ? "udp" : "tcp"
    
    # Output
    if (show_process == "true") {
        proc_info = get_process(local_port)
        printf "%-7s %-8s %8s %8s %40s %20s %s\n", netid, ss_state, recvq, sendq, local_addr, foreign_addr, proc_info
    } else {
        printf "%-6s %-12s %8s %8s %-40s %-40s\n", netid, ss_state, recvq, sendq, local_addr, foreign_addr
    }
}'

# Cleanup temp file
rm -f "$PROC_FILE" 2>/dev/null

# UNIX sockets (limited support)
if $SHOW_UNIX; then
    echo "# UNIX socket support limited on iOS - use 'lsof -U' for details"
fi
