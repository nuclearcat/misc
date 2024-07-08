#!/bin/sh
clang -g -O2 -I/usr/include/x86_64-linux-gnu -target bpf -c ebpf_fw.c -o ebpf_fw.o
# if ok
if [ $? -eq 0 ]; then
PDIR=$(pwd)
sudo tc qdisc del dev enp4s0 ingress
sudo tc qdisc add dev enp4s0 handle ffff: ingress
sudo tc filter add dev enp4s0 ingress bpf object-file ${PDIR}/ebpf_fw.o section filter direct-action
sudo tc filter show dev enp4s0 ingress
echo "Output from trace_pipe:"
sudo cat /sys/kernel/debug/tracing/trace_pipe
fi

