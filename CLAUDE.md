# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`cosmotop` is a cross-platform system monitoring tool built as a single executable using Cosmopolitan Libc. It's a fork of `btop++` that displays real-time system metrics (CPU, memory, disk, network, GPU, NPU) in a terminal interface. The project uses the Actually Portable Executable (APE) format to create binaries that run natively on multiple operating systems.

## Build System and Commands

### Core Build Commands

The project uses CMake with two distinct build targets that must be built separately:

```bash
# Build the multiplatform host executable (requires cosmocc toolchain)
export CC=cosmocc
export CXX=cosmoc++
cmake -B build-cosmotop -DTARGET=host
cmake --build build-cosmotop

# Build platform-native plugin binaries
cmake -B build-plugin -DTARGET=plugin
cmake --build build-plugin
```

### Available Build Targets

- `TARGET=host`: Builds the multiplatform `cosmotop.com` executable using Cosmopolitan Libc
- `TARGET=plugin`: Builds platform-specific plugin executables/DLLs for system data collection

### Testing and Validation

No automated test suite is present. Manual testing involves:
- Running the built executable on target platforms
- Verifying system metrics collection functionality
- Testing GPU/NPU detection and monitoring capabilities

## Architecture Overview

### Dual-Binary Architecture

The project uses a unique dual-binary architecture:

1. **Host Executable** (`cosmotop.com`): Multiplatform binary built with Cosmopolitan Libc that handles:
   - Terminal UI rendering and user interaction
   - Generic system metrics processing
   - Plugin management and communication
   - Configuration and theme management

2. **Platform Plugins**: Native executables/DLLs for each supported OS that handle:
   - OS-specific system data collection
   - Hardware-specific metrics (GPU, NPU, sensors)
   - Communication with host via plugin interface

### Key Components

- **Core UI**: `cosmotop_draw.cpp`, `cosmotop_input.cpp`, `cosmotop_menu.cpp`
- **Configuration**: `cosmotop_config.cpp` - handles settings and themes
- **Plugin System**: `cosmotop_plugin.cpp` - manages platform-specific data collection
- **MCP Server**: `cosmotop_mcp.cpp` - Model Context Protocol interface for AI tools
- **Platform Collectors**: `src/{linux,windows,osx,freebsd,etc}/cosmotop_collect.cpp`

### Third-Party Dependencies

- **libcosmo_plugin**: Custom plugin communication library
- **TinyMCP**: Model Context Protocol implementation
- **cpp-httplib**: HTTP client/server functionality
- **fmt**: String formatting library
- **range-v3**: Range algorithms (C++20 ranges on Windows)
- **rocm_smi_lib**: AMD GPU monitoring (Linux)
- **widecharwidth**: Unicode character width calculations

## Platform Support

### Core Platforms (Bundled Plugins)
- Linux x86_64/aarch64, Windows x86_64, macOS x86_64/aarch64

### Extended Platforms (Downloaded Plugins)
- FreeBSD, NetBSD, OpenBSD, DragonFlyBSD, Solaris
- Various architectures: i386, powerpc64le, s390x, riscv64, loongarch64

### Platform-Specific Notes

- **Linux**: Static linking for plugins, ROCm support for AMD GPUs
- **Windows**: Builds as DLL, requires LibreHardwareMonitor for GPU metrics
- **macOS**: Framework linking (CoreFoundation, IOKit), IOReport for Apple Silicon
- **BSD variants**: Require specific system libraries (kvm, devstat, etc.)

## Special Features

### GPU Monitoring
- **Linux**: Intel (requires root or intel-gpu-exporter), AMD (rocm_smi_lib), NVIDIA (libnvidia-ml.so)
- **Windows**: LibreHardwareMonitor integration
- **macOS**: Built-in GPU metrics support

### NPU Monitoring
- **Intel**: `/sys/devices/pci0000:00/0000:00:0b.0` access required
- **Rockchip**: `/sys/kernel/debug/rknpu` access required
- **Apple Neural Engine**: Built-in support on Apple Silicon

### MCP Mode
Exposes system monitoring tools via Model Context Protocol:
- `get_process_info`, `get_cpu_info`, `get_memory_info`
- `get_network_info`, `get_disk_info`, `get_gpu_info`, `get_npu_info`
- `get_container_info`, `get_system_info`

## Development Workflow

Sample commands included, assuming run on Linux and from the repo's root directory.
Note that bundling is required to run the host and plugin together.

1. **Host Development**: Use cosmocc toolchain on Linux for best results
   - `CXX=cosmoc++ CC=cosmocc cmake -B build-cosmotop`
   - `cmake --build build-cosmotop --parallel 4`
2. **Plugin Development**: Use native compilers for each target platform
   - `cmake -B build-plugin -DTARGET=plugin`
   - `cmake --build build-plugin --parallel 4`
3. **Bundling**: Combine host executable with plugins using `zip` command
   - `cp build-cosmotop/cosmotop.com .`
   - `cp build-plugin/cosmotop-plugin.exe cosmotop-linux-x86_64.exe`
   - `zip -r cosmotop.com cosmotop-linux-x86_64.exe themes/`
4. **Distribution**: Optional Chimp executable creation for extended platform support

## Configuration

- **Config Path**: `~/.config/cosmotop/cosmotop.conf`
- **Themes Path**: `~/.config/cosmotop/themes`
- **Plugin Cache**: `~/.cosmotop` (extracted/downloaded plugins)