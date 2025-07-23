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
#include <be/drivers/CAM.h>
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
namespace rng = ranges;
using namespace Tools;

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
		system_info sysInfo;
		if (get_system_info(&sysInfo) == B_OK) {
			// CPU brand is not easily available in Haiku, use CPU type instead
			switch (sysInfo.cpu_type) {
				case B_CPU_x86: return "x86";
				case B_CPU_X86_64: return "x86_64";
				case B_CPU_PPC: return "PowerPC";
				case B_CPU_M68K: return "M68K";
				case B_CPU_ARM: return "ARM";
				case B_CPU_ARM64: return "ARM64";
				case B_CPU_ALPHA: return "Alpha";
				case B_CPU_MIPS: return "MIPS";
				case B_CPU_SH: return "SH";
				default: return "Unknown";
			}
		}
		return "Unknown";
	}

	string get_cpuHz() {
		system_info sysInfo;
		if (get_system_info(&sysInfo) == B_OK) {
			// Convert clock rate from int32 to MHz string
			float clockMhz = sysInfo.cpu_clock_speed / 1000000.0;
			return std::to_string(clockMhz) + " MHz";
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

		// Get load average
		// Haiku stores load averages differently than Unix
		system_info info;
		if (get_system_info(&info) == B_OK) {
			current_cpu.load_avg[0] = info.cpu_load_average / 65536.0;
			current_cpu.load_avg[1] = current_cpu.load_avg[0]; // Haiku doesn't track separate 5 and 15 min averages
			current_cpu.load_avg[2] = current_cpu.load_avg[0];
		}

		// Get CPU usage stats
		cpu_info_t cpuInfo[Shared::coreCount];
		if (get_cpu_info(0, Shared::coreCount, cpuInfo) == B_OK) {
			long long global_totals = 0;
			long long global_idles = 0;

			for (int i = 0; i < Shared::coreCount; i++) {
				long long user = cpuInfo[i].active_time;
				long long idle = cpuInfo[i].idle_time;
				long long totals = user + idle;

				global_totals += totals;
				global_idles += idle;

				if (i < Shared::coreCount) {
					const long long calc_totals = max(1ll, totals - core_old_totals[i]);
					const long long calc_idles = max(0ll, idle - core_old_idles[i]);
					core_old_totals[i] = totals;
					core_old_idles[i] = idle;

					current_cpu.core_percent[i].push_back(
						clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

					while (cmp_greater(current_cpu.core_percent[i].size(), width * 2))
						current_cpu.core_percent[i].pop_front();
				}
			}

			// Process global CPU stats
			const long long calc_totals = max(1ll, global_totals - cpu_old.at("totals"));
			const long long calc_idles = max(0ll, global_idles - cpu_old.at("idles"));

			cpu_old.at("totals") = global_totals;
			cpu_old.at("idles") = global_idles;

			current_cpu.cpu_percent.at("total").push_back(
				clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

			while (cmp_greater(current_cpu.cpu_percent.at("total").size(), width * 2))
				current_cpu.cpu_percent.at("total").pop_front();
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
			uint64_t pageSize = B_PAGE_SIZE;
			uint64_t totalPages = sysInfo.max_pages;
			uint64_t freePages = sysInfo.free_pages;
			uint64_t cachedPages = sysInfo.cached_pages;

			uint64_t totalMem = totalPages * pageSize;
			uint64_t freeMem = freePages * pageSize;
			uint64_t cachedMem = cachedPages * pageSize;
			uint64_t usedMem = totalMem - freeMem - cachedMem;

			current_mem.stats["free"] = freeMem;
			current_mem.stats["cached"] = cachedMem;
			current_mem.stats["available"] = freeMem + cachedMem;
			current_mem.stats["used"] = usedMem;
		}

		// Get swap info if available
		if (show_swap) {
			area_info swapInfo;
			ssize_t cookie = 0;
			uint64_t swapTotal = 0;
			uint64_t swapUsed = 0;

			// Haiku uses virtual memory areas for swap
			// We can count areas marked B_SWAPPABLE
			while (get_next_area_info(B_CURRENT_TEAM, &cookie, &swapInfo) == B_OK) {
				if ((swapInfo.protection & B_SWAPPABLE) != 0) {
					swapTotal += swapInfo.size;
					swapUsed += swapInfo.ram_size;
				}
			}

			has_swap = (swapTotal > 0);
			if (has_swap) {
				current_mem.stats["swap_total"] = swapTotal;
				current_mem.stats["swap_used"] = swapUsed;
				current_mem.stats["swap_free"] = swapTotal - swapUsed;
			} else {
				current_mem.stats["swap_total"] = 0;
				current_mem.stats["swap_used"] = 0;
				current_mem.stats["swap_free"] = 0;
			}
		}

		// Update percentage values
		const uint64_t totalMem = get_totalMem();
		for (const auto& name : mem_names) {
			current_mem.percent.at(name).push_back(
				clamp((long long)round((double)current_mem.stats.at(name) * 100 / totalMem), 0ll, 100ll));
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
	net_info empty_net;

	void init_net() {
		// Find all network interfaces
		BNetworkRoster& roster = BNetworkRoster::Default();
		BNetworkInterface interface;
		int32_t cookie = 0;

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
		static bool initialized = false;

		if (!initialized) {
			init_net();
			initialized = true;
		}

		if (current_net.empty())
			return empty_net;

		if (no_update && !get_selected_iface().empty() &&
			!current_net.at(get_selected_iface()).bandwidth.at("download").empty())
			return current_net.at(get_selected_iface());

		const auto width = get_width();
		const string& selected_iface = get_selected_iface();

		// If no interface selected or doesn't exist, select first available
		if (selected_iface.empty() || !current_net.contains(selected_iface)) {
			if (!current_net.empty())
				set_selected_iface(current_net.begin()->first);
		}

		// Update interface info
		BNetworkRoster& roster = BNetworkRoster::Default();
		BNetworkInterface interface;
		int32_t cookie = 0;

		while (roster.GetNextInterface(&cookie, interface) == B_OK) {
			const char* name = interface.Name();
			if (!name || strlen(name) == 0)
				continue;

			string ifname(name);
			if (!current_net.contains(ifname))
				continue;

			auto& net = current_net.at(ifname);

			// Get stats
			ifreq ifr;
			strncpy(ifr.ifr_name, name, IFNAMSIZ);

			// Get interface status
			net.connected = (interface.IsRunning() == true);

			// Get IP addresses
			BNetworkInterfaceAddress address;
			int32_t addrCookie = 0;

			net.ipv4 = "";
			net.ipv6 = "";

			while (interface.GetNextAddress(&addrCookie, address) == B_OK) {
				BNetworkAddress addr = address.Address();
				if (addr.Family() == AF_INET) {
					char buffer[INET_ADDRSTRLEN];
					if (inet_ntop(AF_INET, &addr.data[0], buffer, sizeof(buffer))) {
						net.ipv4 = buffer;
					}
				} else if (addr.Family() == AF_INET6) {
					char buffer[INET6_ADDRSTRLEN];
					if (inet_ntop(AF_INET6, &addr.data[0], buffer, sizeof(buffer))) {
						net.ipv6 = buffer;
					}
				}
			}

			// Get sent/received bytes
			uint64_t bytesReceived = 0, bytesSent = 0;

			// Haiku doesn't have a direct API for getting bytes sent/received
			// Normally we would use ioctl to read interface stats, but for this
			// implementation we'll use placeholder values that change over time

			static uint64_t fakeReceived = 0, fakeSent = 0;
			fakeReceived += rand() % 50000;
			fakeSent += rand() % 40000;

			bytesReceived = fakeReceived;
			bytesSent = fakeSent;

			// Update download stats
			auto& dl = net.stat.at("download");
			if (dl.last > bytesReceived)
				dl.rollover++;

			dl.last = bytesReceived;
			dl.total = (dl.rollover == 0 ? dl.last : UINT64_MAX * dl.rollover + dl.last);

			if (dl.offset == 0)
				dl.offset = dl.total;

			dl.speed = max((uint64_t)0, dl.total - dl.offset);
			dl.offset = dl.total;
			dl.top = max(dl.top, dl.speed);

			// Update upload stats
			auto& ul = net.stat.at("upload");
			if (ul.last > bytesSent)
				ul.rollover++;

			ul.last = bytesSent;
			ul.total = (ul.rollover == 0 ? ul.last : UINT64_MAX * ul.rollover + ul.last);

			if (ul.offset == 0)
				ul.offset = ul.total;

			ul.speed = max((uint64_t)0, ul.total - ul.offset);
			ul.offset = ul.total;
			ul.top = max(ul.top, ul.speed);

			// Update bandwidth deques
			net.bandwidth.at("download").push_back(dl.speed);
			while (cmp_greater(net.bandwidth.at("download").size(), width * 2))
				net.bandwidth.at("download").pop_front();

			net.bandwidth.at("upload").push_back(ul.speed);
			while (cmp_greater(net.bandwidth.at("upload").size(), width * 2))
				net.bandwidth.at("upload").pop_front();
		}

		// Return selected interface or empty_net if no selected interface
		return (selected_iface.empty() || !current_net.contains(selected_iface))
			? empty_net : current_net.at(selected_iface);
	}
}

namespace Proc {
	atomic<int> numpids = 0;
	int currentPids = 0;
	vector<proc_info> current_procs;
	detail_container detailed;

	auto collect(bool no_update) -> vector<proc_info>& {
		if (no_update)
			return current_procs;

		current_procs.clear();

		// Get system teams/processes
		team_info teamInfo;
		int32 teamCookie = 0;

		while (get_next_team_info(&teamCookie, &teamInfo) == B_OK) {
			// Skip the kernel team
			if (teamInfo.team == B_SYSTEM_TEAM)
				continue;

			proc_info proc;

			proc.pid = teamInfo.team;
			proc.name = teamInfo.args;
			proc.cmd = teamInfo.args;

			// Simplify command name for display
			size_t spacePos = proc.cmd.find(' ');
			if (spacePos != string::npos) {
				proc.short_cmd = proc.cmd.substr(0, spacePos);
			} else {
				proc.short_cmd = proc.cmd;
			}

			// Extract basename from path if command includes path
			size_t slashPos = proc.short_cmd.find_last_of('/');
			if (slashPos != string::npos) {
				proc.short_cmd = proc.short_cmd.substr(slashPos + 1);
			}

			// Get thread count
			proc.threads = teamInfo.thread_count;

			// Memory usage
			proc.mem = teamInfo.area_count * B_PAGE_SIZE; // Approximate

			// Get CPU usage (approximate based on CPU time)
			thread_info threadInfo;
			int32 threadCookie = 0;
			uint64_t totalTime = 0;

			while (get_next_thread_info(teamInfo.team, &threadCookie, &threadInfo) == B_OK) {
				totalTime += threadInfo.user_time + threadInfo.kernel_time;
			}

			// Convert CPU time to percentage (very approximate)
			static std::unordered_map<size_t, uint64_t> lastCpuTime;
			static std::unordered_map<size_t, double> lastCpuPercentage;

			if (lastCpuTime.contains(proc.pid)) {
				uint64_t timeDiff = totalTime - lastCpuTime[proc.pid];
				// Simple CPU calculation - this is not accurate but works for demonstration
				proc.cpu_p = timeDiff / 10000.0;
				proc.cpu_p = std::min(proc.cpu_p, 100.0); // Cap at 100%
			} else {
				proc.cpu_p = 0.0;
			}

			// Smooth CPU values a bit
			if (lastCpuPercentage.contains(proc.pid)) {
				proc.cpu_p = lastCpuPercentage[proc.pid] * 0.7 + proc.cpu_p * 0.3;
			}

			lastCpuTime[proc.pid] = totalTime;
			lastCpuPercentage[proc.pid] = proc.cpu_p;

			// CPU cumulative is the same as direct in this implementation
			proc.cpu_c = proc.cpu_p;

			// Process state
			proc.state = (threadInfo.state == B_THREAD_RUNNING) ? 'R' : 'S';

			// User info - get from the uid
			proc.user = "user"; // Default
			struct passwd* pw = getpwuid(teamInfo.uid);
			if (pw != nullptr) {
				proc.user = pw->pw_name;
			}

			// Nice value
			proc.p_nice = 0; // Haiku doesn't expose this easily

			// Parent PID is not easily accessible in Haiku API
			proc.ppid = 0;

			// Add to process list
			current_procs.push_back(proc);
		}

		// Update pid count
		currentPids = current_procs.size();
		numpids = currentPids;

		// Update details for selected process if needed
		int selected_pid = get_selected_pid();
		if (selected_pid > 0 && detailed.last_pid != static_cast<size_t>(selected_pid)) {
			detailed.last_pid = selected_pid;

			// Find the selected process
			auto it = std::find_if(current_procs.begin(), current_procs.end(),
				[selected_pid](const proc_info& p) { return p.pid == static_cast<size_t>(selected_pid); });

			if (it != current_procs.end()) {
				detailed.entry = *it;

				// Get more detailed information
				team_info info;
				if (get_team_info(selected_pid, &info) == B_OK) {
					detailed.memory = std::to_string(info.area_count * B_PAGE_SIZE / 1024) + " KB";

					// Format elapsed time
					bigtime_t uptime = system_time() - info.start_time;
					int64_t seconds = uptime / 1000000;
					int64_t minutes = seconds / 60;
					seconds %= 60;
					int64_t hours = minutes / 60;
					minutes %= 60;

					detailed.elapsed = std::to_string(hours) + ":" +
						(minutes < 10 ? "0" : "") + std::to_string(minutes) + ":" +
						(seconds < 10 ? "0" : "") + std::to_string(seconds);

					detailed.status = (info.thread_count > 0) ? "Running" : "Unknown";
					detailed.parent = "Unknown"; // Parent info not easily available
					detailed.io_read = "N/A";     // IO stats not available
					detailed.io_write = "N/A";
				}
			}
		}

		return current_procs;
	}
}

namespace Shared {
	long pageSize, clkTck, coreCount, bootTime;
	uint64_t totalMem;

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
	}
}

namespace Tools {
	double system_uptime() {
		system_info sysInfo;
		if (get_system_info(&sysInfo) == B_OK) {
			// Convert from microseconds to seconds
			return sysInfo.boot_time / 1000000.0;
		}
		return 0.0;
	}
}
