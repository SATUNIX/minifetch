# minifetch
A C based fetch tool. Header based configuration. Supports Arch. 

## Install Options:
Different options to support use case. 
1. compile
2. install (compiles and moves to /usr/local/bin

### 1. Compile binary: 

```bash
bash ./install.sh
```

### 2. Install in one go:

```bash
chmod +x ./install.sh
sudo ./install.sh
```

---

## Config: 

See the header file, its pretty self explanitory in there. 

**V1:** 
- Logo is configured in the header file, NULL terminated, prints next to the system info. 

**V2:**
- Planning to have a second logo.txt embeds in binary for easier ascii arts.

## Example output:
```
[agrace@archlinux minifetch]$ ./minifetch 
                             OS: Arch Linux
                   ,____     Host: archlinux
                   |---.|    Kernel: 6.16.5-arch1-1
           ___     |    `    Uptime: 2h 4m
          / .-  ./=)         Packages: 689
         |  | |_//|          Shell: bash
         ;  |-;| /_|         Resolution: 
        / _| |/  |           WM: Hyprland
       /      /( |           CPU: Intel(R) Core(TM) i9-14900KF (32)
       |   /  |` ) |         GPU: Intel Corporation Battlemage G21 [Arc B580]
       /    _/    |          Memory: 7.45 GiB / 126 GiB
      /--._/      |          Disk (/): 10 GiB / 931 GiB (1%)
      `/|)    |    /         
        /     |   |          
      .'      |   |          
jgs  /           |           
    (_.-.__.__./  /          
                             
```
---
