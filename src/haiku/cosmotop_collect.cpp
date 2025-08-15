/* Copyright 2025 Brett Jia (dev.bjia56@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

indent = tab
tab-size = 4
*/

#include <cosmo_plugin.hpp>

// Haiku-specific headers
#include <OS.h>
#include <Drivers.h>
#include <NetworkInterface.h>
#include <NetworkRoster.h>
#include <NetworkDevice.h>
#include <NetworkAddress.h>
#include <Path.h>
#include <FindDirectory.h>
#include <Directory.h>
#include <Entry.h>
#include <fs_info.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <be/kernel/fs_info.h>
#include <image.h>
#include <be/kernel/OS.h>
#include <be/drivers/Drivers.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>

#include <stdexcept>
#include <cmath>
#include <fstream>
#include <numeric>
#include <regex>
#include <string>
#include <memory>
#include <utility>

#include <range/v3/all.hpp>

#include "../cosmotop_config.hpp"
#include "../cosmotop_shared.hpp"
#include "../cosmotop_tools.hpp"

using std::clamp, std::string_literals::operator""s, std::cmp_equal, std::cmp_less, std::cmp_greater;
using std::ifstream, std::numeric_limits, std::streamsize, std::round, std::max, std::min;
namespace fs = std::filesystem;
namespace rng = ranges;
using namespace Tools;

namespace Shared {
	uint64_t totalMem = 0;
	uint64_t pageSize = 0;
}

namespace Cpu {
	vector<long long> core_old_totals;
	vector<long long> core_old_idles;
	vector<string> available_fields = {"Auto", "total"};
	vector<string> available_sensors = {"Auto"};
	cpu_info current_cpu;
	bool got_sensors = false, cpu_temp_only = false;

	string cpuName;
	string cpuHz;
	bool has_battery = false;
	tuple<int, float, long, string> current_bat;

	const array<string, 4> time_names = {"user", "nice", "system", "idle"};

	std::unordered_map<string, long long> cpu_old = {
		{"totals", 0},
		{"idles", 0},
		{"user", 0},
		{"nice", 0},
		{"system", 0},
		{"idle", 0}
	};

	vector<string> core_sensors;
	std::unordered_map<int, int> core_mapping;

	bool get_sensors() {
		// Does Haiku have temperature sensors?
		return false;
	}

	string get_cpuName() {
		string name = "Unknown";
		string vendorName = "";
		cpu_platform platform = B_CPU_UNKNOWN;

		// Get CPU topology information for platform and vendor details
		uint32 count = 0;

		if (get_cpu_topology_info(nullptr, &count) == B_OK && count > 0) {
			auto topologyInfo = std::unique_ptr<cpu_topology_node_info[]>(new cpu_topology_node_info[count]);
			if (get_cpu_topology_info(topologyInfo.get(), &count) == B_OK) {
				// Look for package level topology info which contains platform and vendor details
				for (uint32 i = 0; i < count; i++) {
					if (topologyInfo[i].type == B_TOPOLOGY_PACKAGE) {
						switch (topologyInfo[i].data.package.vendor) {
							case B_CPU_VENDOR_INTEL: vendorName = "Intel"; break;
							case B_CPU_VENDOR_AMD: vendorName = "AMD"; break;
							case B_CPU_VENDOR_VIA: vendorName = "VIA"; break;
							case B_CPU_VENDOR_CYRIX: vendorName = "Cyrix"; break;
							case B_CPU_VENDOR_RISE: vendorName = "Rise"; break;
							case B_CPU_VENDOR_TRANSMETA: vendorName = "Transmeta"; break;
							case B_CPU_VENDOR_IDT: vendorName = "IDT"; break;
							case B_CPU_VENDOR_NATIONAL_SEMICONDUCTOR: vendorName = "National Semiconductor"; break;
							case B_CPU_VENDOR_IBM: vendorName = "IBM"; break;
							case B_CPU_VENDOR_MOTOROLA: vendorName = "Motorola"; break;
							case B_CPU_VENDOR_NEC: vendorName = "NEC"; break;
							case B_CPU_VENDOR_HYGON: vendorName = "Hygon"; break;
							case B_CPU_VENDOR_SUN: vendorName = "Sun"; break;
							case B_CPU_VENDOR_FUJITSU: vendorName = "Fujitsu"; break;
							case B_CPU_VENDOR_UNKNOWN:
							default: break;
						}
					} else if (topologyInfo[i].type == B_TOPOLOGY_ROOT) {
						platform = topologyInfo[i].data.root.platform;
					}
				}
			}
		}

		// Build CPU name based on platform and vendor
		switch (platform) {
			case B_CPU_x86:
				name = vendorName.empty() ? "x86" : vendorName + " x86";
				break;
			case B_CPU_x86_64:
				name = vendorName.empty() ? "x86_64" : vendorName + " x86_64";
				break;
			case B_CPU_PPC:
				name = "PowerPC";
				break;
			case B_CPU_PPC_64:
				name = "PowerPC 64";
				break;
			case B_CPU_M68K:
				name = "M68K";
				break;
			case B_CPU_ARM:
				name = "ARM";
				break;
			case B_CPU_ARM_64:
				name = "ARM64";
				break;
			case B_CPU_ALPHA:
				name = "Alpha";
				break;
			case B_CPU_MIPS:
				name = "MIPS";
				break;
			case B_CPU_SH:
				name = "SuperH";
				break;
			case B_CPU_SPARC:
				name = "SPARC";
				break;
			case B_CPU_RISC_V:
				name = "RISC-V";
				break;
			default:
				name = vendorName.empty() ? "Unknown" : vendorName;
				break;
		}

		// For x86/x86_64 platforms, try to get more detailed CPU info via CPUID
		if ((platform == B_CPU_x86 || platform == B_CPU_x86_64) && !vendorName.empty()) {
			cpuid_info cpuidInfo;
			if (get_cpuid(&cpuidInfo, 0, 0) == B_OK) {
				// Extract brand string from CPUID if available
				char brandString[49] = {0};
				uint32* brand = reinterpret_cast<uint32*>(brandString);

				// Try to get extended brand string (CPUID functions 0x80000002-0x80000004)
				if (get_cpuid(&cpuidInfo, 0x80000002, 0) == B_OK) {
					brand[0] = cpuidInfo.regs.eax;
					brand[1] = cpuidInfo.regs.ebx;
					brand[2] = cpuidInfo.regs.ecx;
					brand[3] = cpuidInfo.regs.edx;

					if (get_cpuid(&cpuidInfo, 0x80000003, 0) == B_OK) {
						brand[4] = cpuidInfo.regs.eax;
						brand[5] = cpuidInfo.regs.ebx;
						brand[6] = cpuidInfo.regs.ecx;
						brand[7] = cpuidInfo.regs.edx;

						if (get_cpuid(&cpuidInfo, 0x80000004, 0) == B_OK) {
							brand[8] = cpuidInfo.regs.eax;
							brand[9] = cpuidInfo.regs.ebx;
							brand[10] = cpuidInfo.regs.ecx;
							brand[11] = cpuidInfo.regs.edx;

							// Clean up the brand string and use it if non-empty
							string fullBrand(brandString);
							fullBrand.erase(0, fullBrand.find_first_not_of(" \t"));
							fullBrand.erase(fullBrand.find_last_not_of(" \t") + 1);

							if (!fullBrand.empty()) {
								name = fullBrand;
							}
						}
					}
				}
			}
		}

		auto name_vec = ssplit(name, ' ');

		if ((s_contains(name, "Xeon"s) or v_contains(name_vec, "Duo"s)) and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')'))
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		}
		else if (v_contains(name_vec, "Ryzen"s)) {
			auto ryz_pos = v_index(name_vec, "Ryzen"s);
			name = "Ryzen"	+ (ryz_pos < name_vec.size() - 1 ? ' ' + name_vec.at(ryz_pos + 1) : "")
							+ (ryz_pos < name_vec.size() - 2 ? ' ' + name_vec.at(ryz_pos + 2) : "");
		}
		else if (s_contains(name, "Intel"s) and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')') and name_vec.at(cpu_pos + 1).size() != 1)
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		}
		else
			name.clear();

		if (name.empty() and not name_vec.empty()) {
			for (const auto& n : name_vec) {
				if (n == "@") break;
				name += n + ' ';
			}
			name.pop_back();
			for (const auto& replace : {"Processor", "CPU", "(R)", "(TM)", "Intel", "AMD", "Core"}) {
				name = s_replace(name, replace, "");
				name = s_replace(name, "  ", " ");
			}
			name = trim(name);
		}

		return name;
	}

	string get_cpuHz() {
		uint32 count = 0;

		if (get_cpu_topology_info(nullptr, &count) == B_OK && count > 0) {
			auto topologyInfo = std::unique_ptr<cpu_topology_node_info[]>(new cpu_topology_node_info[count]);
			if (get_cpu_topology_info(topologyInfo.get(), &count) == B_OK) {
				for (uint32 i = 0; i < count; i++) {
					if (topologyInfo[i].type == B_TOPOLOGY_CORE) {
						auto hz = topologyInfo[i].data.core.default_frequency;
						if (hz > 0) {
							return std::to_string(hz / 1000000) + " MHz";
						} else {
							return "";
						}
					}
				}
			}
		}
		return "";
	}

	void update_sensors() {}

	auto get_core_mapping() -> std::unordered_map<int, int> {
		// In Haiku, logical CPU IDs are already sequential from 0
		std::unordered_map<int, int> core_map;
		for (int i = 0; i < Shared::coreCount; i++) {
			core_map[i] = i;
		}
		return core_map;
	}

	auto collect(bool no_update) -> cpu_info & {
		if (no_update && !current_cpu.cpu_percent.at("total").empty())
			return current_cpu;

		const auto width = get_width();
		const auto time_since_boot_ms = system_time();

		// Per cpu percentages
		auto infos = std::unique_ptr<::cpu_info[]>(new ::cpu_info[Shared::coreCount]);
		uint64_t total = 0;
		static bigtime_t last_update = 0;
		static vector<uint64_t> last_active_times(Shared::coreCount, 0);
		if (get_cpu_info(0, Shared::coreCount, infos.get()) == B_OK) {
			for (int i = 0; i < Shared::coreCount; i++) {
				if (last_update == 0) {
					// First collection, just store old values
					last_active_times[i] = infos[i].active_time;
					continue;
				}

				auto active = infos[i].active_time;
				auto previous = last_active_times[i];
				auto percent = clamp((long long)round((double)(active - previous) * 100 / (time_since_boot_ms - last_update)), 0ll, 100ll);

				current_cpu.core_percent.push_back({});
				current_cpu.core_percent[i].push_back(percent);
				while (cmp_greater(current_cpu.core_percent[i].size(), width * 2))
					current_cpu.core_percent[i].pop_front();
			}
		} else {
			Logger::error("Failed to get CPU info");
		}

		// Global percentages
		const uint64_t total_active = std::accumulate(current_cpu.core_percent.begin(), current_cpu.core_percent.end(), 0ll,
			[](long long sum, const deque<long long>& core) { return sum + (core.empty() ? 0 : core.back()); });
		const uint64_t last_total_active = std::accumulate(last_active_times.begin(), last_active_times.end(), 0ll);
		current_cpu.cpu_percent.at("total").push_back(
			clamp((long long)round((double)(total_active - last_total_active) * 100 / (time_since_boot_ms - last_update)), 0ll, 100ll));
		while (cmp_greater(current_cpu.cpu_percent.at("total").size(), width * 2))
			current_cpu.cpu_percent.at("total").pop_front();

		last_update = time_since_boot_ms;
		for (int i = 0; i < Shared::coreCount; i++) {
			last_active_times[i] = infos[i].active_time;
		}

		if (Config::getB("show_cpu_freq")) {
			auto hz = get_cpuHz();
			if (hz != "") {
				cpuHz = hz;
			}
		}

		return current_cpu;
	}
}

namespace Mem {
	double old_uptime;
	int disk_ios = 0;
	vector<string> last_found;
	bool has_swap = false;
	mem_info current_mem;

	uint64_t get_totalMem() {
		return Shared::totalMem;
	}

	void assign_values(struct disk_info& disk, int64_t readBytes, int64_t writeBytes) {
		const auto width = get_width();
		disk_ios++;
		if (disk.io_read.empty()) {
			disk.io_read.push_back(0);
		} else {
			disk.io_read.push_back(max((int64_t)0, (readBytes - disk.old_io.at(0))));
		}
		disk.old_io.at(0) = readBytes;
		while (cmp_greater(disk.io_read.size(), width * 2)) disk.io_read.pop_front();

		if (disk.io_write.empty()) {
			disk.io_write.push_back(0);
		} else {
			disk.io_write.push_back(max((int64_t)0, (writeBytes - disk.old_io.at(1))));
		}
		disk.old_io.at(1) = writeBytes;
		while (cmp_greater(disk.io_write.size(), width * 2)) disk.io_write.pop_front();

		// Activity is a combination of read and write
		if (disk.io_activity.empty())
			disk.io_activity.push_back(0);
		else
			disk.io_activity.push_back(clamp((long)round((double)(disk.io_write.back() + disk.io_read.back()) / (1 << 20)), 0l, 100l));
		while (cmp_greater(disk.io_activity.size(), width * 2)) disk.io_activity.pop_front();
	}

	void collect_disk(std::unordered_map<string, disk_info> &disks,
	                  std::unordered_map<string, string> &mapping) {
		// Get volume information
		BVolumeRoster volumeRoster;
		BVolume volume;

		// Create a map of devices to populate
		std::unordered_map<string, disk_info> temp_disks;
		vector<string> temp_order;

		while (volumeRoster.GetNextVolume(&volume) == B_OK) {
			char name[B_FILE_NAME_LENGTH];
			char device[B_PATH_NAME_LENGTH];

			// Skip volumes we can't get info from
			if (volume.GetName(name) != B_OK || strlen(name) == 0)
				continue;

			fs_info info;
			if (fs_stat_dev(volume.Device(), &info) != B_OK)
				continue;

			string fsname = name;
			string fstype = info.fsh_name;

			// Create a disk_info entry
			disk_info di;
			di.name = fsname;
			di.fstype = fstype;
			di.dev = string(info.device_name);
			di.total = volume.Capacity();
			di.free = volume.FreeBytes();
			di.used = di.total - di.free;
			di.used_percent = (di.total > 0) ? static_cast<int>((static_cast<double>(di.used) / di.total) * 100) : 0;
			di.free_percent = 100 - di.used_percent;

			// Add to our temporary collections
			string device_path = string(info.device_name);
			temp_disks[device_path] = di;
			temp_order.push_back(device_path);

			// Add to mapping
			mapping[device_path] = fsname;
		}

		// Update disks with I/O statistics
		for (const auto& device : temp_order) {
			// See if this is a disk we already track
			if (disks.contains(device)) {
				// Update existing entry but keep I/O stats
				auto& existing = disks.at(device);
				auto& new_disk = temp_disks.at(device);

				new_disk.old_io = existing.old_io;
				new_disk.io_read = existing.io_read;
				new_disk.io_write = existing.io_write;
				new_disk.io_activity = existing.io_activity;

				// For Haiku, we don't have good real-time disk I/O stats
				// so we'll just use 0 values
				assign_values(new_disk, new_disk.old_io.at(0), new_disk.old_io.at(1));

				disks[device] = new_disk;
			} else {
				// New disk entry, initialize with zeros
				assign_values(temp_disks.at(device), 0, 0);
				disks[device] = temp_disks.at(device);
			}
		}

		// Update the disk order
		last_found = temp_order;
	}

	auto collect(bool no_update) -> mem_info & {
		if (no_update && !current_mem.percent.at("used").empty())
			return current_mem;

		const auto show_swap = Config::getB("show_swap");
		const auto show_disks = Config::getB("show_disks");
		const auto swap_disk = Config::getB("swap_disk");
		const auto width = get_width();

		// Get memory stats
		system_info sysInfo;
		if (get_system_info(&sysInfo) == B_OK) {
			uint64_t pageSize = Shared::pageSize;
			uint64_t totalPages = sysInfo.max_pages;
			uint64_t freePages = sysInfo.max_pages - sysInfo.used_pages;
			uint64_t cachedPages = sysInfo.cached_pages;

			uint64_t totalMem = totalPages * pageSize;
			uint64_t freeMem = freePages * pageSize;
			uint64_t cachedMem = cachedPages * pageSize;
			uint64_t usedMem = totalMem - freeMem - cachedMem;

			current_mem.stats["free"] = freeMem;
			current_mem.stats["cached"] = cachedMem;
			current_mem.stats["available"] = freeMem + cachedMem;
			current_mem.stats["used"] = usedMem;

			// Get swap info if available
			if (show_swap) {
				uint64_t maxSwap = sysInfo.max_swap_pages * pageSize;
				uint64_t freeSwap = sysInfo.free_swap_pages * pageSize;
				uint64_t usedSwap = maxSwap - freeSwap;

				has_swap = (maxSwap > 0);
				if (has_swap) {
					current_mem.stats["swap_total"] = maxSwap;
					current_mem.stats["swap_used"] = usedSwap;
					current_mem.stats["swap_free"] = freeSwap;
				} else {
					current_mem.stats["swap_total"] = 0;
					current_mem.stats["swap_used"] = 0;
					current_mem.stats["swap_free"] = 0;
				}
			}
		}

		// Update percentage values
		for (const auto& name : mem_names) {
			current_mem.percent.at(name).push_back(
				clamp((long long)round((double)current_mem.stats.at(name) * 100 / Shared::totalMem), 0ll, 100ll));
			while (cmp_greater(current_mem.percent.at(name).size(), width * 2))
				current_mem.percent.at(name).pop_front();
		}

		// Update swap percentages if swap is available
		if (has_swap && show_swap && current_mem.stats.at("swap_total") > 0) {
			const uint64_t totalSwap = current_mem.stats.at("swap_total");
			for (const auto& name : swap_names) {
				current_mem.percent.at(name).push_back(
					clamp((long long)round((double)current_mem.stats.at(name) * 100 / totalSwap), 0ll, 100ll));
				while (cmp_greater(current_mem.percent.at(name).size(), width * 2))
					current_mem.percent.at(name).pop_front();
			}
		}

		// Collect disk information if enabled
		if (show_disks) {
			std::unordered_map<string, string> mapping;
			collect_disk(current_mem.disks, mapping);
			current_mem.disks_order = last_found;
		}

		return current_mem;
	}
}

namespace Net {
	std::unordered_map<string, net_info> current_net;
	net_info empty_net = {};
	vector<string> interfaces;
	string selected_iface;
	int errors = 0;
	std::unordered_map<string, uint64_t> graph_max = {{"download", {}}, {"upload", {}}};
	std::unordered_map<string, array<int, 2>> max_count = {{"download", {}}, {"upload", {}}};
	bool rescale = true;
	uint64_t timestamp = 0;

	void init() {
		// Find all network interfaces
		BNetworkRoster& roster = BNetworkRoster::Default();
		BNetworkInterface interface;
		uint32_t cookie = 0;

		while (roster.GetNextInterface(&cookie, interface) == B_OK) {
			const char* name = interface.Name();
			if (name && strlen(name) > 0) {
				// Add interface to current_net if it doesn't exist
				if (!current_net.contains(name)) {
					net_info net;
					net.stat["download"] = {};
					net.stat["upload"] = {};
					current_net[name] = net;
				}
			}
		}
	}

	auto collect(bool no_update) -> net_info & {
		return empty_net;
	}
}

namespace Proc {

	vector<proc_info> current_procs;
	std::unordered_map<string, string> uid_user;
	string current_sort;
	string current_filter;
	bool current_rev = false;

	fs::file_time_type passwd_time;

	uint64_t cputimes;
	int collapse = -1, expand = -1;
	uint64_t old_cputimes = 0;
	atomic<int> numpids = 0;
	int filter_found = 0;

	detail_container detailed;

	auto collect(bool no_update) -> vector<proc_info>& {
		return current_procs;
	}
}

namespace Shared {
	long clkTck, coreCount, bootTime;

	void init() {
		// Get system info
		system_info sysInfo;
		if (get_system_info(&sysInfo) != B_OK) {
			Logger::error("Failed to get system info");
			return;
		}

		// Get CPU core count
		coreCount = sysInfo.cpu_count;

		// Get page size
		pageSize = B_PAGE_SIZE;

		// Get clock ticks per second (Haiku uses microseconds)
		clkTck = 1000000;

		// Get total memory
		totalMem = sysInfo.max_pages * pageSize;

		// Get boot time
		bootTime = (system_time() - sysInfo.boot_time) / 1000000;

		// Initialize CPU structures
		Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), coreCount, {});
		Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), coreCount, 0);
		Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), coreCount, 0);

		// Initialize CPU
		Cpu::collect();
		Cpu::cpuName = Cpu::get_cpuName();
		Cpu::got_sensors = Cpu::get_sensors();

		// Initialize Mem
		Mem::old_uptime = system_uptime();
		Mem::collect();

		// Initialize Net
		Net::init();
	}
}

namespace Tools {
	double system_uptime() {
		auto time_since_boot_ms = system_time();
		return time_since_boot_ms / 1000000.0; // Convert to seconds
	}
}
