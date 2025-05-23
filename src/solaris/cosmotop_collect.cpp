/* Copyright 2025 Your Name (your@email.com)
   Licensed under the Apache License, Version 2.0 */
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
	int disk_ios = 0;
	bool has_swap = false;
	mem_info current_mem{};
}

namespace Shared {
	uint64_t totalMem;
	long pageSize, clkTck, coreCount, bootTime;

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

		// Initialize CPU structures
		Cpu::current_cpu.core_percent.insert(Cpu::current_cpu.core_percent.begin(), coreCount, {});
		Cpu::core_old_totals.insert(Cpu::core_old_totals.begin(), coreCount, 0);
		Cpu::core_old_idles.insert(Cpu::core_old_idles.begin(), coreCount, 0);

		// Initialize other components
		Cpu::collect();
		Cpu::cpuName = Cpu::get_cpuName();
		Cpu::got_sensors = Cpu::get_sensors();

		Mem::old_uptime = system_uptime();
		Mem::collect();
	}
}

namespace Cpu {
	bool get_sensors() {
		kstat_ctl_t *kc = kstat_open();
		if (!kc) return false;

		kstat_t *ks = kstat_lookup(kc, "cpu_info", -1, NULL);
		if (ks && kstat_read(kc, ks, NULL) != -1) {
			got_sensors = true;
			// Try to get temperature if available
			kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ks, "temperature");
			if (kn) {
				current_cpu.temp_max = kn->value.i32;
			}
		}

		kstat_close(kc);
		return got_sensors;
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

	void update_sensors() {
		kstat_ctl_t *kc = kstat_open();
		if (!kc) return;

		for (int i = 0; i < Shared::coreCount; i++) {
			kstat_t *ks = kstat_lookup(kc, "cpu_temp", i, NULL);
			if (ks && kstat_read(kc, ks, NULL) != -1) {
				kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ks, "temperature");
				if (kn) {
					int temp = kn->value.i32;
					if (i < current_cpu.temp.size()) {
						current_cpu.temp.at(i).push_back(temp);
						if (current_cpu.temp.at(i).size() > 20)
							current_cpu.temp.at(i).pop_front();
					}
				}
			}
		}
		kstat_close(kc);
	}

	string get_cpuHz() {
		kstat_ctl_t *kc = kstat_open();
		if (!kc) return "";

		kstat_t *ks = kstat_lookup(kc, "cpu_info", 0, NULL);
		if (ks && kstat_read(kc, ks, NULL) != -1) {
			kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ks, "current_clock_Hz");
			if (kn) {
				kstat_close(kc);
				return to_string(kn->value.ui32 / 1e6).substr(0, 4);
			}
		}
		kstat_close(kc);
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
		kstat_ctl_t *kc = kstat_open();
		if (!kc) return current_cpu;

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

					if (current_cpu.core_percent.at(i).size() > 40)
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

		// Get load averages
		double loadavg[3];
		if (getloadavg(loadavg, 3) == 3) {
			for (size_t i = 0; i < min(current_cpu.load_avg.size(), (size_t)3); i++) {
				current_cpu.load_avg[i] = loadavg[i];
			}
		}

		kstat_close(kc);
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
		// Solaris disk stats via kstat
		kstat_ctl_t *kc = kstat_open();
		if (!kc) return;

		for (kstat_t *ks = kc->kc_chain; ks; ks = ks->ks_next) {
			if (strncmp(ks->ks_module, "sd", 2) == 0 ||
				strncmp(ks->ks_module, "ssd", 3) == 0) {
				if (kstat_read(kc, ks, NULL) == -1) continue;

				kstat_named_t *kn;
				string dev = string(ks->ks_name);

				if (mapping.count(dev)) {
					auto &disk = disks[mapping[dev]];

					// Read operations
					kn = (kstat_named_t *)kstat_data_lookup(ks, "reads");
					long reads = kn ? kn->value.ui32 : 0;

					// Write operations
					kn = (kstat_named_t *)kstat_data_lookup(ks, "writes");
					long writes = kn ? kn->value.ui32 : 0;

					// Bytes read/written
					kn = (kstat_named_t *)kstat_data_lookup(ks, "nread");
					uint64_t nread = kn ? kn->value.ui32 : 0;

					kn = (kstat_named_t *)kstat_data_lookup(ks, "nwritten");
					uint64_t nwritten = kn ? kn->value.ui32 : 0;

					assign_values(disk, nread, nwritten);
				}
			}
		}
		kstat_close(kc);
	}

	auto collect(bool no_update) -> mem_info & {
		kstat_ctl_t *kc = kstat_open();
		if (!kc) return current_mem;

		// Get memory stats
		kstat_t *ks = kstat_lookup(kc, "unix", 0, "system_pages");
		if (ks && kstat_read(kc, ks, NULL) != -1) {
			kstat_named_t *kn;

			kn = (kstat_named_t *)kstat_data_lookup(ks, "freemem");
			current_mem.stats["free"] = kn->value.ul * Shared::pageSize;

			kn = (kstat_named_t *)kstat_data_lookup(ks, "physmem");
			uint64_t physmem = kn->value.ul * Shared::pageSize;

			kn = (kstat_named_t *)kstat_data_lookup(ks, "pages_locked");
			current_mem.stats["used"] = kn->value.ul * Shared::pageSize;

			current_mem.stats["available"] = physmem - current_mem.stats["used"];
		}

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
				uint64_t total = 0, used = 0;
				for (int i = 0; i < nswap; i++) {
					total += swp[i].ste_pages;
					used += swp[i].ste_free;
					free(swp[i].ste_path);
				}
				current_mem.stats["swap_total"] = total * Shared::pageSize;
				current_mem.stats["swap_used"] = (total - used) * Shared::pageSize;
			}
			free(swt);
		}

		kstat_close(kc);
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
		kstat_ctl_t *kc = kstat_open();
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

		kstat_close(kc);
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
		kstat_ctl_t *kc = kstat_open();
		if (!kc) return 0.0;

		kstat_t *ks = kstat_lookup(kc, "unix", 0, "system_misc");
		if (ks && kstat_read(kc, ks, NULL) != -1) {
			kstat_named_t *kn = (kstat_named_t *)kstat_data_lookup(ks, "boot_time");
			if (kn) {
				time_t now = time(NULL);
				kstat_close(kc);
				return difftime(now, kn->value.ui32);
			}
		}
		kstat_close(kc);
		return 0.0;
	}
}
