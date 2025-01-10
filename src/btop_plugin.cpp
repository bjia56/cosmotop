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

using std::unordered_map;

//#define BTOP_PLUGIN
//#define BTOP_PLUGIN_HOST
//#define GPU_SUPPORT

#ifdef BTOP_PLUGIN

void plugin_initializer(Plugin* plugin) {
#ifdef GPU_SUPPORT
	plugin->registerHandler<bool>("Gpu::Nvml::shutdown", []() {
		return Gpu::Nvml::shutdown();
	});
	plugin->registerHandler<bool>("Gpu::Rsmi::shutdown", []() {
		return Gpu::Rsmi::shutdown();
	});
	plugin->registerHandler<vector<Gpu::gpu_info>, bool>("Gpu::collect", [](bool no_update) {
		return Gpu::collect(no_update);
	});
#endif

	plugin->registerHandler<Cpu::cpu_info, bool>("Cpu::collect", [](bool no_update) {
		return Cpu::collect(no_update);
	});
	plugin->registerHandler<string>("Cpu::get_cpuHz", []() {
		return Cpu::cpuHz;
	});
	plugin->registerHandler<bool>("Cpu::update_core_mapping", []() {
		Cpu::core_mapping = Cpu::get_core_mapping();
		return true;
	});
	plugin->registerHandler<bool>("Cpu::get_has_battery", []() {
		return Cpu::has_battery;
	});
	plugin->registerHandler<bool>("Cpu::get_got_sensors", []() {
		return Cpu::got_sensors;
	});
	plugin->registerHandler<bool>("Cpu::get_cpu_temp_only", []() {
		return Cpu::cpu_temp_only;
	});
	plugin->registerHandler<string>("Cpu::get_cpuName", []() {
		return Cpu::cpuName;
	});
	plugin->registerHandler<vector<string>>("Cpu::get_available_fields", []() {
		return Cpu::available_fields;
	});
	plugin->registerHandler<vector<string>>("Cpu::get_available_sensors", []() {
		return Cpu::available_sensors;
	});
	plugin->registerHandler<tuple<int, float, long, string>>("Cpu::get_current_bat", []() {
		return Cpu::current_bat;
	});

	plugin->registerHandler<Mem::mem_info, bool>("Mem::collect", [](bool no_update) {
		return Mem::collect(no_update);
	});
	plugin->registerHandler<uint64_t>("Mem::get_totalMem", []() {
		return Mem::totalMem;
	});
	plugin->registerHandler<bool>("Mem::get_has_swap", []() {
		return Mem::has_swap;
	});
	plugin->registerHandler<int>("Mem::get_disk_ios", []() {
		return Mem::disk_ios;
	});

	plugin->registerHandler<Net::net_info, bool>("Net::collect", [](bool no_update) {
		return Net::collect(no_update);
	});
	plugin->registerHandler<string>("Net::get_selected_iface", []() {
		return Net::selected_iface;
	});
	plugin->registerHandler<bool, string>("Net::set_selected_iface", [](string iface) {
		Net::selected_iface = iface;
		return true;
	});
	plugin->registerHandler<vector<string>>("Net::get_interfaces", []() {
		return Net::interfaces;
	});
	plugin->registerHandler<unordered_map<string, uint64_t>>("Net::get_graph_max", []() {
		return Net::graph_max;
	});
	plugin->registerHandler<bool>("Net::set_rescale", [](bool rescale) {
		Net::rescale = rescale;
		return true;
	});
	plugin->registerHandler<unordered_map<string, net_info>>("Net::get_current_net", []() {
		return Net::current_net;
	});

	plugin->registerHandler<vector<Proc::proc_info>, bool>("Proc::collect", [](bool no_update) {
		return Proc::collect(no_update);
	});
	plugin->registerHandler<int>("Proc::get_numpids", []() {
		return Proc::numpids;
	});
	plugin->registerHandler<bool, int>("Proc::set_collapse", [](int val) {
		Proc::collapse = val;
		return true;
	});
	plugin->registerHandler<bool, int>("Proc::set_expand", [](int val) {
		Proc::expand = val;
		return true;
	});
	plugin->registerHandler<bool>("Proc::increment_filter_found", []() {
		Proc::filter_found++;
		return true;
	});
	plugin->registerHandler<Proc::detail_container>("Proc::get_detailed", []() {
		return Proc::get_detailed();
	});

	plugin->registerHandler<bool>("Shared::init", []() {
		Shared::init();
		return true;
	});
	plugin->registerHandler<long>("Shared::get_coreCount", []() {
		return Shared::coreCount;
	});

	plugin->registerHandler<double>("Tools::system_uptime", []() {
		return Tools::system_uptime();
	});
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
		ziposPluginPath << "mac/";
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
		pluginName = "libbtop.dll";
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
	if (!std::filesystem::exists(pluginPath)) {
		std::filesystem::copy_file(ziposPluginPath.str() + pluginName, pluginPath);
		chmod(pluginPath.c_str(), 0700);
	}

	pluginHost = new PluginHost(pluginPath.string());
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