/* Copyright 2021 Aristocratos (jakob@qvantnet.com)
   Copyright 2025 Brett Jia (dev.bjia56@gmail.com)

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
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
// man 3 getifaddrs: "BUGS: If	both <net/if.h>	and <ifaddrs.h>	are being included, <net/if.h> must be included before <ifaddrs.h>"
#include <net/if.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/tcp_fsm.h>
#include <netinet/in.h> // for inet_ntop stuff
#include <pwd.h>
#include <sys/_timeval.h>
#include <sys/endian.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/vmmeter.h>
#include <sys/limits.h>
#include <vector>
#include <vm/vm_param.h>
#include <kvm.h>
#include <paths.h>
#include <fcntl.h>
#include <unistd.h>
#include <devstat.h>

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

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

namespace Cpu {
	vector<long long> core_old_totals;
	vector<long long> core_old_idles;
	vector<string> available_fields = {"Auto", "total"};
	vector<string> available_sensors = {"Auto"};
	cpu_info current_cpu;
	bool got_sensors = false, cpu_temp_only = false;

	//* Populate found_sensors map
	bool get_sensors();

	//* Get current cpu clock speed
	string get_cpuHz();

	//* Search /proc/cpuinfo for a cpu name
	string get_cpuName();

	struct Sensor {
		fs::path path;
		string label;
		int64_t temp = 0;
		int64_t high = 0;
		int64_t crit = 0;
	};

	string cpu_sensor;
	vector<string> core_sensors;
	std::unordered_map<int, int> core_mapping;
}  // namespace Cpu

namespace Mem {
	double old_uptime;
	std::vector<string> zpools;

	void get_zpools();
}

namespace Shared {

	fs::path passwd_path;
	uint64_t totalMem;
	long pageSize, clkTck, coreCount, physicalCoreCount, arg_max;
	int totalMem_len, kfscale;
	long bootTime;

	void init() {
		//? Shared global variables init
		int mib[2];
		mib[0] = CTL_HW;
		mib[1] = HW_NCPU;
		int ncpu;
		size_t len = sizeof(ncpu);
		if (sysctl(mib, 2, &ncpu, &len, nullptr, 0) == -1) {
			Logger::warning("Could not determine number of cores, defaulting to 1.");
		} else {
			coreCount = ncpu;
		}

		pageSize = sysconf(_SC_PAGE_SIZE);
		if (pageSize <= 0) {
			pageSize = 4096;
			Logger::warning("Could not get system page size. Defaulting to 4096, processes memory usage might be incorrect.");
		}

		clkTck = sysconf(_SC_CLK_TCK);
		if (clkTck <= 0) {
			clkTck = 100;
			Logger::warning("Could not get system clock ticks per second. Defaulting to 100, processes cpu usage might be incorrect.");
		}

		int64_t memsize = 0;
		size_t size = sizeof(memsize);
		if (sysctlbyname("hw.physmem", &memsize, &size, nullptr, 0) < 0) {
			Logger::warning("Could not get memory size");
		}
		totalMem = memsize;

		struct timeval result;
		size = sizeof(result);
		if (sysctlbyname("kern.boottime", &result, &size, nullptr, 0) < 0) {
			Logger::warning("Could not get boot time");
		} else {
			bootTime = result.tv_sec;
		}

		size = sizeof(kfscale);
		if (sysctlbyname("kern.fscale", &kfscale, &size, nullptr, 0) == -1) {
			kfscale = 2048;
		}

		//* Get maximum length of process arguments
		arg_max = sysconf(_SC_ARG_MAX);

		//? Init for namespace Cpu
		Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), Shared::coreCount, {});
		Cpu::current_cpu.temp.insert(Cpu::current_cpu.temp.begin(), Shared::coreCount + 1, {});
		Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), Shared::coreCount, 0);
		Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), Shared::coreCount, 0);
		Logger::debug("Init -> Cpu::collect()");
		Cpu::collect();
		for (auto &[field, vec] : Cpu::current_cpu.cpu_percent) {
			if (not vec.empty() and not v_contains(Cpu::available_fields, field)) Cpu::available_fields.push_back(field);
		}
		Logger::debug("Init -> Cpu::get_cpuName()");
		Cpu::cpuName = Cpu::get_cpuName();
		Logger::debug("Init -> Cpu::get_sensors()");
		Cpu::got_sensors = Cpu::get_sensors();
		Logger::debug("Init -> Cpu::get_core_mapping()");
		Cpu::core_mapping = Cpu::get_core_mapping();
		Cpu::current_bat = Cpu::get_battery();

		//? Init for namespace Mem
		Mem::old_uptime = system_uptime();
		Logger::debug("Init -> Mem::collect()");
		Mem::collect();
		Logger::debug("Init -> Mem::get_zpools()");
		Mem::get_zpools();

		//? Init for namespace Net
		Logger::debug("Init -> Net::collect()");
		Net::collect();
	}
}  // namespace Shared

namespace Cpu {
	string cpuName;
	string cpuHz;
	bool has_battery = true;
	tuple<int, float, long, string> current_bat;

	const array<string, 10> time_names = {"user", "nice", "system", "idle"};

	std::unordered_map<string, long long> cpu_old = {
		{"totals", 0},
		{"idles", 0},
		{"user", 0},
		{"nice", 0},
		{"system", 0},
		{"idle", 0}
	};

	string get_cpuName() {
		string name;
		char buffer[1024];
		size_t size = sizeof(buffer);
		if (sysctlbyname("hw.model", &buffer, &size, nullptr, 0) < 0) {
			Logger::error("Failed to get CPU name");
			return name;
		}
		name = string(buffer);

		auto name_vec = ssplit(name);

		if ((s_contains(name, "Xeon"s) or v_contains(name_vec, "Duo"s)) and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')'))
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		} else if (v_contains(name_vec, "Ryzen"s)) {
			auto ryz_pos = v_index(name_vec, "Ryzen"s);
			name = "Ryzen" + (ryz_pos < name_vec.size() - 1 ? ' ' + name_vec.at(ryz_pos + 1) : "") + (ryz_pos < name_vec.size() - 2 ? ' ' + name_vec.at(ryz_pos + 2) : "");
		} else if (s_contains(name, "Intel"s) and v_contains(name_vec, "CPU"s)) {
			auto cpu_pos = v_index(name_vec, "CPU"s);
			if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')') and name_vec.at(cpu_pos + 1) != "@")
				name = name_vec.at(cpu_pos + 1);
			else
				name.clear();
		} else
			name.clear();

		if (name.empty() and not name_vec.empty()) {
			for (const auto &n : name_vec) {
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

	bool get_sensors() {
		got_sensors = false;
		if (Config::getB("show_coretemp") and Config::getB("check_temp")) {
			int32_t temp;
			size_t size = sizeof(temp);
			if (sysctlbyname("dev.cpu.0.temperature", &temp, &size, nullptr, 0) < 0) {
				Logger::warning("Could not get temp sensor - maybe you need to load the coretemp module");
			} else {
				got_sensors = true;
				int temp;
				size_t size = sizeof(temp);
				sysctlbyname("dev.cpu.0.coretemp.tjmax", &temp, &size, nullptr, 0); //assuming the max temp is same for all cores
				temp = (temp - 2732) / 10; // since it's an int, it's multiplied by 10, and offset to absolute zero...
				current_cpu.temp_max = temp;
			}
		}
		return got_sensors;
	}

	void update_sensors() {
		int temp = 0;
		int p_temp = 0;
		int found = 0;
		bool got_package = false;
		size_t size = sizeof(p_temp);
		if (sysctlbyname("hw.acpi.thermal.tz0.temperature", &p_temp, &size, nullptr, 0) >= 0) {
			got_package = true;
			p_temp = (p_temp - 2732) / 10; // since it's an int, it's multiplied by 10, and offset to absolute zero...
		}

		size = sizeof(temp);
		for (int i = 0; i < Shared::coreCount; i++) {
			string s = "dev.cpu." + std::to_string(i) + ".temperature";
			if (sysctlbyname(s.c_str(), &temp, &size, nullptr, 0) >= 0) {
				temp = (temp - 2732) / 10;
				if (not got_package) {
					p_temp += temp;
					found++;
				}
				if (cmp_less(i + 1, current_cpu.temp.size())) {
					current_cpu.temp.at(i + 1).push_back(temp);
					if (current_cpu.temp.at(i + 1).size() > 20)
						current_cpu.temp.at(i + 1).pop_front();
				}
			}
		}

		if (not got_package) p_temp /= found;
		current_cpu.temp.at(0).push_back(p_temp);
		if (current_cpu.temp.at(0).size() > 20)
			current_cpu.temp.at(0).pop_front();

	}

	string get_cpuHz() {
		unsigned int freq = 1;
		size_t size = sizeof(freq);

		if (sysctlbyname("dev.cpu.0.freq", &freq, &size, nullptr, 0) < 0) {
			return "";
		}
		return std::to_string(freq / 1000.0 ).substr(0, 3); // seems to be in MHz
	}

	auto get_core_mapping() -> std::unordered_map<int, int> {
		std::unordered_map<int, int> core_map;
		if (cpu_temp_only) return core_map;

		for (long i = 0; i < Shared::coreCount; i++) {
			core_map[i] = i;
		}

		//? If core mapping from cpuinfo was incomplete try to guess remainder, if missing completely, map 0-0 1-1 2-2 etc.
		if (cmp_less(core_map.size(), Shared::coreCount)) {
			if (Shared::coreCount % 2 == 0 and (long) core_map.size() == Shared::coreCount / 2) {
				for (int i = 0, n = 0; i < Shared::coreCount / 2; i++) {
					if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
					core_map[Shared::coreCount / 2 + i] = n++;
				}
			} else {
				core_map.clear();
				for (int i = 0, n = 0; i < Shared::coreCount; i++) {
					if (std::cmp_greater_equal(n, core_sensors.size())) n = 0;
					core_map[i] = n++;
				}
			}
		}

		//? Apply user set custom mapping if any
		const auto custom_map = Config::getS("cpu_core_map");
		if (not custom_map.empty()) {
			try {
				for (const auto &split : ssplit(custom_map)) {
					const auto vals = ssplit(split, ':');
					if (vals.size() != 2) continue;
					int change_id = std::stoi(vals.at(0));
					int new_id = std::stoi(vals.at(1));
					if (not core_map.contains(change_id) or cmp_greater(new_id, core_sensors.size())) continue;
					core_map.at(change_id) = new_id;
				}
			} catch (...) {
			}
		}

		return core_map;
	}

	auto get_battery() -> tuple<int, float, long, string> {
		if (not has_battery) return {0, 0, 0, ""};

		long seconds = -1;
		float watts = -1;
		uint32_t percent = -1;
		size_t size = sizeof(percent);
		string status = "discharging";
		if (sysctlbyname("hw.acpi.battery.life", &percent, &size, nullptr, 0) < 0) {
			has_battery = false;
		} else {
			has_battery = true;
			size_t size = sizeof(seconds);
			if (sysctlbyname("hw.acpi.battery.time", &seconds, &size, nullptr, 0) < 0) {
				seconds = 0;
			}
			size = sizeof(watts);
			if (sysctlbyname("hw.acpi.battery.rate", &watts, &size, nullptr, 0) < 0) {
				watts = -1;
			}
			int state;
			size = sizeof(state);
			if (sysctlbyname("hw.acpi.battery.state", &state, &size, nullptr, 0) < 0) {
				status = "unknown";
			} else {
				if (state == 2) {
					status = "charging";
				}
			}
			if (percent == 100) {
				status = "full";
			}
		}

		return {percent, watts, seconds, status};
	}

	auto collect(bool no_update) -> cpu_info & {
		if (no_update and not current_cpu.cpu_percent.at("total").empty())
			return current_cpu;
		auto &cpu = current_cpu;
		const auto width = get_width();

		if (getloadavg(cpu.load_avg.data(), cpu.load_avg.size()) < 0) {
			Logger::error("failed to get load averages");
		}

		vector<array<long, CPUSTATES>> cpu_time(Shared::coreCount);
		size_t size = sizeof(long) * CPUSTATES * Shared::coreCount;
		if (sysctlbyname("kern.cp_times", &cpu_time[0], &size, nullptr, 0) == -1) {
			Logger::error("failed to get CPU times");
		}
		long long global_totals = 0;
		long long global_idles = 0;
		vector<long long> times_summed = {0, 0, 0, 0};

		for (long i = 0; i < Shared::coreCount; i++) {
			vector<long long> times;
			//? 0=user, 1=nice, 2=system, 3=idle
			for (int x = 0; const unsigned int c_state : {CP_USER, CP_NICE, CP_SYS, CP_IDLE}) {
				auto val = cpu_time[i][c_state];
				times.push_back(val);
				times_summed.at(x++) += val;
			}
			try {
				//? All values
				const long long totals = std::accumulate(times.begin(), times.end(), 0ll);

				//? Idle time
				const long long idles = times.at(3);

				global_totals += totals;
				global_idles += idles;

				//? Calculate cpu total for each core
				if (i > Shared::coreCount) break;
				const long long calc_totals = max(0ll, totals - core_old_totals.at(i));
				const long long calc_idles = max(0ll, idles - core_old_idles.at(i));
				core_old_totals.at(i) = totals;
				core_old_idles.at(i) = idles;

				cpu.core_percent.at(i).push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

				//? Reduce size if there are more values than needed for graph
				if (cpu.core_percent.at(i).size() > 40) cpu.core_percent.at(i).pop_front();

			} catch (const std::exception &e) {
				Logger::error("Cpu::collect() : " + (string)e.what());
				throw std::runtime_error("collect() : " + (string)e.what());
			}

		}

		const long long calc_totals = max(1ll, global_totals - cpu_old.at("totals"));
		const long long calc_idles = max(1ll, global_idles - cpu_old.at("idles"));

		//? Populate cpu.cpu_percent with all fields from syscall
		for (int ii = 0; const auto &val : times_summed) {
			cpu.cpu_percent.at(time_names.at(ii)).push_back(clamp((long long)round((double)(val - cpu_old.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll));
			cpu_old.at(time_names.at(ii)) = val;

			//? Reduce size if there are more values than needed for graph
			while (cmp_greater(cpu.cpu_percent.at(time_names.at(ii)).size(), width * 2)) cpu.cpu_percent.at(time_names.at(ii)).pop_front();

			ii++;
		}

		cpu_old.at("totals") = global_totals;
		cpu_old.at("idles") = global_idles;

		//? Total usage of cpu
		cpu.cpu_percent.at("total").push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

		//? Reduce size if there are more values than needed for graph
		while (cmp_greater(cpu.cpu_percent.at("total").size(), width * 2)) cpu.cpu_percent.at("total").pop_front();

		if (Config::getB("show_cpu_freq")) {
			auto hz = get_cpuHz();
			if (hz != "") {
				cpuHz = hz;
			}
		}

		if (Config::getB("check_temp") and got_sensors)
			update_sensors();

		if (Config::getB("show_battery") and has_battery)
			current_bat = get_battery();

		return cpu;
	}
}  // namespace Cpu

namespace Mem {
	bool has_swap = false;
	vector<string> fstab;
	fs::file_time_type fstab_time;
	int disk_ios = 0;
	vector<string> last_found;

	mem_info current_mem{};

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

		// no io times - need to push something anyway or we'll get an ABORT
		if (disk.io_activity.empty())
			disk.io_activity.push_back(0);
		else
			disk.io_activity.push_back(clamp((long)round((double)(disk.io_write.back() + disk.io_read.back()) / (1 << 20)), 0l, 100l));
		while (cmp_greater(disk.io_activity.size(), width * 2)) disk.io_activity.pop_front();
	}

	class PipeWrapper {
		public:
			PipeWrapper(const char *file, const char *mode) {fd = popen(file, mode);}
			virtual ~PipeWrapper() {if (fd) pclose(fd);}
			auto operator()() -> FILE* { return fd;};
		private:
			FILE *fd;
	};

	// find all zpools in the system. Do this only at startup.
	void get_zpools() {
		std::regex toReplace("\\.");
		PipeWrapper poolPipe = PipeWrapper("zpool list -H -o name", "r");

		while (not std::feof(poolPipe())) {
			char poolName[512];
			size_t len = 512;
			if (fgets(poolName, len, poolPipe())) {
				poolName[strcspn(poolName, "\n")] = 0;
				Logger::debug("zpool found: " + string(poolName));
				Mem::zpools.push_back(std::regex_replace(poolName, toReplace, "%25"));
			}
		}
	}

	void collect_disk(std::unordered_map<string, disk_info> &disks, std::unordered_map<string, string> &mapping) {
		// this bit is for 'regular' mounts
		static struct statinfo cur;
		long double etime = 0;
		uint64_t total_bytes_read;
		uint64_t total_bytes_write;

		static std::unique_ptr<struct devinfo, decltype(std::free)*> curDevInfo (reinterpret_cast<struct devinfo*>(std::calloc(1, sizeof(struct devinfo))), std::free);
		cur.dinfo = curDevInfo.get();

		if (getdevs(&cur) != -1) {
			for (int i = 0; i < cur.dinfo->numdevs; i++) {
				auto d = cur.dinfo->devices[i];
				string devStatName = "/dev/" + string(d.device_name) + std::to_string(d.unit_number);
				for (auto& [ignored, disk] : disks) { // find matching mountpoints - could be multiple as d.device_name is only ada (and d.unit_number is the device number), while the disk.dev is like /dev/ada0s1
					if (disk.dev.string().rfind(devStatName, 0) == 0 and mapping.contains(disk.dev)) {
						compute_stats_read(&d, nullptr, etime, &total_bytes_read, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
						compute_stats_write(&d, nullptr, etime, &total_bytes_write, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
						assign_values(disk, total_bytes_read, total_bytes_write);
						string mountpoint = mapping.at(disk.dev);
						Logger::debug("dev " + devStatName + " -> " + mountpoint  + " read=" + std::to_string(total_bytes_read) + " write=" + std::to_string(total_bytes_write));
					}
				}

			}
		}

		// this code is for ZFS mounts
		for (const auto &poolName : Mem::zpools) {
			char sysCtl[1024];
			snprintf(sysCtl, sizeof(sysCtl), "sysctl kstat.zfs.%s.dataset | egrep \'dataset_name|nread|nwritten\'", poolName.c_str());
			PipeWrapper f = PipeWrapper(sysCtl, "r");
			if (f()) {
				char buf[512];
				size_t len = 512;
				uint64_t nread = 0, nwritten = 0;
				while (not std::feof(f())) {
					if (fgets(buf, len, f())) {
						char *name = std::strtok(buf, ": \n");
						char *value = std::strtok(nullptr, ": \n");
						if (string(name).find("dataset_name") != string::npos) {
							// create entry if datasetname matches with anything in mapping
							// relies on the fact that the dataset name is last value in the list
							// alternatively you could parse the objset-0x... when this changes, you have a new entry
							string datasetname = string(value);// this is the zfs volume, like 'zroot/usr/home' -> this maps onto the device we get back from getmntinfo(3)
							if (mapping.contains(datasetname)) {
								string mountpoint = mapping.at(datasetname);
								if (disks.contains(mountpoint)) {
									auto& disk = disks.at(mountpoint);
									assign_values(disk, nread, nwritten);
								}
							}
						} else if (string(name).find("nread") != string::npos) {
							nread = atoll(value);
						} else if (string(name).find("nwritten") != string::npos) {
							nwritten = atoll(value);
						}
					}
				}
			}
		}

	}

	auto collect(bool no_update) -> mem_info & {
		if (no_update and not current_mem.percent.at("used").empty())
			return current_mem;

		const auto show_swap = Config::getB("show_swap");
		const auto show_disks = Config::getB("show_disks");
		const auto swap_disk = Config::getB("swap_disk");
		auto &mem = current_mem;
		static bool snapped = (getenv("COSMOTOP_SNAPPED") != nullptr);
		const auto width = get_width();

		int mib[4];
		u_int memActive, memWire, cachedMem, freeMem;
		size_t len;

   		len = 4; sysctlnametomib("vm.stats.vm.v_active_count", mib, &len);
		len = sizeof(memActive);
		sysctl(mib, 4, &(memActive), &len, nullptr, 0);
		memActive *= Shared::pageSize;

		len = 4; sysctlnametomib("vm.stats.vm.v_wire_count", mib, &len);
		len = sizeof(memWire);
		sysctl(mib, 4, &(memWire), &len, nullptr, 0);
		memWire *= Shared::pageSize;

		mem.stats.at("used") = memWire + memActive;
		mem.stats.at("available") = Shared::totalMem - memActive - memWire;

		len = sizeof(cachedMem);
   		len = 4; sysctlnametomib("vm.stats.vm.v_cache_count", mib, &len);
   		sysctl(mib, 4, &(cachedMem), &len, nullptr, 0);
   		cachedMem *= Shared::pageSize;
   		mem.stats.at("cached") = cachedMem;

		len = sizeof(freeMem);
   		len = 4; sysctlnametomib("vm.stats.vm.v_free_count", mib, &len);
   		sysctl(mib, 4, &(freeMem), &len, nullptr, 0);
   		freeMem *= Shared::pageSize;
   		mem.stats.at("free") = freeMem;

		if (show_swap) {
			char buf[_POSIX2_LINE_MAX];
			Shared::KvmPtr kd {kvm_openfiles(nullptr, _PATH_DEVNULL, nullptr, O_RDONLY, buf)};
   			struct kvm_swap swap[16];
   			int nswap = kvm_getswapinfo(kd.get(), swap, 16, 0);
			int totalSwap = 0, usedSwap = 0;
			for (int i = 0; i < nswap; i++) {
				totalSwap += swap[i].ksw_total;
				usedSwap += swap[i].ksw_used;
			}
			mem.stats.at("swap_total") = totalSwap * Shared::pageSize;
			mem.stats.at("swap_used") = usedSwap * Shared::pageSize;
		}

		if (show_swap and mem.stats.at("swap_total") > 0) {
			for (const auto &name : swap_names) {
				mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / mem.stats.at("swap_total")));
				while (cmp_greater(mem.percent.at(name).size(), width * 2))
					mem.percent.at(name).pop_front();
			}
			has_swap = true;
		} else
			has_swap = false;
		//? Calculate percentages
		for (const auto &name : mem_names) {
			mem.percent.at(name).push_back(round((double)mem.stats.at(name) * 100 / Shared::totalMem));
			while (cmp_greater(mem.percent.at(name).size(), width * 2))
				mem.percent.at(name).pop_front();
		}

		if (show_disks) {
			std::unordered_map<string, string> mapping;  // keep mapping from device -> mountpoint, since IOKit doesn't give us the mountpoint
			double uptime = system_uptime();
			const auto disks_filter = Config::getS("disks_filter");
			bool filter_exclude = false;
			// auto only_physical = Config::getB("only_physical");
			auto &disks = mem.disks;
			vector<string> filter;
			if (not disks_filter.empty()) {
				filter = ssplit(disks_filter);
				if (filter.at(0).starts_with("exclude=")) {
					filter_exclude = true;
					filter.at(0) = filter.at(0).substr(8);
				}
			}

			struct statfs *stfs;
			int count = getmntinfo(&stfs, MNT_WAIT);
			vector<string> found;
			found.reserve(last_found.size());
			for (int i = 0; i < count; i++) {
				auto fstype = string(stfs[i].f_fstypename);
				if (fstype == "autofs" || fstype == "devfs" || fstype == "linprocfs" || fstype == "procfs" || fstype == "tmpfs" || fstype == "linsysfs" ||
					fstype == "fdesckfs") {
					// in memory filesystems -> not useful to show
					continue;
				}

				std::error_code ec;
				string mountpoint = stfs[i].f_mntonname;
				string dev = stfs[i].f_mntfromname;
				mapping[dev] = mountpoint;

				//? Match filter if not empty
				if (not filter.empty()) {
					bool match = v_contains(filter, mountpoint);
					if ((filter_exclude and match) or (not filter_exclude and not match))
						continue;
				}

				found.push_back(mountpoint);
				if (not disks.contains(mountpoint)) {
					disks[mountpoint] = disk_info{fs::canonical(dev, ec), fs::path(mountpoint).filename()};

					if (disks.at(mountpoint).dev.empty())
						disks.at(mountpoint).dev = dev;

					if (disks.at(mountpoint).name.empty())
						disks.at(mountpoint).name = (mountpoint == "/" ? "root" : mountpoint);
				}


				if (not v_contains(last_found, mountpoint))
					set_redraw(true);

				disks.at(mountpoint).free = stfs[i].f_bfree;
				disks.at(mountpoint).total = stfs[i].f_iosize;
			}

			//? Remove disks no longer mounted or filtered out
			if (swap_disk and has_swap) found.push_back("swap");
			for (auto it = disks.begin(); it != disks.end();) {
				if (not v_contains(found, it->first))
					it = disks.erase(it);
				else
					it++;
			}
			if (found.size() != last_found.size()) set_redraw(true);
			last_found = std::move(found);

			//? Get disk/partition stats
			for (auto &[mountpoint, disk] : disks) {
				if (std::error_code ec; not fs::exists(mountpoint, ec))
					continue;
				struct statvfs vfs;
				if (statvfs(mountpoint.c_str(), &vfs) < 0) {
					Logger::warning("Failed to get disk/partition stats with statvfs() for: " + mountpoint);
					continue;
				}
				disk.total = vfs.f_blocks * vfs.f_frsize;
				disk.free = vfs.f_bfree * vfs.f_frsize;
				disk.used = disk.total - disk.free;
				if (disk.total != 0) {
					disk.used_percent = round((double)disk.used * 100 / disk.total);
					disk.free_percent = 100 - disk.used_percent;
				} else {
					disk.used_percent = 0;
					disk.free_percent = 0;
				}
			}

			//? Setup disks order in UI and add swap if enabled
			mem.disks_order.clear();
			if (snapped and disks.contains("/mnt"))
				mem.disks_order.push_back("/mnt");
			else if (disks.contains("/"))
				mem.disks_order.push_back("/");
			if (swap_disk and has_swap) {
				mem.disks_order.push_back("swap");
				if (not disks.contains("swap"))
					disks["swap"] = {"", "swap"};
				disks.at("swap").total = mem.stats.at("swap_total");
				disks.at("swap").used = mem.stats.at("swap_used");
				disks.at("swap").free = mem.stats.at("swap_free");
				disks.at("swap").used_percent = mem.percent.at("swap_used").back();
				disks.at("swap").free_percent = mem.percent.at("swap_free").back();
			}
			for (const auto &name : last_found)
				if (not is_in(name, "/", "swap", "/dev"))
					mem.disks_order.push_back(name);

			disk_ios = 0;
			collect_disk(disks, mapping);

			old_uptime = uptime;
		}
		return mem;
	}

}  // namespace Mem

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
		auto &net = current_net;
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
			for (auto *ifa = if_addrs.get(); ifa != nullptr; ifa = ifa->ifa_next) {
				if (ifa->ifa_addr == nullptr) continue;
				family = ifa->ifa_addr->sa_family;
				const auto &iface = ifa->ifa_name;
				//? Update available interfaces vector and get status of interface
				if (not v_contains(interfaces, iface)) {
					interfaces.push_back(iface);
					net[iface].connected = (ifa->ifa_flags & IFF_RUNNING);

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
				}  //else, ignoring family==AF_LINK (see man 3 getifaddrs)
			}

			std::unordered_map<string, std::tuple<uint64_t, uint64_t>> ifstats;
			int mib[] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0};
			size_t len;
			if (sysctl(mib, 6, nullptr, &len, nullptr, 0) < 0) {
				Logger::error("failed getting network interfaces");
			} else {
				std::unique_ptr<char[]> buf(new char[len]);
				if (sysctl(mib, 6, buf.get(), &len, nullptr, 0) < 0) {
					Logger::error("failed getting network interfaces");
				} else {
					char *lim = buf.get() + len;
					char *next = nullptr;
					for (next = buf.get(); next < lim;) {
						struct if_msghdr *ifm = (struct if_msghdr *)next;
						next += ifm->ifm_msglen;
						struct if_data ifm_data = ifm->ifm_data;
						if (ifm->ifm_addrs & RTA_IFP) {
							struct sockaddr_dl *sdl = (struct sockaddr_dl *)(ifm + 1);
							char iface[32];
							strncpy(iface, sdl->sdl_data, sdl->sdl_nlen);
							iface[sdl->sdl_nlen] = 0;
							ifstats[iface] = std::tuple(ifm_data.ifi_ibytes, ifm_data.ifi_obytes);
						}
					}
				}
			}

			//? Get total received and transmitted bytes + device address if no ip was found
			for (const auto &iface : interfaces) {
				for (const string dir : {"download", "upload"}) {
					auto &saved_stat = net.at(iface).stat.at(dir);
					auto &bandwidth = net.at(iface).bandwidth.at(dir);
					uint64_t val = dir == "download" ? std::get<0>(ifstats[iface]) : std::get<1>(ifstats[iface]);

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
						if (saved_stat.speed > graph_max[dir]) {
							++max_count[dir][0];
							if (max_count[dir][1] > 0) --max_count[dir][1];
						} else if (graph_max[dir] > 10 << 10 and saved_stat.speed < graph_max[dir] / 10) {
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
			if (not config_iface.empty() and v_contains(interfaces, config_iface))
				selected_iface = config_iface;
			else {
				//? Sort interfaces by total upload + download bytes
				auto sorted_interfaces = interfaces;
				rng::sort(sorted_interfaces, [&](const auto &a, const auto &b) {
					return cmp_greater(net.at(a).stat["download"].total + net.at(a).stat["upload"].total,
									   net.at(b).stat["download"].total + net.at(b).stat["upload"].total);
				});
				selected_iface.clear();
				//? Try to set to a connected interface
				for (const auto &iface : sorted_interfaces) {
					if (net.at(iface).connected) selected_iface = iface;
					break;
				}
				//? If no interface is connected set to first available
				if (selected_iface.empty() and not sorted_interfaces.empty())
					selected_iface = sorted_interfaces.at(0);
				else if (sorted_interfaces.empty())
					return empty_net;
			}
		}

		//? Calculate max scale for graphs if needed
		if (net_auto) {
			bool sync = false;
			for (const auto &dir : {"download", "upload"}) {
				for (const auto &sel : {0, 1}) {
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
}  // namespace Net

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

	string get_status(char s) {
		if (s & SACTIVE) return "Running";
		if (s & SSTOP) return "Stopped";
		if (s & SIDL) return "Idle";
		if (s & SZOMB) return "Zombie";
		return "Unknown";
	}

	//* Get detailed info for selected process
	void _collect_details(const size_t pid, vector<proc_info> &procs) {
		if (pid != detailed.last_pid) {
			detailed = {};
			detailed.last_pid = pid;
			detailed.skip_smaps = not Config::getB("proc_info_smaps");
		}
		const auto width = get_width();

		//? Copy proc_info for process from proc vector
		auto p_info = rng::find(procs, pid, &proc_info::pid);
		detailed.entry = *p_info;

		//? Update cpu percent deque for process cpu graph
		if (not Config::getB("proc_per_core")) detailed.entry.cpu_p *= Shared::coreCount;
		detailed.cpu_percent.push_back(clamp((long long)round(detailed.entry.cpu_p), 0ll, 100ll));
		while (cmp_greater(detailed.cpu_percent.size(), width)) detailed.cpu_percent.pop_front();

		//? Process runtime : current time - start time (both in unix time - seconds since epoch)
		struct timeval currentTime;
		gettimeofday(&currentTime, nullptr);
		detailed.elapsed = sec_to_dhms(currentTime.tv_sec - detailed.entry.cpu_s); // only interested in second granularity, so ignoring tc_usec
		if (detailed.elapsed.size() > 8) detailed.elapsed.resize(detailed.elapsed.size() - 3);

		//? Get parent process name
		if (detailed.parent.empty()) {
			auto p_entry = rng::find(procs, detailed.entry.ppid, &proc_info::pid);
			if (p_entry != procs.end()) detailed.parent = p_entry->name;
		}

		//? Expand process status from single char to explanative string
		detailed.status = get_status(detailed.entry.state);

		detailed.mem_bytes.push_back(detailed.entry.mem);
		detailed.memory = floating_humanizer(detailed.entry.mem);

		if (detailed.first_mem == -1 or detailed.first_mem < detailed.mem_bytes.back() / 2 or detailed.first_mem > detailed.mem_bytes.back() * 4) {
			detailed.first_mem = min((uint64_t)detailed.mem_bytes.back() * 2, Mem::get_totalMem());
			set_redraw(true);
		}

		while (cmp_greater(detailed.mem_bytes.size(), width)) detailed.mem_bytes.pop_front();

		// rusage_info_current rusage;
		// if (proc_pid_rusage(pid, RUSAGE_INFO_CURRENT, (void **)&rusage) == 0) {
		// 	// this fails for processes we don't own - same as in Linux
		// 	detailed.io_read = floating_humanizer(rusage.ri_diskio_bytesread);
		// 	detailed.io_write = floating_humanizer(rusage.ri_diskio_byteswritten);
		// }
	}

	//* Collects and sorts process information from /proc
	auto collect(bool no_update) -> vector<proc_info> & {
		const auto sorting = Config::getS("proc_sorting");
		const auto reverse = Config::getB("proc_reversed");
		const auto filter = Config::getS("proc_filter");
		const auto per_core = Config::getB("proc_per_core");
		const auto tree = Config::getB("proc_tree");
		const auto show_detailed = Config::getB("show_detailed");
		const size_t detailed_pid = Config::getI("detailed_pid");
		bool should_filter = current_filter != filter;
		if (should_filter) current_filter = filter;
		bool sorted_change = (sorting != current_sort or reverse != current_rev or should_filter);
		if (sorted_change) {
			current_sort = sorting;
			current_rev = reverse;
		}

		const int cmult = (per_core) ? Shared::coreCount : 1;
		bool got_detailed = false;

		static vector<size_t> found;

		vector<array<long, CPUSTATES>> cpu_time(Shared::coreCount);
		size_t size = sizeof(long) * CPUSTATES * Shared::coreCount;
		if (sysctlbyname("kern.cp_times", &cpu_time[0], &size, nullptr, 0) == -1) {
			Logger::error("failed to get CPU times");
		}
		cputimes = 0;
		for (const auto core : cpu_time) {
			for (const unsigned int c_state : {CP_USER, CP_NICE, CP_SYS, CP_IDLE}) {
				cputimes += core[c_state];
			}
		}

		//* Use pids from last update if only changing filter, sorting or tree options
		if (no_update and not current_procs.empty()) {
			if (show_detailed and detailed_pid != detailed.last_pid) _collect_details(detailed_pid, current_procs);
		} else {
			//* ---------------------------------------------Collection start----------------------------------------------

			should_filter = true;
			found.clear();
			struct timeval currentTime;
			gettimeofday(&currentTime, nullptr);
			const double timeNow = currentTime.tv_sec + (currentTime.tv_usec / 1'000'000);

			int count = 0;
			char buf[_POSIX2_LINE_MAX];
			Shared::KvmPtr kd {kvm_openfiles(nullptr, _PATH_DEVNULL, nullptr, O_RDONLY, buf)};
   			const struct kinfo_proc* kprocs = kvm_getprocs(kd.get(), KERN_PROC_ALL, 0, &count);

   			for (int i = 0; i < count; i++) {
	  			const struct kinfo_proc* kproc = &kprocs[i];
				const size_t pid = (size_t)kproc->kp_pid;
				if (pid < 1) continue;
				found.push_back(pid);

				//? Check if pid already exists in current_procs
				bool no_cache = false;
				auto find_old = rng::find(current_procs, pid, &proc_info::pid);
				if (find_old == current_procs.end()) {
					current_procs.push_back({pid});
					find_old = current_procs.end() - 1;
					no_cache = true;
				}

				auto &new_proc = *find_old;

				//? Get program name, command, username, parent pid, nice and status
				if (no_cache) {
					if (string(kproc->kp_comm) == "idle"s) {
						current_procs.pop_back();
						found.pop_back();
						continue;
					}
					new_proc.name = kproc->kp_comm;
					char** argv = kvm_getargv(kd.get(), kproc, 0);
					if (argv) {
						for (int i = 0; argv[i] and cmp_less(new_proc.cmd.size(), 1000); i++) {
							new_proc.cmd += argv[i] + " "s;
						}
						if (not new_proc.cmd.empty()) new_proc.cmd.pop_back();
					}
					if (new_proc.cmd.empty()) new_proc.cmd = new_proc.name;
					if (new_proc.cmd.size() > 1000) {
						new_proc.cmd.resize(1000);
						new_proc.cmd.shrink_to_fit();
					}
					new_proc.ppid = kproc->kp_ppid;
					new_proc.cpu_s = round(kproc->kp_start.tv_sec);
					struct passwd *pwd = getpwuid(kproc->kp_uid);
					if (pwd)
						new_proc.user = pwd->pw_name;
				}
				new_proc.p_nice = kproc->kp_nice;
				new_proc.state = kproc->kp_stat;

				int cpu_t = 0;
				cpu_t 	= kproc->kp_ru.ru_utime.tv_sec * 1'000'000 + kproc->kp_ru.ru_utime.tv_usec
						+ kproc->kp_ru.ru_stime.tv_sec * 1'000'000 + kproc->kp_ru.ru_stime.tv_usec;

				new_proc.mem = kproc->kp_vm_rssize * Shared::pageSize;
				new_proc.threads = kproc->kp_nthreads;

				//? Process cpu usage since last update
				new_proc.cpu_p = clamp((100.0 * kproc->kp_lwp.kl_pctcpu / Shared::kfscale) * cmult, 0.0, 100.0 * Shared::coreCount);

				//? Process cumulative cpu usage since process start
				new_proc.cpu_c = (double)(cpu_t * Shared::clkTck / 1'000'000) / max(1.0, timeNow - new_proc.cpu_s);

				//? Update cached value with latest cpu times
				new_proc.cpu_t = cpu_t;

				if (show_detailed and not got_detailed and new_proc.pid == detailed_pid) {
					got_detailed = true;
				}
			}

			//? Clear dead processes from current_procs
			current_procs |= rng::actions::remove_if([&](const auto &element) { return not v_contains(found, element.pid); });

			//? Update the details info box for process if active
			if (show_detailed and got_detailed) {
				_collect_details(detailed_pid, current_procs);
			} else if (show_detailed and not got_detailed and detailed.status != "Dead") {
				detailed.status = "Dead";
				set_redraw(true);
			}

			old_cputimes = cputimes;

		}

		//* ---------------------------------------------Collection done-----------------------------------------------

		//* Match filter if defined
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

		//* Sort processes
		if (sorted_change or not no_update) {
			proc_sorter(current_procs, sorting, reverse, tree);
		}

		//* Generate tree view if enabled
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

			//? Stable sort to retain selected sorting among processes with the same parent
			rng::stable_sort(current_procs, rng::less{}, & proc_info::ppid);

			//? Start recursive iteration over processes with the lowest shared parent pids
			for (auto& p : rng::equal_range(current_procs, current_procs.at(0).ppid, rng::less{}, &proc_info::ppid)) {
				_tree_gen(p, current_procs, tree_procs, 0, false, filter, false, no_update, should_filter);
			}

			//? Recursive sort over tree structure to account for collapsed processes in the tree
			int index = 0;
			tree_sort(tree_procs, sorting, reverse, index, current_procs.size());

			//? Add tree begin symbol to first item if childless
			if (tree_procs.front().children.empty())
				tree_procs.front().entry.get().prefix.replace(tree_procs.front().entry.get().prefix.size() - 8, 8, " ┌─ ");

			//? Add tree terminator symbol to last item if childless
			if (tree_procs.back().children.empty())
				tree_procs.back().entry.get().prefix.replace(tree_procs.back().entry.get().prefix.size() - 8, 8, " └─ ");

			//? Final sort based on tree index
			rng::sort(current_procs, rng::less{}, & proc_info::tree_index);

			//? Move current selection/view to the selected process when collapsing/expanding in the tree
			if (locate_selection) {
				int loc = rng::find(current_procs, Proc::get_selected_pid(), &proc_info::pid)->tree_index;
				if (config_ints.at("proc_start") >= loc or config_ints.at("proc_start") <= loc - Proc::get_select_max())
					Config::ints_set_at("proc_start", max(0, loc - 1));
				Config::ints_set_at("proc_selected", loc - config_ints.at("proc_start") + 1);
			}
		}

		numpids = (int)current_procs.size() - filter_found;
		return current_procs;
	}
}  // namespace Proc

namespace Tools {
	double system_uptime() {
		struct timeval ts, currTime;
		std::size_t len = sizeof(ts);
		int mib[2] = {CTL_KERN, KERN_BOOTTIME};
		if (sysctl(mib, 2, &ts, &len, nullptr, 0) != -1) {
			gettimeofday(&currTime, nullptr);
			return currTime.tv_sec - ts.tv_sec;
		}
		return 0.0;
	}
}  // namespace Tools
