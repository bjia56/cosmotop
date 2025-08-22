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

		return simplify_cpu_name(name);
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
		static uint64_t last_total = 0;
		static vector<uint64_t> last_active_times(Shared::coreCount, 0);
		if (get_cpu_info(0, Shared::coreCount, infos.get()) == B_OK) {
			for (int i = 0; i < Shared::coreCount; i++) {
				total += infos[i].active_time;
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

		if (last_update != 0) {
			// Global percentages
			current_cpu.cpu_percent.at("total").push_back(
				clamp((long long)round((double)((total - last_total) * 100 / (time_since_boot_ms - last_update)) / Shared::coreCount), 0ll, 100ll));
			while (cmp_greater(current_cpu.cpu_percent.at("total").size(), width * 2))
				current_cpu.cpu_percent.at("total").pop_front();

			if (Config::getB("show_cpu_freq")) {
				auto hz = get_cpuHz();
				if (hz != "") {
					cpuHz = hz;
				}
			}
		}

		last_update = time_since_boot_ms;
		last_total = total;
		for (int i = 0; i < Shared::coreCount; i++) {
			last_active_times[i] = infos[i].active_time;
		}

		return current_cpu;
	}
}

namespace Mem {
	double old_uptime;
	int disk_ios = 0;
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

	vector<string> collect_disk(std::unordered_map<string, disk_info> &disks) {
		// Get volume information
		BVolumeRoster volumeRoster;
		BVolume volume;

		// Create a map of devices to populate
		std::unordered_map<string, disk_info> temp_disks;
		vector<string> order;

		while (volumeRoster.GetNextVolume(&volume) == B_OK) {
			char name[B_FILE_NAME_LENGTH];

			// Skip volumes we can't get info from
			if (volume.GetName(name) != B_OK || strlen(name) == 0)
				continue;

			fs_info info;
			if (fs_stat_dev(volume.Device(), &info) != B_OK)
				continue;

			string fstype = info.fsh_name;
			if (is_in(fstype, "ramfs", "devfs", "packagefs", "rootfs"))
				continue;

			// Create a disk_info entry
			disk_info di;
			di.name = string(name);
			di.fstype = fstype;
			di.dev = string(info.device_name);
			di.total = volume.Capacity();
			di.free = volume.FreeBytes();
			di.used = di.total - di.free;
			di.used_percent = clamp((long long)round((double)di.used * 100 / di.total), 0ll, 100ll);
			di.free_percent = 100 - di.used_percent;

			// Add to our temporary collections
			string device_path = string(info.device_name);
			temp_disks[device_path] = di;
			order.push_back(device_path);
		}

		// Update disks with I/O statistics
		for (const auto& device : order) {
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

		return order;
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
			uint64_t usedPages = sysInfo.used_pages;
			uint64_t cachedPages = sysInfo.cached_pages;

			uint64_t totalMem = totalPages * pageSize;
			uint64_t usedMem = usedPages * pageSize;
			uint64_t cachedMem = cachedPages * pageSize;
			uint64_t freeMem = totalMem - usedMem - cachedMem;

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
			for (const auto& name : swap_names) {
				current_mem.percent.at(name).push_back(
					clamp((long long)round((double)current_mem.stats.at(name) * 100 / Shared::totalMem), 0ll, 100ll));
				while (cmp_greater(current_mem.percent.at(name).size(), width * 2))
					current_mem.percent.at(name).pop_front();
			}
		}

		// Collect disk information if enabled
		if (show_disks) {
			current_mem.disks_order = collect_disk(current_mem.disks);
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

	auto collect(bool no_update) -> net_info & {
		auto& net = current_net;
		const auto config_iface = Config::getS("net_iface");
		const auto net_sync = Config::getB("net_sync");
		const auto net_auto = Config::getB("net_auto");
		auto new_timestamp = time_ms();
		const auto width = get_width();

		if (not no_update and errors < 3) {
			//? Get interface list using getifaddrs() wrapper
			IfAddrsPtr if_addrs {};
			if (if_addrs.get_status() != 0) {
				errors++;
				Logger::error("Net::collect() -> getifaddrs() failed with id " + to_string(if_addrs.get_status()));
				set_redraw(true);
				return empty_net;
			}
			int family = 0;
			static_assert(INET6_ADDRSTRLEN >= INET_ADDRSTRLEN); // 46 >= 16, compile-time assurance.
			enum { IPBUFFER_MAXSIZE = INET6_ADDRSTRLEN }; // manually using the known biggest value, guarded by the above static_assert
			char ip[IPBUFFER_MAXSIZE];
			interfaces.clear();
			string ipv4, ipv6;

			//? Iteration over all items in getifaddrs() list
			for (auto* ifa = if_addrs.get(); ifa != nullptr; ifa = ifa->ifa_next) {
				if (ifa->ifa_addr == nullptr) continue;
				family = ifa->ifa_addr->sa_family;
				const auto& iface = ifa->ifa_name;

				//? Update available interfaces vector and get status of interface
				if (not v_contains(interfaces, iface)) {
					interfaces.push_back(iface);
					net[iface].connected = (ifa->ifa_flags & IFF_UP);

					// An interface can have more than one IP of the same family associated with it,
					// but we pick only the first one to show in the NET box.
					// Note: Interfaces without any IPv4 and IPv6 set are still valid and monitorable!
					net[iface].ipv4.clear();
					net[iface].ipv6.clear();
				}


				//? Get IPv4 address
				if (family == AF_INET) {
					if (net[iface].ipv4.empty()) {
						if (nullptr != inet_ntop(family, &(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr), ip, IPBUFFER_MAXSIZE)) {
							net[iface].ipv4 = ip;
						} else {
							int errsv = errno;
							Logger::error("Net::collect() -> Failed to convert IPv4 to string for iface " + string(iface) + ", errno: " + strerror(errsv));
						}
					}
				}
				//? Get IPv6 address
				else if (family == AF_INET6) {
					if (net[iface].ipv6.empty()) {
						if (nullptr != inet_ntop(family, &(reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr)->sin6_addr), ip, IPBUFFER_MAXSIZE)) {
							net[iface].ipv6 = ip;
						} else {
							int errsv = errno;
							Logger::error("Net::collect() -> Failed to convert IPv6 to string for iface " + string(iface) + ", errno: " + strerror(errsv));
						}
					}
				} //else, ignoring family==AF_PACKET (see man 3 getifaddrs) which is the first one in the `for` loop.
			}

			//? Get total received and transmitted bytes
			for (const auto& iface : interfaces) {
				for (const string dir : {"download", "upload"}) {
					auto& saved_stat = net.at(iface).stat.at(dir);
					auto& bandwidth = net.at(iface).bandwidth.at(dir);
					uint64_t val{};

					//? Search for metrics in BNetworkRoster
					BNetworkRoster &net_roster = BNetworkRoster::Default();
					BNetworkInterface iface_info;
					uint32 cookie = 0;
					while (net_roster.GetNextInterface(&cookie, iface_info) == B_OK) {
						if (iface == iface_info.Name()) {
							ifreq_stats stats;
							if (iface_info.GetStats(stats) == B_OK) {
								val = (dir == "download") ? stats.receive.bytes : stats.send.bytes;
								break;
							}
						}
					}

					//? Update speed, total and top values
					if (val < saved_stat.last) {
						saved_stat.rollover += saved_stat.last;
						saved_stat.last = 0;
					}
					if (cmp_greater((unsigned long long)saved_stat.rollover + (unsigned long long)val, numeric_limits<uint64_t>::max())) {
						saved_stat.rollover = 0;
						saved_stat.last = 0;
					}
					saved_stat.speed = round((double)(val - saved_stat.last) / ((double)(new_timestamp - timestamp) / 1000));
					if (saved_stat.speed > saved_stat.top) saved_stat.top = saved_stat.speed;
					if (saved_stat.offset > val + saved_stat.rollover) saved_stat.offset = 0;
					saved_stat.total = (val + saved_stat.rollover) - saved_stat.offset;
					saved_stat.last = val;

					//? Add values to graph
					bandwidth.push_back(saved_stat.speed);
					while (cmp_greater(bandwidth.size(), width * 2)) bandwidth.pop_front();

					//? Set counters for auto scaling
					if (net_auto and selected_iface == iface) {
						if (net_sync and saved_stat.speed < net.at(iface).stat.at(dir == "download" ? "upload" : "download").speed) continue;
						if (saved_stat.speed > graph_max[dir]) {
							++max_count[dir][0];
							if (max_count[dir][1] > 0) --max_count[dir][1];
						}
						else if (graph_max[dir] > 10 << 10 and saved_stat.speed < graph_max[dir] / 10) {
							++max_count[dir][1];
							if (max_count[dir][0] > 0) --max_count[dir][0];
						}

					}
				}
			}

			//? Clean up net map if needed
			if (net.size() > interfaces.size()) {
				for (auto it = net.begin(); it != net.end();) {
					if (not v_contains(interfaces, it->first))
						it = net.erase(it);
					else
						it++;
				}
			}

			timestamp = new_timestamp;
		}

		//? Return empty net_info struct if no interfaces was found
		if (net.empty())
			return empty_net;

		//? Find an interface to display if selected isn't set or valid
		if (selected_iface.empty() or not v_contains(interfaces, selected_iface)) {
			max_count["download"][0] = max_count["download"][1] = max_count["upload"][0] = max_count["upload"][1] = 0;
			set_redraw(true);
			if (net_auto) rescale = true;
			if (not config_iface.empty() and v_contains(interfaces, config_iface)) selected_iface = config_iface;
			else {
				//? Sort interfaces by total upload + download bytes
				auto sorted_interfaces = interfaces;
				rng::sort(sorted_interfaces, [&](const auto& a, const auto& b){
					return 	cmp_greater(net.at(a).stat["download"].total + net.at(a).stat["upload"].total,
										net.at(b).stat["download"].total + net.at(b).stat["upload"].total);
				});
				selected_iface.clear();
				//? Try to set to a connected interface
				for (const auto& iface : sorted_interfaces) {
					if (net.at(iface).connected) selected_iface = iface;
					break;
				}
				//? If no interface is connected set to first available
				if (selected_iface.empty() and not sorted_interfaces.empty()) selected_iface = sorted_interfaces.at(0);
				else if (sorted_interfaces.empty()) return empty_net;

			}
		}

		//? Calculate max scale for graphs if needed
		if (net_auto) {
			bool sync = false;
			for (const auto& dir: {"download", "upload"}) {
				for (const auto& sel : {0, 1}) {
					if (rescale or max_count[dir][sel] >= 5) {
						const long long avg_speed = (net[selected_iface].bandwidth[dir].size() > 5
							? std::accumulate(net.at(selected_iface).bandwidth.at(dir).rbegin(), net.at(selected_iface).bandwidth.at(dir).rbegin() + 5, 0ll) / 5
							: net[selected_iface].stat[dir].speed);
						graph_max[dir] = max(uint64_t(avg_speed * (sel == 0 ? 1.3 : 3.0)), (uint64_t)10 << 10);
						max_count[dir][0] = max_count[dir][1] = 0;
						set_redraw(true);
						if (net_sync) sync = true;
						break;
					}
				}
				//? Sync download/upload graphs if enabled
				if (sync) {
					const auto other = (string(dir) == "upload" ? "download" : "upload");
					graph_max[other] = graph_max[dir];
					max_count[other][0] = max_count[other][1] = 0;
					break;
				}
			}
		}

		rescale = false;
		return net.at(selected_iface);
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

	// Static data for CPU percentage calculation
	std::unordered_map<team_id, team_usage_info> old_usage;
	bigtime_t last_update_time = 0;

	// Helper function to get memory usage for a team
	uint64_t get_team_memory(team_id team) {
		area_info areaInfo;
		ssize_t cookie = 0;
		uint64_t totalMemory = 0;

		while (get_next_area_info(team, &cookie, &areaInfo) == B_OK) {
			// Only count areas that are part of this team's address space
			if (areaInfo.team == team) {
				totalMemory += areaInfo.size;
			}
		}

		return totalMemory;
	}

	// Helper function to calculate CPU percentage
	double calculate_cpu_percent(team_id team, const team_usage_info& current_usage, bigtime_t time_diff) {
		if (time_diff <= 0 || !old_usage.contains(team)) {
			return 0.0;
		}

		auto& old_info = old_usage[team];
		bigtime_t cpu_diff = (current_usage.user_time + current_usage.kernel_time) -
							 (old_info.user_time + old_info.kernel_time);

		if (cpu_diff <= 0) {
			return 0.0;
		}

		// Convert to percentage: (cpu_time_used / total_time_elapsed) * 100
		double percent = (double)cpu_diff * 100.0 / time_diff;
		return min(percent, 100.0 * Shared::coreCount);
	}

	// Helper function to resolve username from UID
	string resolve_username(uid_t uid) {
		string uid_str = to_string(uid);
		if (uid_user.contains(uid_str)) {
			return uid_user[uid_str];
		}

		// Try to get username from system
		struct passwd* pw = getpwuid(uid);
		if (pw && pw->pw_name) {
			uid_user[uid_str] = string(pw->pw_name);
			return uid_user[uid_str];
		}

		// Fallback to UID as string
		uid_user[uid_str] = uid_str;
		return uid_str;
	}

	// Helper function to map team state to process state character
	char get_team_state(team_id team) {
		// Haiku doesn't have direct team state info, so we check thread states
		thread_info threadInfo;
		int32 cookie = 0;
		
		// Get the first thread of the team to determine state
		if (get_next_thread_info(team, &cookie, &threadInfo) == B_OK) {
			switch (threadInfo.state) {
				case B_THREAD_RUNNING:
					return 'R';
				case B_THREAD_READY:
					return 'R';
				case B_THREAD_RECEIVING:
				case B_THREAD_ASLEEP:
					return 'S';
				case B_THREAD_SUSPENDED:
					return 'T';
				default:
					return 'S';
			}
		}
		return 'S'; // Default to sleeping
	}

	// Collect detailed information for a specific process
	void _collect_details(const team_id pid, vector<proc_info> &procs) {
		if (pid != detailed.last_pid) {
			detailed = {};
			detailed.last_pid = pid;
			detailed.skip_smaps = not Config::getB("proc_info_smaps");
		}
		const auto width = get_width();

		// Find the process in current_procs
		auto p_info = rng::find(procs, pid, &proc_info::pid);
		if (p_info == procs.end()) return;
		
		detailed.entry = *p_info;

		// Update cpu percent deque for process cpu graph
		if (not Config::getB("proc_per_core")) detailed.entry.cpu_p *= Shared::coreCount;
		detailed.cpu_percent.push_back(clamp((long long)round(detailed.entry.cpu_p), 0ll, 100ll));
		while (cmp_greater(detailed.cpu_percent.size(), width)) detailed.cpu_percent.pop_front();

		// Process runtime: current time - start time
		bigtime_t current_time = system_time();
		detailed.elapsed = sec_to_dhms((current_time - detailed.entry.cpu_s) / 1000000);
		if (detailed.elapsed.size() > 8) detailed.elapsed.resize(detailed.elapsed.size() - 3);

		// Get parent process name
		if (detailed.parent.empty() && detailed.entry.ppid > 0) {
			auto parent_info = rng::find(procs, detailed.entry.ppid, &proc_info::pid);
			if (parent_info != procs.end()) {
				detailed.parent = parent_info->name;
			}
		}

		// Set status
		detailed.status = "Running";
		set_redraw(true);
	}

	auto collect(bool no_update) -> vector<proc_info>& {
		const auto sorting = Config::getS("proc_sorting");
		const auto reverse = Config::getB("proc_reversed");
		const auto filter = Config::getS("proc_filter");
		const auto per_core = Config::getB("proc_per_core");
		const auto tree = Config::getB("proc_tree");
		const auto show_detailed = Config::getB("show_detailed");
		const auto detailed_pid = Config::getI("detailed_pid");
		bool should_filter = current_filter != filter;
		if (should_filter) current_filter = filter;
		bool sorted_change = (sorting != current_sort or reverse != current_rev or should_filter);
		if (sorted_change) {
			current_sort = sorting;
			current_rev = reverse;
		}

		const int cmult = (per_core) ? Shared::coreCount : 1;
		bool got_detailed = false;

		static vector<team_id> found;

		// Get current time for CPU calculations
		bigtime_t current_time = system_time();
		bigtime_t time_diff = last_update_time > 0 ? current_time - last_update_time : 0;

		// Use cached data if only changing filter, sorting or tree options
		if (no_update and not current_procs.empty()) {
			if (show_detailed and detailed_pid != detailed.last_pid) {
				_collect_details(detailed_pid, current_procs);
			}
		}
		// Collection start
		else {
			should_filter = true;
			found.clear();

			// Get total system memory for calculations
			auto totalMem = Mem::get_totalMem();

			team_info teamInfo;
			int32 cookie = 0;

			// Iterate through all teams in the system
			while (get_next_team_info(&cookie, &teamInfo) == B_OK) {
				const team_id tid = teamInfo.team;
				found.push_back(tid);

				// Check if team already exists in current_procs
				auto find_old = rng::find(current_procs, tid, &proc_info::pid);
				bool no_cache = false;
				if (find_old == current_procs.end()) {
					current_procs.push_back({tid});
					find_old = current_procs.end() - 1;
					no_cache = true;
				}

				auto& new_proc = *find_old;

				// Cache values that shouldn't change frequently
				if (no_cache || new_proc.name.empty()) {
					new_proc.name = string(teamInfo.name);
					new_proc.cmd = string(teamInfo.args);
					if (new_proc.cmd.size() > 1000) {
						new_proc.cmd.resize(1000);
						new_proc.cmd.shrink_to_fit();
					}
					new_proc.ppid = teamInfo.parent;
					new_proc.user = resolve_username(teamInfo.uid);
					new_proc.cpu_s = teamInfo.start_time;
				}

				// Update dynamic values
				new_proc.threads = teamInfo.thread_count;
				new_proc.state = get_team_state(tid);

				// Get memory usage
				new_proc.mem = get_team_memory(tid);

				// Get CPU usage
				team_usage_info usage;
				if (get_team_usage_info(tid, B_TEAM_USAGE_SELF, &usage) == B_OK) {
					new_proc.cpu_p = calculate_cpu_percent(tid, usage, time_diff);
					new_proc.cpu_c = new_proc.cpu_p / cmult;
					new_proc.cpu_t = usage.user_time + usage.kernel_time;
					
					// Store usage for next calculation
					old_usage[tid] = usage;
				} else {
					new_proc.cpu_p = new_proc.cpu_c = 0.0;
					new_proc.cpu_t = 0;
				}

				// Check if this is the detailed process
				if (show_detailed and tid == detailed_pid) {
					got_detailed = true;
				}
			}

			// Clean up old processes that no longer exist
			current_procs |= rng::actions::remove_if([&](const auto &element) { 
				return not v_contains(found, element.pid); 
			});

			// Clean up old usage data
			for (auto it = old_usage.begin(); it != old_usage.end();) {
				if (not v_contains(found, it->first)) {
					it = old_usage.erase(it);
				} else {
					++it;
				}
			}

			// Update the details info box for process if active
			if (show_detailed and got_detailed) {
				_collect_details(detailed_pid, current_procs);
			} else if (show_detailed and not got_detailed and detailed.status != "Dead") {
				detailed.status = "Dead";
				set_redraw(true);
			}

			last_update_time = current_time;
		}

		// Update process count
		numpids = current_procs.size();

		// Match filter if defined
		if (should_filter) {
			filter_found = 0;
			for (auto& p : current_procs) {
				if (not tree and not filter.empty()) {
					if (!matches_filter(p, filter)) {
						p.filtered = true;
						filter_found++;
					} else {
						p.filtered = false;
					}
				} else {
					p.filtered = false;
				}
			}
		}

		// Sort processes
		if (sorted_change or not no_update) {
			proc_sorter(current_procs, sorting, reverse, tree);
		}

		// Generate tree view if enabled
		if (tree and (not no_update or should_filter or sorted_change)) {
			const auto &config_ints = Config::get_ints();
			bool locate_selection = false;
			if (auto find_pid = (collapse != -1 ? collapse : expand); find_pid != -1) {
				auto collapser = rng::find(current_procs, find_pid, &proc_info::pid);
				if (collapser != current_procs.end()) {
					if (collapse == expand) {
						collapser->collapsed = not collapser->collapsed;
					}
					else if (collapse > -1) {
						collapser->collapsed = true;
					}
					else if (expand > -1) {
						collapser->collapsed = false;
					}
					if (config_ints.at("proc_selected") > 0) locate_selection = true;
				}
				collapse = expand = -1;
			}
			if (should_filter or not filter.empty()) filter_found = 0;

			vector<tree_proc> tree_procs;
			tree_procs.reserve(current_procs.size());

			for (auto& p : current_procs) {
				if (not v_contains(found, p.ppid)) p.ppid = 0;
			}

			// Stable sort to retain selected sorting among processes with the same parent
			rng::stable_sort(current_procs, rng::less{}, & proc_info::ppid);

			// Start recursive iteration over processes with the lowest shared parent pids
			for (auto& p : rng::equal_range(current_procs, current_procs.at(0).ppid, rng::less{}, &proc_info::ppid)) {
				_tree_gen(p, current_procs, tree_procs, 0, false, filter, false, no_update, should_filter);
			}

			// Recursive sort over tree structure to account for collapsed processes in the tree
			int index = 0;
			tree_sort(tree_procs, sorting, reverse, index, current_procs.size());

			// Add tree begin symbol to first item if childless
			if (not tree_procs.empty() and tree_procs.front().children.empty())
				tree_procs.front().entry.get().prefix.replace(tree_procs.front().entry.get().prefix.size() - 8, 8, " ┌─ ");

			// Add tree terminator symbol to last item if childless
			if (not tree_procs.empty() and tree_procs.back().children.empty())
				tree_procs.back().entry.get().prefix.replace(tree_procs.back().entry.get().prefix.size() - 8, 8, " └─ ");

			// Final sort based on tree index
			rng::sort(current_procs, rng::less{}, & proc_info::tree_index);

			// Move current selection/view to the selected process when collapsing/expanding in the tree
			if (locate_selection) {
				int loc = rng::find(current_procs, get_selected_pid(), &proc_info::pid)->tree_index;
				if (config_ints.at("proc_start") >= loc or config_ints.at("proc_start") <= loc - get_select_max())
					Config::ints_set_at("proc_start", max(0, loc - 1));
				Config::ints_set_at("proc_selected", loc - config_ints.at("proc_start") + 1);
			}
		}

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
		Net::collect();
	}
}

namespace Tools {
	double system_uptime() {
		auto time_since_boot_ms = system_time();
		return time_since_boot_ms / 1000000.0; // Convert to seconds
	}
}
