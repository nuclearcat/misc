# eBPF based port knocking firewall

## Description

This is a simple port knocking firewall based on eBPF. It uses a BPF_MAP_TYPE_HASH map to store permitted IP and timestamp for lifetime of the entry. The program is loaded using tc filter into ingress path of the interface. The program is written in C and compiled using clang with LLVM backend.

## Usage

You can change #define values in ebpf_fw.c to suit your needs. Program have 2 modes:
- `PORT_KNOCKING` - You need to tcp knock on secret port to open ssh access for X seconds. You can use port 0, as it is not used by any service and most of programs even doesn't allow it as parameter.
- `KNOCK_UDP_MAGIC` - You need to send UDP packet with magic value to open ssh access for X seconds to specified port (you don't need to listen on it).


Check comments inside ebpf_fw.c for more details.

## Notes

This is unfinished project, but it works. I'm not responsible for any damage caused by using this software. Use at your own risk.
For now IPv4 only.
