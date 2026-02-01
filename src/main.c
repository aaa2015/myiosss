/*
 * ss - Socket Statistics for Apple platforms (macOS/iOS)
 * Main entry point and argument parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include "ss.h"

static void parse_args(int argc, char *argv[], ss_options_t *opts);
static void collect_and_display(const ss_options_t *opts);
static void calculate_stats(ss_sock_info_t *list, ss_stats_t *stats);

int main(int argc, char *argv[])
{
    ss_options_t opts = {0};
    
    /* Parse command line arguments */
    parse_args(argc, argv, &opts);
    
    /* Handle version and help */
    if (opts.version) {
        print_version();
        return 0;
    }
    
    if (opts.help) {
        print_help(argv[0]);
        return 0;
    }
    
    /* Default: show TCP and UDP if nothing specified */
    if (!opts.show_tcp && !opts.show_udp && !opts.show_unix) {
        opts.show_tcp = true;
        opts.show_udp = true;
    }
    
    /* Collect and display socket information */
    collect_and_display(&opts);
    
    return 0;
}

static void parse_args(int argc, char *argv[], ss_options_t *opts)
{
    static struct option long_options[] = {
        {"tcp",       no_argument, 0, 't'},
        {"udp",       no_argument, 0, 'u'},
        {"unix",      no_argument, 0, 'x'},
        {"listening", no_argument, 0, 'l'},
        {"all",       no_argument, 0, 'a'},
        {"numeric",   no_argument, 0, 'n'},
        {"processes", no_argument, 0, 'p'},
        {"extended",  no_argument, 0, 'e'},
        {"summary",   no_argument, 0, 's'},
        {"ipv4",      no_argument, 0, '4'},
        {"ipv6",      no_argument, 0, '6'},
        {"version",   no_argument, 0, 'V'},
        {"help",      no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "tuxlanpes46Vh", 
                               long_options, &option_index)) != -1) {
        switch (opt) {
            case 't':
                opts->show_tcp = true;
                break;
            case 'u':
                opts->show_udp = true;
                break;
            case 'x':
                opts->show_unix = true;
                break;
            case 'l':
                opts->show_listening = true;
                break;
            case 'a':
                opts->show_all = true;
                break;
            case 'n':
                opts->numeric = true;
                break;
            case 'p':
                opts->show_process = true;
                break;
            case 'e':
                opts->extended = true;
                break;
            case 's':
                opts->summary = true;
                break;
            case '4':
                opts->ipv4_only = true;
                break;
            case '6':
                opts->ipv6_only = true;
                break;
            case 'V':
                opts->version = true;
                break;
            case 'h':
                opts->help = true;
                break;
            default:
                print_help(argv[0]);
                exit(1);
        }
    }
}

static void collect_and_display(const ss_options_t *opts)
{
    ss_sock_info_t *list = NULL;
    ss_stats_t stats = {0};
    
    /* Collect all sockets */
    list = collect_all_sockets(opts);
    
    if (opts->summary) {
        /* Calculate and print statistics */
        calculate_stats(list, &stats);
        print_summary(&stats);
    } else {
        /* Print header and sockets */
        print_header(opts);
        
        for (ss_sock_info_t *s = list; s != NULL; s = s->next) {
            print_socket(s, opts);
        }
    }
    
    /* Cleanup */
    free_socket_list(list);
}

static void calculate_stats(ss_sock_info_t *list, ss_stats_t *stats)
{
    for (ss_sock_info_t *s = list; s != NULL; s = s->next) {
        switch (s->protocol) {
            case SS_PROTO_TCP:
                stats->tcp_total++;
                switch (s->state) {
                    case SS_TCP_ESTABLISHED:
                        stats->tcp_established++;
                        break;
                    case SS_TCP_SYN_SENT:
                        stats->tcp_syn_sent++;
                        break;
                    case SS_TCP_SYN_RECV:
                        stats->tcp_syn_recv++;
                        break;
                    case SS_TCP_FIN_WAIT1:
                        stats->tcp_fin_wait1++;
                        break;
                    case SS_TCP_FIN_WAIT2:
                        stats->tcp_fin_wait2++;
                        break;
                    case SS_TCP_TIME_WAIT:
                        stats->tcp_time_wait++;
                        break;
                    case SS_TCP_CLOSE_WAIT:
                        stats->tcp_close_wait++;
                        break;
                    case SS_TCP_LAST_ACK:
                        stats->tcp_last_ack++;
                        break;
                    case SS_TCP_LISTEN:
                        stats->tcp_listen++;
                        break;
                    case SS_TCP_CLOSING:
                        stats->tcp_closing++;
                        break;
                    case SS_TCP_CLOSED:
                        stats->tcp_closed++;
                        break;
                    default:
                        break;
                }
                break;
            case SS_PROTO_UDP:
                stats->udp_total++;
                break;
            case SS_PROTO_UNIX_STREAM:
                stats->unix_stream_total++;
                break;
            case SS_PROTO_UNIX_DGRAM:
                stats->unix_dgram_total++;
                break;
            default:
                break;
        }
    }
}
