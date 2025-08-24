# ![cosmotop](img/logo.svg)

`cosmotop` is a system monitoring tool distributed as a single executable for multiple platforms.
A fork of [`btop++`](https://github.com/aristocratos/btop) and built with
[Cosmopolitan Libc](https://github.com/jart/cosmopolitan).

## Installation

Download `cosmotop` from [GitHub releases](https://github.com/bjia56/cosmotop/releases/latest).
Place it anywhere and run!

### Homebrew

The Homebrew tap [`bjia56/tap`](https://github.com/bjia56/homebrew-tap) supports installing the latest `cosmotop` from GitHub releases on both MacOS and Linux.

```bash
brew tap bjia56/tap
brew install cosmotop
```

### Docker

A Docker image is available for Linux x86_64 and aarch64 hosts.

```bash
docker run -it --rm --net=host --pid=host ghcr.io/bjia56/cosmotop:latest
```

### Windows setup notes

On Windows, rename `cosmotop` to either `cosmotop.cmd` or `cosmotop.bat` before running. This allows Windows to execute the file as a batch script, which can then properly self-extract and execute the embedded executable.

## Usage and features

```
Usage: cosmotop [OPTIONS]

Options:
  -h,  --help          show this help message and exit
  -v,  --version       show version info and exit
  -lc, --low-color     disable truecolor, converts 24-bit colors to 256-color
  -t,  --tty_on        force (ON) tty mode, max 16 colors and tty friendly graph symbols
  +t,  --tty_off       force (OFF) tty mode
  -p,  --preset <id>   start with preset, integer value between 0-9
  -u,  --update <ms>   set the program update rate in milliseconds
  -o,  --option        override a configuration option in KEY=VALUE format, can use multiple times
       --show-defaults print default configuration values to stdout
       --show-themes   list all available themes
       --licenses      display licenses of open-source software used in cosmotop
       --debug         start in DEBUG mode: shows microsecond timer for information collect
                       and screen draw functions and sets loglevel to DEBUG
       --mcp           start MCP server mode: exposes system information tools via MCP protocol
```

### GPU monitoring

Monitoring of GPUs is supported on Linux and Windows.
- Windows: LibreHardwareMonitor is included with `cosmotop` and automatically used to fetch GPU information.
- Linux: Intel, AMD, and NVIDIA GPUs are supported, provided the appropriate driver is installed, and the following:
  - Intel: Root privileges are required to access metrics directly. Alternatively, run [intel-gpu-exporter](https://github.com/bjia56/intel-gpu-exporter) in a privileged Docker container, then set the `intel_gpu_exporter` configuration option in `cosmotop` to the exporter's HTTP endpoint.
  - AMD: `rocm_smi_lib` is statically linked and should work out of the box.
  - NVIDIA: `libnvidia-ml.so` must be available.

### NPU monitoring

Utilization monitoring of Intel and Rockchip NPUs is supported on Linux, provided the following:
- Intel: The path `/sys/devices/pci0000:00/0000:00:0b.0` must be readable.
- Rockchip: The path `/sys/kernel/debug/rknpu` must be readable.

Utilization monitoring of the Apple Neural Engine is supported on Apple Silicon. Sudo is not required.

### Container monitoring

Monitoring of running OCI containers is suppored on Linux, Windows, and MacOS using Docker-compatible APIs.
On Linux and MacOS, the standard locations of `/run/docker.sock` and `/var/run/docker.sock` are used by default,
but can be overriden with the `DOCKER_HOST` variable to point to an HTTP endpoint or a different socket (e.g.
`/var/run/podman/podman.sock` for Podman containers). Access to the socket must be over plaintext (i.e. no TLS).

When using tools like [linuxserver/socket-proxy](https://github.com/linuxserver/docker-socket-proxy) to secure
the Docker API, read-only access to the `/version` and `/containers` endpoints is required for monitoring to be
enabled.

### Configuration

The configuration file for `cosmotop` is stored at `~/.config/cosmotop/cosmotop.conf`, populated with defaults
the first time the program runs.

### Themes

A number of themes are available within `cosmotop`. Place custom themes at `~/.config/cosmotop/themes`.

### MCP mode

Model Context Protocol (MCP) mode is enabled with the `--mcp` flag and starts `cosmotop` as a MCP server using the STDIO transport
and protocol version `2024-11-05`. Normal graphical metrics reporting is disabled during this mode. Tools exposed by `cosmotop`:
- `get_process_info`
- `get_cpu_info`
- `get_memory_info`
- `get_network_info`
- `get_disk_info`
- `get_gpu_info` (not available if no GPUs are detected)
- `get_npu_info` (not available if no NPUs are detected)
- `get_container_info` (not available if no container engines are detected)
- `get_system_info`

## Supported platforms

`cosmotop` supports the following operating systems and architectures:

| Operating system | Hardware architecture |
|-|-|
| Linux 2.6.18+ | x86_64, i386, aarch64, powerpc64le, s390x, riscv64, loongarch64 |
| Windows 10+ | x86_64 |
| MacOS 13+ | x86_64, aarch64 |
| MacOS 10.4+ | powerpc |
| FreeBSD 13+ | x86_64, aarch64 |
| NetBSD 10.0+ | x86_64, aarch64 |
| OpenBSD 7.6+ | x86_64, aarch64 |
| DragonFlyBSD 6.4+ | x86_64 |
| MidnightBSD 3.2.3+ | x86_64 |
| Solaris 11.4+ | x86_64 |
| Haiku R1/beta5+ | x86_64 |

Core platforms (Linux x86_64/aarch64, MacOS, Windows) are self-contained and require no additional tooling.
Other platforms require that the host `PATH` contains either `curl`, `wget`, or `python3` to download required plugin components (see [below](#how-it-works)).

## How it works

`cosmotop` uses [Cosmopolitan Libc](https://github.com/jart/cosmopolitan) and the
[Actually Portable Executable](https://justine.lol/ape.html) (APE) and [Chimp](https://github.com/bjia56/chimp) file formats to create a single executable capable of
running on multiple operating systems and architectures. This multiplatform executable contains code to draw
the terminal UI and handle generic systems metrics, like processes, memory, disk, etc. At runtime, the APE executable is extracted out to disk before execution. On Windows, the APE
runs natively. On UNIX, a small loader binary is additionally extracted to run the APE executable.

Collecting real data from the underlying system is done by helper [plugins](https://github.com/bjia56/libcosmo_plugin), which are built for each target platform using host-native compilers and libraries. On core platforms (see [above](#supported-platforms)), plugins are bundled into `cosmotop` and extracted out onto the host under the path `~/.cosmotop`. On other platforms, plugins are downloaded from GitHub releases from the same release tag as `cosmotop` and placed under `~/.cosmotop`, and are optionally re-bundled into the executable. Plugins are used at runtime to gather system metrics that are then displayed by the primary multiplatform executable process in the terminal.

For platforms not supported natively by Cosmopolitan Libc, `cosmotop` uses the [Blink](https://github.com/jart/blink) lightweight virtual machine
to run the x86_64 version of `cosmotop`. Data collection is still done by host-native plugin executables.

## Building from source

`cosmotop` is built with CMake. Both the multiplatform host executable and platform-native plugins can be built with the CMakeLists.txt at the root of this repository, but they *must be built with separate CMake invocations* due to the usage of different compilers.

### Building the multiplatform "host" executable

Download the `cosmocc` toolchain (`cosmocc-X.Y.Z.zip`) from the Cosmopolitan [GitHub releases](https://github.com/jart/cosmopolitan/releases/latest) and extract it somewhere on your filesystem. Add the `bin` directory to `PATH` to ensure the compilers can be found. For best results, compile this part on Linux.

On Linux, it may be needed to [modify `binfmt_misc`](https://github.com/jart/cosmopolitan?tab=readme-ov-file#linux) to run the `cosmocc` toolchain.

```bash
export CC=cosmocc
export CXX=cosmoc++
cmake -B build-host -DTARGET=host
cmake --build build
# or: cmake --build build --parallel
```

This should produce a `cosmotop.com` binary.

### Building platform-native "plugin" binaries

Platform-native plugins are built as executables on all supported platforms except Windows, which builds as a DLL.
To tell CMake to build plugins, use `-DTARGET=plugin`.

```bash
cmake -B build-plugin -DTARGET=plugin
cmake --build build
# or: cmake --build build --parallel
```

This should produce a `cosmotop-plugin.exe` (or `cosmotop-plugin.dll` on Windows). Rename it to `cosmotop-<kernel>-<arch>.[exe|dll]` matching the target platform, for example:

```
cosmotop-linux-x86_64.exe
cosmotop-linux-aarch64.exe
cosmotop-macos-x86_64.exe
cosmotop-macos-aarch64.exe
cosmotop-windows-x86_64.dll
cosmotop-freebsd-x86_64.exe
cosmotop-freebsd-aarch64.exe
cosmotop-dragonflybsd-x86_64.exe
cosmotop-netbsd-x86_64.exe
cosmotop-netbsd-aarch64.exe
cosmotop-openbsd-x86_64.exe
cosmotop-openbsd-aarch64.exe
cosmotop-haiku-x86_64.exe
```

### Bundling everything together

The plugin binaries can be added to `cosmotop.com` with `zip`, for example:

```bash
zip cosmotop.com cosmotop-linux-x86_64.exe
```

Themes can also be bundled:

```bash
zip -r cosmotop.com themes/
```

Optionally, rename `cosmotop.com` to `cosmotop.exe`.

### Optional: Producing a Chimp executable

Download `chimplink` from the Chimp [GitHub releases](https://github.com/bjia56/chimp/releases/latest) and add it to your `PATH`.

Build Blink VMs for any platforms not natively supported by Cosmopolitan Libc. Prebuilts for a variety of platforms are available from Blinkverse [GitHub releases](https://github.com/bjia56/blinkverse/releases).

Use `chimplink` to bundle `cosmotop.exe` with your selection of loaders, for example:

```bash
cosmo_bin=$(dirname $(which cosmocc))
chimplink cosmotop.exe cosmotop some_string_here \
  ${cosmo_bin}/ape-x86_64.elf \
  ${cosmo_bin}/ape-aarch64.elf \
  --os Linux blink-linux-* \
  --os NetBSD blink-netbsd-* \
  --os OpenBSD blink-openbsd-*
```

For optimal Chimp startup performance, instead of using `cosmotop.com` after the host build above, use `apelink` to produce a version that has a special embedded string:

```bash
cosmo_bin=$(dirname $(which cosmocc))
apelink \
  -S "V=some_string_here" \
  -l ${cosmo_bin}/ape-x86_64.elf \
  -M ${cosmo_bin}/ape-m1.c \
  -o cosmotop.com \
  build/cosmotop.com.dbg \
  build/cosmotop.aarch64.elf
```

The final Chimp executable will check if the string matches before overwriting the extracted file on disk. A good choice for this string is a git SHA for uniqueness.

## Licensing

Unless otherwise stated, the code in this repository is licensed under Apache-2.0.

For the most up to date list of licenses included in `cosmotop` release builds, run `cosmotop.exe --licenses`.
