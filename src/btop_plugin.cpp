/* Copyright 2025 bjia56 (dev.bjia56@gmail.com)

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

#include "btop_shared.hpp"
#include "btop_tools.hpp"
#include "btop_config.hpp"

using std::unordered_map;

//#define BTOP_PLUGIN
//#define BTOP_PLUGIN_HOST
//#define GPU_SUPPORT

#ifdef BTOP_PLUGIN

Plugin* plugin = nullptr;

void plugin_initializer(Plugin* plugin) {
	::plugin = plugin;

#ifdef GPU_SUPPORT
	plugin->registerHandler<bool>("Gpu::Nvml::shutdown", std::function([]() {
		return Gpu::Nvml::shutdown();
	}));
	plugin->registerHandler<bool>("Gpu::Rsmi::shutdown", std::function([]() {
		return Gpu::Rsmi::shutdown();
	}));
	plugin->registerHandler<vector<Gpu::gpu_info>, bool>("Gpu::collect", std::function([](bool no_update) {
		return Gpu::collect(no_update);
	}));
	plugin->registerHandler<int>("Gpu::get_count", std::function([]() {
		return Gpu::count;
	}));
	plugin->registerHandler<vector<string>>("Gpu::get_gpu_names", std::function([]() {
		return Gpu::gpu_names;
	}));
	plugin->registerHandler<vector<int>>("Gpu::get_gpu_b_height_offsets", std::function([]() {
		return Gpu::gpu_b_height_offsets;
	}));
	plugin->registerHandler<unordered_map<string, deque<long long>>>("Gpu::get_shared_gpu_percent", std::function([]() {
		return Gpu::shared_gpu_percent;
	}));
#endif

	plugin->registerHandler<Cpu::cpu_info, bool>("Cpu::collect", std::function([](bool no_update) {
		return Cpu::collect(no_update);
	}));
	plugin->registerHandler<string>("Cpu::get_cpuHz", std::function([]() {
		return Cpu::cpuHz;
	}));
	plugin->registerHandler<bool>("Cpu::update_core_mapping", std::function([]() {
		Cpu::core_mapping = Cpu::get_core_mapping();
		return true;
	}));
	plugin->registerHandler<bool>("Cpu::get_has_battery", std::function([]() {
		return Cpu::has_battery;
	}));
	plugin->registerHandler<bool>("Cpu::get_got_sensors", std::function([]() {
		return Cpu::got_sensors;
	}));
	plugin->registerHandler<bool>("Cpu::get_cpu_temp_only", std::function([]() {
		return Cpu::cpu_temp_only;
	}));
	plugin->registerHandler<string>("Cpu::get_cpuName", std::function([]() {
		return Cpu::cpuName;
	}));
	plugin->registerHandler<vector<string>>("Cpu::get_available_fields", std::function([]() {
		return Cpu::available_fields;
	}));
	plugin->registerHandler<vector<string>>("Cpu::get_available_sensors", std::function([]() {
		return Cpu::available_sensors;
	}));
	plugin->registerHandler<tuple<int, float, long, string>>("Cpu::get_current_bat", std::function([]() {
		return Cpu::current_bat;
	}));

	plugin->registerHandler<Mem::mem_info, bool>("Mem::collect", std::function([](bool no_update) {
		return Mem::collect(no_update);
	}));
	plugin->registerHandler<uint64_t>("Mem::get_totalMem", std::function([]() {
		return Mem::get_totalMem();
	}));
	plugin->registerHandler<bool>("Mem::get_has_swap", std::function([]() {
		return Mem::has_swap;
	}));
	plugin->registerHandler<int>("Mem::get_disk_ios", std::function([]() {
		return Mem::disk_ios;
	}));

	plugin->registerHandler<Net::net_info, bool>("Net::collect", std::function([](bool no_update) {
		return Net::collect(no_update);
	}));
	plugin->registerHandler<string>("Net::get_selected_iface", std::function([]() {
		return Net::selected_iface;
	}));
	plugin->registerHandler<bool, string>("Net::set_selected_iface", std::function([](string iface) {
		Net::selected_iface = iface;
		return true;
	}));
	plugin->registerHandler<vector<string>>("Net::get_interfaces", std::function([]() {
		return Net::interfaces;
	}));
	plugin->registerHandler<unordered_map<string, uint64_t>>("Net::get_graph_max", std::function([]() {
		return Net::graph_max;
	}));
	plugin->registerHandler<bool>("Net::set_rescale", std::function([](bool rescale) {
		Net::rescale = rescale;
		return true;
	}));
	plugin->registerHandler<unordered_map<string, Net::net_info>>("Net::get_current_net", std::function([]() {
		return Net::current_net;
	}));

	plugin->registerHandler<vector<Proc::proc_info>, bool>("Proc::collect", std::function([](bool no_update) {
		return Proc::collect(no_update);
	}));
	plugin->registerHandler<int>("Proc::get_numpids", std::function([]() {
		return Proc::numpids.load();
	}));
	plugin->registerHandler<bool, int>("Proc::set_collapse", std::function([](int val) {
		Proc::collapse = val;
		return true;
	}));
	plugin->registerHandler<bool, int>("Proc::set_expand", std::function([](int val) {
		Proc::expand = val;
		return true;
	}));
	plugin->registerHandler<bool>("Proc::increment_filter_found", std::function([]() {
		Proc::filter_found++;
		return true;
	}));
	plugin->registerHandler<Proc::detail_container>("Proc::get_detailed", std::function([]() {
		return Proc::detailed;
	}));

	plugin->registerHandler<bool>("Shared::init", std::function([]() {
		Shared::init();
		return true;
	}));
	plugin->registerHandler<long>("Shared::get_coreCount", std::function([]() {
		return Shared::coreCount;
	}));

	plugin->registerHandler<double>("Tools::system_uptime", std::function([]() {
		return Tools::system_uptime();
	}));
}

namespace Config {
	std::unordered_map<std::string, int>& get_ints() {
		static auto result = plugin->call<std::unordered_map<std::string, int>>("Config::get_ints");
		return result;
	}
	void ints_set_at(const std::string_view name, const int value) {
		plugin->call<bool>("Config::ints_set_at", std::string(name), value);
	}
	bool getB(const std::string_view name) {
		return plugin->call<bool>("Config::getB", std::string(name));
	}
	const int& getI(const std::string_view name) {
		static auto result = plugin->call<int>("Config::getI", std::string(name));
		return result;
	}
	const string& getS(const std::string_view name) {
		static auto result = plugin->call<string>("Config::getS", std::string(name));
		return result;
	}
	void push_back_available_batteries(const string& battery) {
		plugin->call<bool>("Config::push_back_available_batteries", battery);
	}
}

namespace Logger {
	void log_write(const Level level, const string& msg) {
		plugin->call<bool>("Logger::log_write", static_cast<size_t>(level), msg);
	}
}

namespace Mem {
	int get_width() {
		return plugin->call<int>("Mem::get_width");
	}
	void set_redraw(bool val) {
		plugin->call<bool>("Mem::set_redraw", std::move(val));
	}
}

namespace Proc {
	int get_width() {
		return plugin->call<int>("Proc::get_width");
	}
	void set_redraw(bool val) {
		plugin->call<bool>("Proc::set_redraw", std::move(val));
	}
	int get_selected_pid() {
		return plugin->call<int>("Proc::get_selected_pid");
	}
	int get_select_max() {
		return plugin->call<int>("Proc::get_select_max");
	}
}

namespace Cpu {
	int get_width() {
		return plugin->call<int>("Cpu::get_width");
	}
}

#ifdef GPU_SUPPORT
namespace Gpu {
	int get_width() {
		return plugin->call<int>("Gpu::get_width");
	}
}
#endif

namespace Net{
	int get_width() {
		return plugin->call<int>("Net::get_width");
	}
	void set_redraw(bool val) {
		plugin->call<bool>("Net::set_redraw", std::move(val));
	}
}

namespace Runner {
	bool get_stopping() {
		return plugin->call<bool>("Runner::get_stopping");
	}
	bool get_coreNum_reset() {
		return plugin->call<bool>("Runner::get_coreNum_reset");
	}
	void set_coreNum_reset(bool val) {
		plugin->call<bool>("Runner::set_coreNum_reset", std::move(val));
	}
}

#endif // BTOP_PLUGIN

#ifdef BTOP_PLUGIN_HOST

#include <cosmo.h>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>

PluginHost* pluginHost = nullptr;

void create_plugin_host() {
	std::stringstream ziposPluginPath("/zip/");
	if (IsAarch64()) {
		ziposPluginPath << "aarch64/";
	} else {
		ziposPluginPath << "x86_64/";
	}
	if (IsLinux()) {
		ziposPluginPath << "linux/";
	} else if (IsXnu()) {
		ziposPluginPath << "macos/";
	} else if (IsWindows()) {
		ziposPluginPath << "windows/";
	} else if (IsFreebsd()) {
		ziposPluginPath << "freebsd/";
	} else if (IsOpenbsd()) {
		ziposPluginPath << "openbsd/";
	} else if (IsNetbsd()) {
		ziposPluginPath << "netbsd/";
	}

	std::string pluginName = "invalid_platform";
	if (IsLinux() || IsFreebsd()) {
		pluginName = "libbtop.so";
	} else if (IsXnu()) {
		if (IsXnuSilicon()) {
			pluginName = "libbtop.dylib";
		} else {
			pluginName = "libbtop.exe";
		}
	} else if (IsWindows()) {
		pluginName = "btop.dll";
	} else if (IsOpenbsd() || IsNetbsd()) {
		pluginName = "libbtop.exe";
	}

	// Create temp directory for btop plugin
	auto tmpdir = std::filesystem::temp_directory_path() / "btop";
	if (!std::filesystem::exists(tmpdir)) {
		std::filesystem::create_directory(tmpdir);
	}

	// Extract btop plugin from zipos
	auto pluginPath = tmpdir / pluginName;
	if (!std::filesystem::exists(ziposPluginPath.str() + pluginName)) {
		std::cerr << "Plugin not found in zipos: " << ziposPluginPath.str() + pluginName << std::endl;
	} else {
		std::filesystem::copy_file(ziposPluginPath.str() + pluginName, pluginPath);
		chmod(pluginPath.c_str(), 0700);
	}

	pluginHost = new PluginHost(pluginPath.string());

	pluginHost->registerHandler<std::unordered_map<std::string, int>>("Config::get_ints", std::function([]() {
		// convert map of string_view to map of string
		std::unordered_map<std::string, int> result;
		for (const auto& [key, value] : Config::ints) {
			result[std::string(key)] = value;
		}
		return result;
	}));
	pluginHost->registerHandler<bool, std::string, int>("Config::ints_set_at", std::function([](string name, int value) {
		Config::ints.at(name) = value;
		return true;
	}));
	pluginHost->registerHandler<bool>("Config::getB", std::function([](string name) {
		return Config::getB(name);
	}));
	pluginHost->registerHandler<int>("Config::getI", std::function([](string name) {
		return Config::getI(name);
	}));
	pluginHost->registerHandler<string>("Config::getS", std::function([](string name) {
		return Config::getS(name);
	}));
	pluginHost->registerHandler<bool>("Config::push_back_available_batteries", std::function([](string battery) {
		Config::available_batteries.push_back(battery);
		return true;
	}));

	pluginHost->registerHandler<bool>("Logger::log_write", std::function([](size_t level, string msg) {
		Logger::log_write(static_cast<Logger::Level>(level), msg);
		return true;
	}));

	pluginHost->registerHandler<int>("Mem::get_width", std::function([]() {
		return Mem::width;
	}));
	pluginHost->registerHandler<bool>("Mem::set_redraw", std::function([](bool val) {
		Mem::redraw = val;
		return true;
	}));

	pluginHost->registerHandler<int>("Proc::get_width", std::function([]() {
		return Proc::width;
	}));
	pluginHost->registerHandler<bool>("Proc::set_redraw", std::function([](bool val) {
		Proc::redraw = val;
		return true;
	}));
	pluginHost->registerHandler<int>("Proc::get_selected_pid", std::function([]() {
		return Proc::selected_pid;
	}));
	pluginHost->registerHandler<int>("Proc::get_select_max", std::function([]() {
		return Proc::select_max;
	}));

	pluginHost->registerHandler<int>("Cpu::get_width", std::function([]() {
		return Cpu::width;
	}));

#ifdef GPU_SUPPORT
	pluginHost->registerHandler<int>("Gpu::get_width", std::function([]() {
		return Gpu::width;
	}));
#endif

	pluginHost->registerHandler<int>("Net::get_width", std::function([]() {
		return Net::width;
	}));
	pluginHost->registerHandler<bool>("Net::set_redraw", std::function([](bool val) {
		Net::redraw = val;
		return true;
	}));

	pluginHost->registerHandler<bool>("Runner::get_stopping", std::function([]() {
		return Runner::stopping.load();
	}));
	pluginHost->registerHandler<bool>("Runner::get_coreNum_reset", std::function([]() {
		return Runner::coreNum_reset.load();
	}));
	pluginHost->registerHandler<bool>("Runner::set_coreNum_reset", std::function([](bool val) {
		Runner::coreNum_reset = val;
		return true;
	}));

	pluginHost->initialize();
}

#ifdef GPU_SUPPORT
namespace Gpu {
	bool Nvml::shutdown() {
		return pluginHost->call<bool>("Gpu::Nvml::shutdown");
	}
	bool Rsmi::shutdown() {
		return pluginHost->call<bool>("Gpu::Rsmi::shutdown");
	}
	vector<gpu_info>& collect(bool no_update) {
		static auto info = pluginHost->call<vector<gpu_info>, bool>("Gpu::collect", std::move(no_update));
		return info;
	}
	int get_count() {
		return pluginHost->call<int>("Gpu::get_count");
	}
	vector<string>& get_gpu_names() {
		static auto info = pluginHost->call<vector<string>>("Gpu::get_gpu_names");
		return info;
	}
	vector<int>& get_gpu_b_height_offsets() {
		static auto info = pluginHost->call<vector<int>>("Gpu::get_gpu_b_height_offsets");
		return info;
	}
	unordered_map<string, deque<long long>>& get_shared_gpu_percent() {
		static auto info = pluginHost->call<unordered_map<string, deque<long long>>>("Gpu::get_shared_gpu_percent");
		return info;
	}
}
#endif

namespace Cpu {
	cpu_info& collect(bool no_update) {
		static auto info = pluginHost->call<cpu_info, bool>("Cpu::collect", std::move(no_update));
		return info;
	}
	string get_cpuHz() {
		return pluginHost->call<string>("Cpu::get_cpuHz");
	}
	bool update_core_mapping() {
		return pluginHost->call<bool>("Cpu::update_core_mapping");
	}
	bool get_has_battery() {
		return pluginHost->call<bool>("Cpu::get_has_battery");
	}
	bool get_got_sensors() {
		return pluginHost->call<bool>("Cpu::get_got_sensors");
	}
	bool get_cpu_temp_only() {
		return pluginHost->call<bool>("Cpu::get_cpu_temp_only");
	}
	string get_cpuName() {
		return pluginHost->call<string>("Cpu::get_cpuName");
	}
	vector<string>& get_available_fields() {
		static auto info = pluginHost->call<vector<string>>("Cpu::get_available_fields");
		return info;
	}
	vector<string>& get_available_sensors() {
		static auto info = pluginHost->call<vector<string>>("Cpu::get_available_sensors");
		return info;
	}
	tuple<int, float, long, string>& get_current_bat() {
		static auto info = pluginHost->call<tuple<int, float, long, string>>("Cpu::get_current_bat");
		return info;
	}
}

namespace Mem {
	mem_info& collect(bool no_update) {
		static auto info = pluginHost->call<mem_info, bool>("Mem::collect", std::move(no_update));
		return info;
	}
	uint64_t get_totalMem() {
		return pluginHost->call<uint64_t>("Mem::get_totalMem");
	}
	bool get_has_swap() {
		return pluginHost->call<bool>("Mem::get_has_swap");
	}
	int get_disk_ios() {
		return pluginHost->call<int>("Mem::get_disk_ios");
	}
}

namespace Net {
	net_info& collect(bool no_update) {
		static net_info info = pluginHost->call<net_info, bool>("Net::collect", std::move(no_update));
		return info;
	}
	string get_selected_iface() {
		return pluginHost->call<string>("Net::get_selected_iface");
	}
	void set_selected_iface(const string& iface) {
		pluginHost->call<bool, string>("Net::set_selected_iface", string(iface));
	}
	vector<string>& get_interfaces() {
		static auto info = pluginHost->call<vector<string>>("Net::get_interfaces");
		return info;
	}
	unordered_map<string, uint64_t>& get_graph_max() {
		static auto info = pluginHost->call<unordered_map<string, uint64_t>>("Net::get_graph_max");
		return info;
	}
	void set_rescale(bool rescale) {
		pluginHost->call<bool>("Net::set_rescale", std::move(rescale));
	}
	unordered_map<string, net_info>& get_current_net() {
		static auto info = pluginHost->call<unordered_map<string, net_info>>("Net::get_current_net");
		return info;
	}
}

namespace Proc {
	vector<proc_info>& collect(bool no_update) {
		static auto info = pluginHost->call<vector<proc_info>, bool>("Proc::collect", std::move(no_update));
		return info;
	}
	int get_numpids() {
		return pluginHost->call<int>("Proc::get_numpids");
	}
	void set_collapse(int val) {
		pluginHost->call<bool>("Proc::set_collapse", std::move(val));
	}
	void set_expand(int val) {
		pluginHost->call<bool>("Proc::set_expand", std::move(val));
	}
	void increment_filter_found() {
		pluginHost->call<bool>("Proc::increment_filter_found");
	}
	detail_container get_detailed() {
		return pluginHost->call<detail_container>("Proc::get_detailed");
	}
}

namespace Shared {
	void init() {
		pluginHost->call<bool>("Shared::init");
	}
	long get_coreCount() {
		return pluginHost->call<long>("Shared::get_coreCount");
	}
}

namespace Tools {
	double system_uptime() {
		return pluginHost->call<double>("Tools::system_uptime");
	}
}

#endif // BTOP_PLUGIN_HOST