/*
    Simple SSH eBPF firewall with port knocking support
*/

#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <linux/udp.h>
#include <inttypes.h>
#include <linux/pkt_cls.h>

#define htons(x) ((__be16)___constant_swab16((x)))
#define htonl(x) ((__be32)___constant_swab32((x)))

/*
Knocking method:

1) Knocking on port 0
#define KNOCK_PORT 0
Just connect to the host - port 0 by default, you can use
tcp_zero_port_knocker.c for such purpose, as most of the
software will not allow you to connect to port 0.

2) UDP knocking with magic bytes
#define KNOCK_PORT 0
#define KNOCK_UDP_MAGIC 0xdeadbeef
Send 4 bytes of 0xdeadbeef to the host - port 0 by default.

You can send it over netcat
echo -n -e '\xde\xad\xbe\xef' | nc -u -w1 dst_host KNOCK_PORT

*/

#define KNOCK_EXPIRE_TIME 60
#define KNOCK_PORT 0
#define KNOCK_UDP_MAGIC 0xdeadbeef

// debug readable by cat /sys/kernel/debug/tracing/trace_pipe
#define DEBUG

#ifndef __section
# define __section(NAME)                  \
   __attribute__((section(NAME), used))
#endif

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 100000);
    __type(key, uint32_t);
    __type(value, uint64_t);
} flow_map __section(".maps");

static inline int handle_udp(struct __sk_buff *skb) {
    struct ethhdr *eth = (struct ethhdr *)(long)skb->data;
    struct iphdr *ip = (struct iphdr *)(eth + 1);
    struct udphdr *udp = (struct udphdr *)(ip + 1);

    // check size
    if ((void *)((void*)udp + sizeof(*udp)) > (void *)(long)skb->data_end) {
        return TC_ACT_SHOT;
    }

    if (udp->dest != htons(KNOCK_PORT)) {
        return TC_ACT_OK;
    }

    // check if size is exactly payload size (4 byte)
    if ((void *)((void*)udp + sizeof(*udp) + 4) > (void *)(long)skb->data_end) {
        return TC_ACT_OK;
    }

    uint32_t *magic = (uint32_t *)(udp + 1);
    if (*magic == htonl(KNOCK_UDP_MAGIC)) {
        uint64_t ts = bpf_ktime_get_ns();
#ifdef DEBUG
        bpf_printk("Knock on UDP port %u from %u\n", udp->dest, ip->saddr);
#endif        
        bpf_map_update_elem(&flow_map, &ip->saddr, &ts, BPF_ANY);
        return TC_ACT_OK;
    }

    return TC_ACT_OK;
}


static inline int handle_tcp(struct __sk_buff *skb) {
    struct ethhdr *eth = (struct ethhdr *)(long)skb->data;
    struct iphdr *ip = (struct iphdr *)(eth + 1);
    struct tcphdr *tcp = (struct tcphdr *)(ip + 1);

    // check size
    if ((void *)((void*)tcp + sizeof(*tcp)) > (void *)(long)skb->data_end) {
        return TC_ACT_SHOT;
    }

    // is it SYN packet?
    if (!tcp->syn) {
        return TC_ACT_OK;
    }

// have KNOCK_PORT but not KNOCK_UDP_MAGIC
#if defined(KNOCK_PORT) && !defined(KNOCK_UDP_MAGIC)
    if (tcp->dest == htons(KNOCK_PORT)) {
        uint64_t ts = bpf_ktime_get_ns();
        bpf_map_update_elem(&flow_map, &ip->saddr, &ts, BPF_ANY);
        return TC_ACT_OK;
    }
#endif

    if (tcp->dest != htons(22)) {
        return TC_ACT_OK;
    }

    // check if src ip is in flow_map
    uint64_t *value = bpf_map_lookup_elem(&flow_map, &ip->saddr);
    if (!value) {
#ifdef DEBUG
        bpf_printk("No entry for %u\n", ip->saddr);
#endif
        return TC_ACT_SHOT;
    } else {
        uint64_t ts = bpf_ktime_get_ns();
        // is KNOCK expired?
        if ((ts - *value) > (KNOCK_EXPIRE_TIME * 1000000000LU)) {
#ifdef DEBUG
            bpf_printk("Expired entry for %u\n", ip->saddr);
#endif            
            bpf_map_delete_elem(&flow_map, &ip->saddr);
            return TC_ACT_SHOT;
        } else {
#ifdef DEBUG
            bpf_printk("Found entry for %u\n", ip->saddr);
#endif
            return TC_ACT_OK;
        }
    }

    return TC_ACT_OK;
}

static inline int handle_ipv4(struct __sk_buff *skb) {
    struct ethhdr *eth = (struct ethhdr *)(long)skb->data;
    struct iphdr *ip = (struct iphdr *)(eth + 1);

    // check size
    if ((void *)((void*)ip + sizeof(*ip)) > (void *)(long)skb->data_end) {
        return TC_ACT_SHOT;
    }

/* UDP magic knocking */
#if defined(KNOCK_PORT) && defined(KNOCK_UDP_MAGIC)
    if (ip->protocol == IPPROTO_UDP) {
        return handle_udp(skb);
    }
#endif

    if (ip->protocol == IPPROTO_TCP) {
        return handle_tcp(skb);
    }

    return TC_ACT_OK;
}


__section("filter") 
int filter_func(struct __sk_buff *skb)
{
    struct ethhdr *eth;
    struct iphdr *ip;
    struct tcphdr *tcp;

    void *data_end = (void *)(long)skb->data_end;
    void *data = (void *)(long)skb->data;
    if (data + sizeof(*eth) + sizeof(*ip) > data_end)
        return TC_ACT_OK;

    eth = data;
    if (eth->h_proto == htons(ETH_P_IP)) {
        return handle_ipv4(skb);
    }
        
    return TC_ACT_OK;
}

char _license[] __section("license") = "GPL";
