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

#include <kstat.h>
#include <sys/statvfs.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <sys/loadavg.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <sys/processor.h>
#include <sys/pset.h>
#include <sys/systeminfo.h>
#include <libdevinfo.h>
#include <libnvpair.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <procfs.h>
#include <libproc.h>
#include <sys/resource.h>
#include <sys/zone.h>
#include <sys/mnttab.h>
#include <sys/mntent.h>
#include <stropts.h>
#include <utmpx.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>

#include <stdexcept>
#include <cmath>
#include <fstream>
#include <numeric>
#include <regex>
#include <string>
#include <memory>
#include <utility>
#include <list>

#include <range/v3/all.hpp>

#include "../cosmotop_config.hpp"
#include "../cosmotop_shared.hpp"
#include "../cosmotop_tools.hpp"

#include <iostream>

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

	const array<string, 10> time_names = {"user", "nice", "system", "idle"};

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

	//* Populate found_sensors map
	bool get_sensors();

	//* Get current cpu clock speed
	string get_cpuHz();

	//* Search sysinfo for a cpu name
	string get_cpuName();
}

namespace Mem {
	double old_uptime;

	FILE *mnttab;
	int disk_ios = 0;
	vector<string> last_found;

	bool has_swap = false;

	mem_info current_mem;
}

namespace Shared {
	uint64_t totalMem;
	long pageSize, clkTck, coreCount, bootTime;

	kstat_ctl_t *kc;

	void init() {
		// Get CPU core count
		coreCount = sysconf(_SC_NPROCESSORS_ONLN);

		// Get page size
		pageSize = sysconf(_SC_PAGESIZE);
		if (pageSize <= 0) pageSize = 4096;

		// Get clock ticks per second
		clkTck = sysconf(_SC_CLK_TCK);
		if (clkTck <= 0) clkTck = 100;

		// Get total memory
		totalMem = sysconf(_SC_PHYS_PAGES) * pageSize;

		// Get boot time
		struct utmpx *utmpx;
		setutxent();
		while (utmpx = getutxent()) {
			if (utmpx->ut_type == BOOT_TIME) {
				bootTime = utmpx->ut_xtime;
				break;
			}
		}
		endutxent();

		// Initialize kstat
		kc = kstat_open();
		if (!kc) {
			Logger::error("Failed to initialize kstat: " + string(strerror(errno)));
		}

		// Initialize mnttab
		Mem::mnttab = fopen("/etc/mnttab", "r");
		if (!Mem::mnttab) {
			Logger::error("Failed to open mnttab: " + string(strerror(errno)));
		}

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

namespace Cpu {
	bool get_sensors() {
		// Does Solaris have temperature sensors?
		return false;
	}

	string get_cpuName() {
		char buf[256];
		if (sysinfo(SI_HW_PROVIDER, buf, sizeof(buf))) {
			string name = buf;
			sysinfo(SI_HW_SERIAL, buf, sizeof(buf));
			name += " " + string(buf);
			return name;
		}
		return "Unknown";
	}

	void update_sensors() {	}

	string get_cpuHz() {
		kstat_ctl_t *kc = Shared::kc;
		if (!kc) return "";

		kstat_t *ks = kstat_lookup(kc, "cpu_info", 0, NULL);
		if (ks && kstat_read(kc, ks, NULL) != -1) {
			kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ks, "current_clock_Hz");
			if (kn) {
				return to_string(kn->value.ui32 / 1e6).substr(0, 4);
			}
		}
		return "";
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

	auto collect(bool no_update) -> cpu_info & {
		kstat_ctl_t *kc = Shared::kc;
		if (Runner::get_stopping() or !kc or (no_update and not current_cpu.cpu_percent.at("total").empty()))
			return current_cpu;

		const auto width = get_width();

		if (getloadavg(current_cpu.load_avg.data(), current_cpu.load_avg.size()) < 0) {
			Logger::error("failed to get load averages");
		}

		// Get CPU usage from kstat
		cpu_stat_t cs;
		long long global_totals = 0;
		long long global_idles = 0;

		for (int i = 0; i < Shared::coreCount; i++) {
			kstat_t *ks = kstat_lookup(kc, "cpu_stat", i, NULL);
			if (ks && kstat_read(kc, ks, &cs) != -1) {
				long long totals = cs.cpu_sysinfo.cpu[CPU_USER] +
								 cs.cpu_sysinfo.cpu[CPU_KERNEL] +
								 cs.cpu_sysinfo.cpu[CPU_IDLE] +
								 cs.cpu_sysinfo.cpu[CPU_WAIT];
				long long idles = cs.cpu_sysinfo.cpu[CPU_IDLE];

				global_totals += totals;
				global_idles += idles;

				if (i < Shared::coreCount) {
					const long long calc_totals = max(0ll, totals - core_old_totals.at(i));
					const long long calc_idles = max(0ll, idles - core_old_idles.at(i));
					core_old_totals.at(i) = totals;
					core_old_idles.at(i) = idles;

					current_cpu.core_percent.at(i).push_back(
						clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

					while (cmp_greater(current_cpu.core_percent.at(i).size(), width * 2))
						current_cpu.core_percent.at(i).pop_front();
				}
			}
		}

		// Process global CPU stats
		const long long calc_totals = max(1ll, global_totals - cpu_old.at("totals"));
		const long long calc_idles = max(1ll, global_idles - cpu_old.at("idles"));

		cpu_old.at("totals") = global_totals;
		cpu_old.at("idles") = global_idles;

		current_cpu.cpu_percent.at("total").push_back(
			clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

		while (cmp_greater(current_cpu.cpu_percent.at("total").size(), width * 2))
			current_cpu.cpu_percent.at("total").pop_front();

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

	void collect_disk(std::unordered_map<string, disk_info> &disks,
					 std::unordered_map<string, string> &mapping) {
		// Need a way to get stats
		for (auto& [_, disk] : disks) {
			assign_values(disk, 0, 0);
		}
	}

	auto collect(bool no_update) -> mem_info & {
		kstat_ctl_t *kc = Shared::kc;
		if (Runner::get_stopping() or !kc or (no_update and not current_mem.percent.at("used").empty()))
			return current_mem;

		const auto show_swap = Config::getB("show_swap");
		const auto show_disks = Config::getB("show_disks");
		const auto swap_disk = Config::getB("swap_disk");
		const auto width = get_width();

		// Get memory stats
		kstat_t *ks = kstat_lookup(kc, "unix", 0, "system_pages");
		if (ks && kstat_read(kc, ks, NULL) != -1) {
			kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ks, "freemem");
			uint64_t free = kn->value.ui64 * Shared::pageSize;
			current_mem.stats["free"] = free;
			current_mem.stats["used"] = Shared::totalMem - free;
			current_mem.stats["available"] = free;
		}

		if (show_swap) {
			// Get swap info
			struct swaptable *swt;
			struct swapent *swp;
			int nswap;

			if (nswap = swapctl(SC_GETNSWP, NULL)) {
				swt = (struct swaptable *)malloc(sizeof(int) + nswap * sizeof(struct swapent));
				swt->swt_n = nswap;
				swp = swt->swt_ent;

				for (int i = 0; i < nswap; i++) {
					swp[i].ste_path = (char *)malloc(MAXPATHLEN);
					swp[i].ste_length = MAXPATHLEN;
				}

				if (swapctl(SC_LIST, swt) != -1) {
					uint64_t total = 0, free_swap = 0;
					for (int i = 0; i < nswap; i++) {
						total += swp[i].ste_pages;
						free_swap += swp[i].ste_free;
						free(swp[i].ste_path);
					}
					current_mem.stats["swap_total"] = total * Shared::pageSize;
					current_mem.stats["swap_free"] = free_swap * Shared::pageSize;
					current_mem.stats["swap_used"] = (total - free_swap) * Shared::pageSize;
				}
				free(swt);
			}
		}

		//? Calculate percentages
		for (const auto &name : mem_names) {
			current_mem.percent.at(name).push_back(round((double)current_mem.stats.at(name) * 100 / Shared::totalMem));
			while (cmp_greater(current_mem.percent.at(name).size(), width * 2))
				current_mem.percent.at(name).pop_front();
		}

		if (show_disks && mnttab) {
			std::unordered_map<string, string> mapping;
			double uptime = system_uptime();
			const auto disks_filter = Config::getS("disks_filter");
			bool filter_exclude = false;
			// auto only_physical = Config::getB("only_physical");
			auto &disks = current_mem.disks;
			vector<string> filter;
			if (not disks_filter.empty()) {
				filter = ssplit(disks_filter);
				if (filter.at(0).starts_with("exclude=")) {
					filter_exclude = true;
					filter.at(0) = filter.at(0).substr(8);
				}
			}

			struct mnttab mnt;
			vector<string> found;
			found.reserve(last_found.size());
			resetmnttab(mnttab);
			while (getmntent(mnttab, &mnt) == 0) {
				string fstype = mnt.mnt_fstype;
				static std::list<string> rejectList = {
					"autofs", "dev", "devfs", "ctfs", "proc", "mntfs",
					"fd", "tmpfs", "lofs", "objfs", "sharefs"
				};
				if (auto it = std::find(rejectList.begin(), rejectList.end(), fstype); it != rejectList.end()) {
					continue;
				}

				std::error_code ec;
				string mountpoint = mnt.mnt_mountp;
				string dev = mnt.mnt_special;
				mapping[dev] = mountpoint;

				//? Match filter if not empty
				if (not filter.empty()) {
					bool match = v_contains(filter, mountpoint);
					if ((filter_exclude and match) or (not filter_exclude and not match))
						continue;
				}

				found.push_back(mountpoint);
				if (not disks.contains(mountpoint)) {
					disks[mountpoint] = disk_info{std::filesystem::canonical(dev, ec), std::filesystem::path(mountpoint).filename()};

					if (disks.at(mountpoint).dev.empty())
						disks.at(mountpoint).dev = dev;

					if (disks.at(mountpoint).name.empty())
						disks.at(mountpoint).name = (mountpoint == "/" ? "root" : mountpoint);
				}


				if (not v_contains(last_found, mountpoint))
					set_redraw(true);
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
				if (std::error_code ec; not std::filesystem::exists(mountpoint, ec))
					continue;
				struct statvfs vfs;
				if (statvfs(mountpoint.c_str(), &vfs) < 0) {
					Logger::warning("Failed to get disk/partition stats with statvfs() for: " + mountpoint + " (" + string(strerror(errno)) + ")");
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
			current_mem.disks_order.clear();
			if (disks.contains("/"))
				current_mem.disks_order.push_back("/");
			if (swap_disk and has_swap) {
				current_mem.disks_order.push_back("swap");
				if (not disks.contains("swap"))
					disks["swap"] = {"", "swap"};
				disks.at("swap").total = current_mem.stats.at("swap_total");
				disks.at("swap").used = current_mem.stats.at("swap_used");
				disks.at("swap").free = current_mem.stats.at("swap_free");
				disks.at("swap").used_percent = current_mem.percent.at("swap_used").back();
				disks.at("swap").free_percent = current_mem.percent.at("swap_free").back();
			}
			for (const auto &name : last_found)
				if (not is_in(name, "/", "swap", "/dev"))
					current_mem.disks_order.push_back(name);

			disk_ios = 0;
			collect_disk(disks, mapping);

			old_uptime = uptime;
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
		kstat_ctl_t *kc = Shared::kc;
		if (!kc) return empty_net;

		interfaces.clear();

		// Get network interfaces
		for (kstat_t *ks = kc->kc_chain; ks; ks = ks->ks_next) {
			if (strcmp(ks->ks_class, "net") == 0) {
				string iface = ks->ks_name;
				interfaces.push_back(iface);

				if (kstat_read(kc, ks, NULL) == -1) continue;

				kstat_named_t *kn;
				net_info &ni = current_net[iface];

				// Get IP addresses (simplified)
				int s = socket(AF_INET, SOCK_DGRAM, 0);
				if (s >= 0) {
					struct lifreq ifr;
					strncpy(ifr.lifr_name, iface.c_str(), IFNAMSIZ);

					if (ioctl(s, SIOCGLIFADDR, &ifr) >= 0) {
						ni.ipv4 = inet_ntoa(((struct sockaddr_in *)&ifr.lifr_addr)->sin_addr);
					}
					close(s);
				}

				// Get stats
				kn = (kstat_named_t *)kstat_data_lookup(ks, "rbytes");
				uint64_t rbytes = kn ? kn->value.ui32 : 0;

				kn = (kstat_named_t *)kstat_data_lookup(ks, "obytes");
				uint64_t obytes = kn ? kn->value.ui32 : 0;

				// Update stats
				auto &saved_dl = ni.stat["download"];
				auto &saved_ul = ni.stat["upload"];

				if (rbytes < saved_dl.last) saved_dl.rollover += saved_dl.last;
				if (obytes < saved_ul.last) saved_ul.rollover += saved_ul.last;

				saved_dl.speed = (rbytes - saved_dl.last) / ((time_ms() - timestamp) / 1000);
				saved_ul.speed = (obytes - saved_ul.last) / ((time_ms() - timestamp) / 1000);

				saved_dl.last = rbytes;
				saved_ul.last = obytes;

				saved_dl.total = rbytes + saved_dl.rollover - saved_dl.offset;
				saved_ul.total = obytes + saved_ul.rollover - saved_ul.offset;

				ni.bandwidth["download"].push_back(saved_dl.speed);
				ni.bandwidth["upload"].push_back(saved_ul.speed);
			}
		}

		timestamp = time_ms();

		// Interface selection logic (same as original)
		// ...

		return current_net[selected_iface];
	}
}

namespace Proc {
	vector<proc_info> current_procs;
	int collapse = -1, expand = -1;
	atomic<int> numpids = 0;
	int filter_found = 0;
	detail_container detailed;

	auto collect(bool no_update) -> vector<proc_info> & {
		// Open /proc and scan for processes
		DIR *dirp = opendir("/proc");
		if (!dirp) return current_procs;

		struct dirent *dent;
		while ((dent = readdir(dirp))) {
			if (dent->d_name[0] < '0' || dent->d_name[0] > '9') continue;

			pid_t pid = atoi(dent->d_name);
			psinfo_t psinfo;
			char path[PATH_MAX];

			snprintf(path, sizeof(path), "/proc/%d/psinfo", pid);
			int fd = open(path, O_RDONLY);
			if (fd == -1) continue;

			if (read(fd, &psinfo, sizeof(psinfo)) == sizeof(psinfo)) {
				// Find or create proc_info
				auto p = rng::find(current_procs, pid, &proc_info::pid);
				if (p == current_procs.end()) {
					current_procs.push_back({pid});
					p = current_procs.end() - 1;
					p->name = psinfo.pr_fname;
					p->cmd = psinfo.pr_psargs;
					p->ppid = psinfo.pr_ppid;

					struct passwd *pwd = getpwuid(psinfo.pr_uid);
					if (pwd) {
						p->user = pwd->pw_name;
					}
				}

				// Update stats
				p->cpu_p = (100.0 * psinfo.pr_pctcpu / 0x8000) * (Config::getB("proc_per_core") ? 1 : Shared::coreCount);
				p->cpu_c = psinfo.pr_pctcpu;
				p->mem = psinfo.pr_rssize * 1024;
				p->state = psinfo.pr_lwp.pr_state;
				p->threads = psinfo.pr_nlwp;
				p->p_nice = psinfo.pr_lwp.pr_nice;
			}
			close(fd);
		}
		closedir(dirp);

		// Process filtering, sorting, tree view logic
		// (Same as original implementation)
		// ...

		return current_procs;
	}
}

namespace Tools {
	double system_uptime() {
		kstat_ctl_t *kc = Shared::kc;
		if (!kc) return 0.0;

		kstat_t *ks = kstat_lookup(kc, "unix", 0, "system_misc");
		if (ks && kstat_read(kc, ks, NULL) != -1) {
			kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ks, "boot_time");
			if (kn) {
				time_t now = time(NULL);
				return difftime(now, kn->value.ui32);
			}
		}
		return 0.0;
	}
}
