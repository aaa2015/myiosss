# SS - Socket Statistics for Apple Platforms

A Linux `ss` command implementation for macOS and iOS (jailbroken). Provides socket statistics with process information, matching the familiar Linux `ss` output format.

## Features

- Display TCP/UDP socket statistics
- Show listening sockets (`-l`)
- Display process information (`-p`)
- Filter by IPv4/IPv6 (`-4`, `-6`)
- State filtering (`state established`, `state listening`, etc.)
- Output format compatible with Linux `ss`
- High performance: ~0.2s with process info (vs 4.8s before optimization)

## Installation

### macOS (Homebrew)

```bash
brew tap fangqingyuan/ss
brew install ss-apple
```

### iOS (Cydia/Sileo)

Add the following source to Cydia or Sileo:
```
https://fangqingyuan.github.io/ss-apple/
```

Then search for and install "SS (Socket Statistics)".

### Build from Source

#### macOS

```bash
git clone https://github.com/fangqingyuan/ss-apple.git
cd ss-apple
make macos
sudo make install
```

#### iOS (requires Xcode)

```bash
make ios
# Transfer build/ss-ios and build/ss_proc to your device
```

## Usage

```bash
# Show all listening TCP/UDP sockets with process info
ss -tulnp

# Show only TCP listening sockets
ss -tln

# Show established connections
ss -t state established

# Show all sockets (including TIME-WAIT, etc.)
ss -ta

# IPv4 only
ss -4 -tulnp

# No header
ss -H -tulnp

# Summary statistics
ss -s
```

## Options

```
Usage: ss [OPTIONS] [state STATE-FILTER]

Options:
  -t, --tcp        Display TCP sockets
  -u, --udp        Display UDP sockets
  -x, --unix       Display UNIX sockets (limited)
  -l, --listening  Display listening sockets only
  -a, --all        Display all sockets
  -n, --numeric    Numeric output (default)
  -p, --processes  Show process using socket
  -s, --summary    Summary statistics
  -4, --ipv4       IPv4 only
  -6, --ipv6       IPv6 only
  -H, --no-header  Suppress header line
  -V, --version    Show version
  -h, --help       Show help

STATE-FILTER:
  established, syn-sent, syn-recv, fin-wait-1, fin-wait-2,
  time-wait, close-wait, last-ack, listening, closing, closed
  all, connected
```

## Example Output

```
Netid   State    Recv-Q Send-Q    Local Address:Port     Peer Address:Port Process
udp     UNCONN        0      0          0.0.0.0:5353          0.0.0.0:*    users:(("mDNSResponder",pid=423,fd=6))
tcp     LISTEN        0      0          0.0.0.0:22            0.0.0.0:*    users:(("sshd",pid=1234,fd=4))
tcp     LISTEN        0      0             [::]:22               [::]:*    users:(("sshd",pid=1234,fd=4))
tcp     ESTAB         0      0    192.168.1.10:22     192.168.1.100:54321  users:(("sshd",pid=5678,fd=5))
```

## Architecture

### macOS
Uses native C implementation with `libproc` API for process and socket information.

### iOS
- **ss** - Shell script wrapper that parses `netstat` output
- **ss_proc** - Fast native C program for port-to-process mapping (replaces slow `lsof`)

## Performance

| Command | Before | After | Improvement |
|---------|--------|-------|-------------|
| `ss -tuln` | 0.6s | 0.16s | 3.7x |
| `ss -tulnp` | 4.8s | 0.21s | 23x |

## Differences from Linux ss

- UNIX socket support is limited on iOS
- Some advanced options (like `-m` for memory, `-i` for TCP info) are not available
- Uses `netstat` backend on iOS instead of direct kernel access

## Requirements

- **macOS**: macOS 10.15+ (Catalina or later)
- **iOS**: iOS 13.0+ (jailbroken)

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Related Projects

- [iproute2](https://github.com/iproute2/iproute2) - Linux networking tools including the original `ss`
