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

#pragma once

#include <array>
#include <atomic>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <vector>
#include <unordered_map>

#ifndef _WIN32
#include <unistd.h>

// From `man 3 getifaddrs`: <net/if.h> must be included before <ifaddrs.h>
// clang-format off
#include <net/if.h>
#include <ifaddrs.h>
// clang-format on

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
# include <kvm.h>
#endif
#endif // !_WIN32

using std::array;
using std::atomic;
using std::deque;
using std::string;
using std::tuple;
using std::vector;

using namespace std::literals; // for operator""s

void term_resize(bool force=false);
void banner_gen();

extern void clean_quit(int sig);

namespace Global {
	extern const vector<array<string, 4>> Banner_src;
	extern const string Version;
	extern atomic<bool> quitting;
	extern string exit_error_msg;
	extern atomic<bool> thread_exception;
	extern string banner;
	extern atomic<bool> resized;
	extern string overlay;
	extern string clock;

#ifndef _WIN32
	extern uid_t real_uid, set_uid;
#endif

	extern atomic<bool> init_conf;

	bool get_quitting();
}

namespace Runner {

	extern atomic<bool> active;
	extern atomic<bool> reading;
	extern atomic<bool> stopping;
	extern atomic<bool> redraw;
	extern atomic<bool> coreNum_reset;

#ifndef _WIN32
	extern pthread_t runner_id;
#endif

	extern bool pause_output;
	extern string debug_bg;

	bool get_stopping();
	bool get_coreNum_reset();
	void set_coreNum_reset(bool coreNum_reset);
	void active_atomic_wait();

	void run(const string& box="", bool no_update = false, bool force_redraw = false);
	void stop();

}

namespace Tools {
	//* Platform specific function for system_uptime (seconds since last restart)
	double system_uptime();
}

namespace Shared {
	//* Initialize platform specific needed variables and check for errors
	void init();

	//* Tear down GPU, etc.
	bool shutdown();

	extern long coreCount, page_size, clk_tck;

	long get_coreCount();

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	struct KvmDeleter {
		void operator()(kvm_t* handle) {
			kvm_close(handle);
		}
	};
	using KvmPtr = std::unique_ptr<kvm_t, KvmDeleter>;
#endif
}


namespace Npu {
	extern int width;
	extern int shown;
	extern int count;
	extern vector<int> shown_panels;
	extern vector<string> npu_names;
	extern vector<int> npu_b_height_offsets;
	extern std::unordered_map<string, deque<long long>> shared_npu_percent; // averages

	int get_count();

	vector<string>& get_npu_names();
	vector<int>& get_npu_b_height_offsets();

	int get_width();

	std::unordered_map<string, deque<long long>>& get_shared_npu_percent();

	//* Container for supported Npu::*::collect() functions
	struct npu_info_supported {
		bool npu_utilization = true;
	};

	//* Per-device container for NPU info
	struct npu_info {
		std::unordered_map<string, deque<long long>> npu_percent = {
			{"npu-totals", {}},
		};

		npu_info_supported supported_functions;
	};

	//* Collect npu stats
	auto collect(bool no_update = false) -> vector<npu_info>&;

	//* Draw contents of npu box using <npus> as source
	string draw(const npu_info& npu, unsigned long index, bool force_redraw, bool data_same);
}


namespace Gpu {
	extern vector<string> box;
	extern int width, height, min_width, min_height;
	extern vector<int> x_vec, y_vec;
	extern vector<bool> redraw;
	extern int shown;
	extern int count;
	extern vector<int> shown_panels;
	extern vector<string> gpu_names;
	extern vector<int> gpu_b_height_offsets;
	extern long long gpu_pwr_total_max;

	int get_count();
	vector<string>& get_gpu_names();
	vector<int>& get_gpu_b_height_offsets();

	int get_width();

	extern std::unordered_map<string, deque<long long>> shared_gpu_percent; // averages, power/vram total

	std::unordered_map<string, deque<long long>>& get_shared_gpu_percent();

	const array mem_names { "used"s, "free"s };

	//* Container for supported Gpu::*::collect() functions
	struct gpu_info_supported {
		bool gpu_utilization = true,
			 mem_utilization = true,
			 gpu_clock = true,
			 mem_clock = true,
			 pwr_usage = true,
			 pwr_state = true,
			 temp_info = true,
			 mem_total = true,
			 mem_used = true,
			 pcie_txrx = true;
	};

	//* Per-device container for GPU info
	struct gpu_info {
		std::unordered_map<string, deque<long long>> gpu_percent = {
			{"gpu-totals", {}},
			{"gpu-vram-totals", {}},
			{"gpu-pwr-totals", {}},
		};
		unsigned int gpu_clock_speed; // MHz

		long long pwr_usage; // mW
		long long pwr_max_usage = 255000;
		long long pwr_state;

		deque<long long> temp = {0};
		long long temp_max = 110;

		long long mem_total = 0;
		long long mem_used = 0;
		deque<long long> mem_utilization_percent = {0}; // TODO: properly handle GPUs that can't report some stats
		long long mem_clock_speed = 0; // MHz

		long long pcie_tx = 0; // KB/s
		long long pcie_rx = 0;

		gpu_info_supported supported_functions;

		// vector<proc_info> graphics_processes = {}; // TODO
		// vector<proc_info> compute_processes = {};
	};

	//* Collect gpu stats and temperatures
	auto collect(bool no_update = false) -> vector<gpu_info>&;

	//* Draw contents of gpu box using <gpus> as source
  	string draw(const gpu_info& gpu, unsigned long index, bool force_redraw, bool data_same);
}

namespace Cpu {
	extern bool shown, redraw, got_sensors, cpu_temp_only, has_battery;
	extern string cpuName, cpuHz;
	extern vector<string> available_fields;
	extern vector<string> available_sensors;
	extern tuple<int, float, long, string> current_bat;
	bool get_has_battery();
	bool get_got_sensors();
	bool get_cpu_temp_only();
	string get_cpuName();
	vector<string>& get_available_fields();
	vector<string>& get_available_sensors();
	tuple<int, float, long, string>& get_current_bat();

	extern string box;
	extern int x, y, width, height, min_width, min_height;

	int get_width();

	struct cpu_info {
		std::unordered_map<string, deque<long long>> cpu_percent = {
			{"total", {}},
			{"user", {}},
			{"nice", {}},
			{"system", {}},
			{"idle", {}},
			{"iowait", {}},
			{"irq", {}},
			{"softirq", {}},
			{"steal", {}},
			{"guest", {}},
			{"guest_nice", {}},
			{"dpc", {}}
		};
		vector<deque<long long>> core_percent;
		vector<deque<long long>> temp;
		long long temp_max = 0;
		array<double, 3> load_avg;
	};

	//* Collect cpu stats and temperatures
	auto collect(bool no_update = false) -> cpu_info&;

	//* Draw contents of cpu box using <cpu> as source
	string draw(const cpu_info& cpu, const vector<Gpu::gpu_info>& gpu, const vector<Npu::npu_info>& npu, bool force_redraw = false, bool data_same = false);

	//* Parse /proc/cpu info for mapping of core ids
	auto get_core_mapping() -> std::unordered_map<int, int>;
	extern std::unordered_map<int, int> core_mapping;
	auto update_core_mapping() -> bool;

	auto get_cpuHz() -> string;

	//* Get battery info from /sys
	auto get_battery() -> tuple<int, float, long, string>;

#ifdef _WIN32
	struct GpuRaw {
		uint64_t usage = 0;
		uint64_t mem_total = 0;
		uint64_t mem_used = 0;
		uint64_t temp = 0;
		bool cpu_gpu = false;
		uint64_t clock_mhz = 0;
	};

	struct OHMRraw {
		std::unordered_map<string, GpuRaw> GPUS;
		vector<int> CPU;
		int CpuClock = 0;
	};
#endif
}

namespace Mem {
	extern string box;
	extern int x, y, width, height, min_width, min_height;
	extern bool has_swap, shown, redraw;
	const array mem_names { "used"s, "available"s, "cached"s, "free"s };
	const array swap_names { "swap_used"s, "swap_free"s };
	extern int disk_ios;

	bool get_has_swap();
	int get_disk_ios();

	int get_width();
	void set_redraw(bool val);

	struct disk_info {
		std::filesystem::path dev;
		string name;
		string fstype{};                // defaults to ""
		std::filesystem::path stat{};   // defaults to ""
		int64_t total{};
		int64_t used{};
		int64_t free{};
		int used_percent{};
		int free_percent{};

		array<int64_t, 3> old_io = {0, 0, 0};
		deque<long long> io_read = {};
		deque<long long> io_write = {};
		deque<long long> io_activity = {};
	};

	struct mem_info {
		std::unordered_map<string, uint64_t> stats =
			{{"used", 0}, {"available", 0}, {"commit", 0}, {"commit_total", 0}, {"cached", 0}, {"free", 0},
			{"swap_total", 0}, {"swap_used", 0}, {"swap_free", 0}};
		std::unordered_map<string, deque<long long>> percent =
			{{"used", {}}, {"available", {}}, {"commit", {}}, {"cached", {}}, {"free", {}},
			{"swap_total", {}}, {"swap_used", {}}, {"swap_free", {}}};
		std::unordered_map<string, disk_info> disks;
		vector<string> disks_order;
	};

	//?* Get total system memory
	uint64_t get_totalMem();

	//* Collect mem & disks stats
	auto collect(bool no_update = false) -> mem_info&;

	//* Draw contents of mem box using <mem> as source
	string draw(const mem_info& mem, bool force_redraw = false, bool data_same = false);

}

namespace Net {
	extern string box;
	extern int x, y, width, height, min_width, min_height;
	extern bool shown, redraw;
	extern string selected_iface;
	extern vector<string> interfaces;
	extern bool rescale;
	extern std::unordered_map<string, uint64_t> graph_max;

	string get_selected_iface();
	void set_selected_iface(const string& iface);
	vector<string>& get_interfaces();
	std::unordered_map<string, uint64_t>& get_graph_max();
	void set_rescale(bool rescale);

	int get_width();
	void set_redraw(bool val);

	struct net_stat {
		uint64_t speed{};
		uint64_t top{};
		uint64_t total{};
		uint64_t last{};
		uint64_t offset{};
		uint64_t rollover{};
	};

	struct net_info {
		std::unordered_map<string, deque<long long>> bandwidth = { {"download", {}}, {"upload", {}} };
		std::unordered_map<string, net_stat> stat = { {"download", {}}, {"upload", {}} };
		string ipv4{};      // defaults to ""
		string ipv6{};      // defaults to ""
		bool connected{};
	};

#ifndef _WIN32
	class IfAddrsPtr {
		struct ifaddrs* ifaddr;
		int status;
	public:
		IfAddrsPtr() { status = getifaddrs(&ifaddr); }
		~IfAddrsPtr() { freeifaddrs(ifaddr); }
		[[nodiscard]] constexpr auto operator()() -> struct ifaddrs* { return ifaddr; }
		[[nodiscard]] constexpr auto get() -> struct ifaddrs* { return ifaddr; }
		[[nodiscard]] constexpr auto get_status() const noexcept -> int { return status; };
	};
#endif // !_WIN32

	extern std::unordered_map<string, net_info> current_net;
	std::unordered_map<string, net_info>& get_current_net();

	//* Collect net upload/download stats
	auto collect(bool no_update=false) -> net_info&;

	//* Draw contents of net box using <net> as source
	string draw(const net_info& net, bool force_redraw = false, bool data_same = false);
}

namespace Proc {
	extern atomic<int> numpids;
	int get_numpids();

	extern string box;
	extern int x, y, width, height, min_width, min_height;
	extern bool shown, redraw;
	extern int select_max_rows;
	extern atomic<int> detailed_pid;
	extern int selected_pid, start, selected, collapse, expand, filter_found, selected_depth;
	extern string selected_name;

	void set_collapse(int val);
	void set_expand(int val);
	void increment_filter_found();

	int get_width();
	void set_redraw(bool val);
	int get_selected_pid();
	int get_select_max();

	//? Contains the valid sorting options for processes
	const vector<string> sort_vector = {
		"pid",
		"name",
		"command",
		"threads",
		"user",
		"memory",
		"cpu direct",
		"cpu lazy",
	};

	//? Translation from process state char to explanative string
	const std::unordered_map<char, string> proc_states = {
		{'R', "Running"},
		{'S', "Sleeping"},
		{'D', "Waiting"},
		{'Z', "Zombie"},
		{'T', "Stopped"},
		{'t', "Tracing"},
		{'X', "Dead"},
		{'x', "Dead"},
		{'K', "Wakekill"},
		{'W', "Unknown"},
		{'P', "Parked"}
	};

	//* Container for process information
	struct proc_info {
		size_t pid{};
		string name{};          // defaults to ""
		string cmd{};           // defaults to ""
		string short_cmd{};     // defaults to ""
		size_t threads{};
		int name_offset{};
		string user{};          // defaults to ""
		uint64_t mem{};
		double cpu_p{};         // defaults to = 0.0
		double cpu_c{};         // defaults to = 0.0
		char state = '0';
		int64_t p_nice{};
		uint64_t ppid{};
		uint64_t cpu_s{};
		uint64_t cpu_t{};
		string prefix{};        // defaults to ""
		size_t depth{};
		size_t tree_index{};
		bool collapsed{};
		bool filtered{};
#ifdef _WIN32
		bool WMI = false;
#else
		std::optional<bool> WMI;
#endif
	};

	//* Container for process info box
	struct detail_container {
		size_t last_pid{};
		bool skip_smaps{};
		proc_info entry;
		string elapsed, parent, status, io_read, io_write, memory;
#ifdef _WIN32
		string owner, start, description, last_name, service_type;
#else
		std::optional<string> owner, start, description, last_name, service_type;
#endif
		long long first_mem = -1;
		deque<long long> cpu_percent;
		deque<long long> mem_bytes;
#ifdef _WIN32
		double mem_percent = 0.0;
		bool can_pause = false;
		bool can_stop = false;
#else
		std::optional<double> mem_percent;
		std::optional<bool> can_pause;
		std::optional<bool> can_stop;
#endif
	};

	//? Contains all info for proc detailed box
	extern detail_container detailed;
	detail_container get_detailed();

	//* Collect and sort process information from /proc
	auto collect(bool no_update = false) -> vector<proc_info>&;

	//* Update current selection and view, returns -1 if no change otherwise the current selection
	int selection(const string& cmd_key);

	//* Draw contents of proc box using <plist> as data source
	string draw(const vector<proc_info>& plist, bool force_redraw = false, bool data_same = false);

	struct tree_proc {
		std::reference_wrapper<proc_info> entry;
		vector<tree_proc> children;
	};

	//* Sort vector of proc_info's
	void proc_sorter(vector<proc_info>& proc_vec, const string& sorting, bool reverse, bool tree = false);

	//* Recursive sort of process tree
	void tree_sort(vector<tree_proc>& proc_vec, const string& sorting,
				   bool reverse, int& c_index, const int index_max, bool collapsed = false);

	bool matches_filter(const proc_info& proc, const std::string& filter);

	//* Generate process tree list
	void _tree_gen(proc_info& cur_proc, vector<proc_info>& in_procs, vector<tree_proc>& out_procs,
				   int cur_depth, bool collapsed, const string& filter,
				   bool found = false, bool no_update = false, bool should_filter = false);
}
