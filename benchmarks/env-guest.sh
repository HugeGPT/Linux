> sudo lsb_release -a

Distributor ID:	Ubuntu
Description:	Ubuntu 18.04.5 LTS
Release:	18.04
Codename:	bionic

> sudo lscpu

Architecture:        x86_64
CPU op-mode(s):      32-bit, 64-bit
Byte Order:          Little Endian
CPU(s):              80
On-line CPU(s) list: 0-79
Thread(s) per core:  1
Core(s) per socket:  1
Socket(s):           80
NUMA node(s):        1
Vendor ID:           GenuineIntel
CPU family:          6
Model:               85
Model name:          Intel Xeon Processor (Skylake, IBRS)
Stepping:            4
CPU MHz:             1995.312
BogoMIPS:            3990.62
Virtualization:      VT-x
Hypervisor vendor:   KVM
Virtualization type: full
L1d cache:           32K
L1i cache:           32K
L2 cache:            4096K
L3 cache:            16384K
NUMA node0 CPU(s):   0-79
Flags:               fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ss syscall nx pdpe1gb rdtscp lm constant_tsc rep_good nopl xtopology cpuid tsc_known_freq pni pclmulqdq vmx ssse3 fma cx16 pdcm pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c rdrand hypervisor lahf_lm abm 3dnowprefetch cpuid_fault invpcid_single pti ssbd ibrs ibpb stibp tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase tsc_adjust bmi1 hle avx2 smep bmi2 erms invpcid rtm avx512f avx512dq rdseed adx smap clflushopt clwb avx512cd avx512bw avx512vl xsaveopt xsavec xgetbv1 xsaves arat umip pku ospke md_clear arch_capabilities

> sudo lsmod

Module                  Size  Used by
intel_rapl_msr         20480  0
intel_rapl_common      24576  1 intel_rapl_msr
qxl                    61440  1
isst_if_common         16384  0
drm_ttm_helper         16384  1 qxl
crct10dif_pclmul       16384  1
crc32_pclmul           16384  0
ttm                   102400  2 qxl,drm_ttm_helper
ghash_clmulni_intel    16384  0
aesni_intel           372736  0
glue_helper            16384  1 aesni_intel
drm_kms_helper        233472  3 qxl
crypto_simd            16384  1 aesni_intel
cryptd                 24576  2 crypto_simd,ghash_clmulni_intel
syscopyarea            16384  1 drm_kms_helper
sysfillrect            16384  1 drm_kms_helper
sysimgblt              16384  1 drm_kms_helper
joydev                 24576  0
fb_sys_fops            16384  1 drm_kms_helper
kvm_intel             258048  0
input_leds             16384  0
cec                    45056  1 drm_kms_helper
drm                   524288  6 drm_kms_helper,qxl,drm_ttm_helper,ttm
rapl                   20480  0
qemu_fw_cfg            20480  0
serio_raw              20480  0
lpc_ich                24576  0
mac_hid                16384  0
sch_fq_codel           20480  2
parport_pc             49152  0
ppdev                  24576  0
lp                     20480  0
parport                65536  3 parport_pc,lp,ppdev
virtio_rng             16384  0
ip_tables              28672  0
x_tables               49152  1 ip_tables
autofs4                45056  0
hid_generic            16384  0
usbhid                 53248  0
hid                   122880  2 usbhid,hid_generic
virtio_net             57344  0
psmouse               151552  0
net_failover           20480  1 virtio_net
ahci                   40960  0
failover               16384  1 net_failover
virtio_blk             20480  2
libahci                32768  1 ahci

> sudo lshw -c memory

  *-firmware
       description: BIOS
       vendor: SeaBIOS
       physical id: 0
       version: 1.13.0-1ubuntu1.1
       date: 04/01/2014
       size: 96KiB
  *-memory
       description: System Memory
       physical id: 1000
       size: 97GiB
       capabilities: ecc
       configuration: errordetection=multi-bit-ecc
     *-bank:0
          description: DIMM RAM
          vendor: QEMU
          physical id: 0
          slot: DIMM 0
          size: 16GiB
     *-bank:1
          description: DIMM RAM
          vendor: QEMU
          physical id: 1
          slot: DIMM 1
          size: 16GiB
     *-bank:2
          description: DIMM RAM
          vendor: QEMU
          physical id: 2
          slot: DIMM 2
          size: 16GiB
     *-bank:3
          description: DIMM RAM
          vendor: QEMU
          physical id: 3
          slot: DIMM 3
          size: 16GiB
     *-bank:4
          description: DIMM RAM
          vendor: QEMU
          physical id: 4
          slot: DIMM 4
          size: 16GiB
     *-bank:5
          description: DIMM RAM
          vendor: QEMU
          physical id: 5
          slot: DIMM 5
          size: 16GiB
     *-bank:6
          description: DIMM RAM
          vendor: QEMU
          physical id: 6
          slot: DIMM 6
          size: 1696MiB

