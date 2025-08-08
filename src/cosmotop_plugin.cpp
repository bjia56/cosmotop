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

#include <sstream>

#include <cosmo_plugin.hpp>

#include "cosmotop_shared.hpp"
#include "cosmotop_tools.hpp"
#include "cosmotop_config.hpp"

using std::unordered_map;

#ifndef __COSMOPOLITAN__

#include "config.h"

namespace Gpu {
	namespace Nvml {
		bool shutdown();
	}
	namespace Rsmi {
		bool shutdown();
	}
}

namespace Shared {
	namespace WMI {
		bool shutdown();
	}
}

static Plugin* plugin = nullptr;
static int plugin_cache_counter = 0;

void plugin_initializer(Plugin* plugin) {
	::plugin = plugin;

	plugin->registerHandler<string>("build_info", std::function([]() {
		std::stringstream ss;
		ss << "Host-native plugin compiled with: " << COMPILER << " (" << COMPILER_VERSION << ")";
		return ss.str();
	}));
	plugin->registerHandler<bool>("reset_cache", std::function([]() {
		plugin_cache_counter++;
		return true;
	}));

	plugin->registerHandler<bool>("register_cosmotop_directory", std::function([](std::string dir) {
#ifdef _WIN32
		extern std::filesystem::path cosmotop_dir;
		cosmotop_dir = dir;
#endif
		return true;
	}));

	plugin->registerHandler<vector<Npu::npu_info>, bool>("Npu::collect", std::function([](bool no_update) {
#if defined(__linux__) || defined(__APPLE__)
		return Npu::collect(no_update);
#else
		return vector<Npu::npu_info>();
#endif
	}));
	plugin->registerHandler<int>("Npu::get_count", std::function([]() {
#if defined(__linux__) || defined(__APPLE__)
		return Npu::count;
#else
		return 0;
#endif
	}));
	plugin->registerHandler<vector<string>>("Npu::get_npu_names", std::function([]() {
#if defined(__linux__) || defined(__APPLE__)
		return Npu::npu_names;
#else
		return vector<string>();
#endif
	}));
	plugin->registerHandler<vector<int>>("Npu::get_npu_b_height_offsets", std::function([]() {
#if defined(__linux__) || defined(__APPLE__)
		return Npu::npu_b_height_offsets;
#else
		return vector<int>();
#endif
	}));
	plugin->registerHandler<unordered_map<string, deque<long long>>>("Npu::get_shared_npu_percent", std::function([]() {
#if defined(__linux__) || defined(__APPLE__)
		return Npu::shared_npu_percent;
#else
		return unordered_map<string, deque<long long>>();
#endif
	}));

	plugin->registerHandler<vector<Gpu::gpu_info>, bool>("Gpu::collect", std::function([](bool no_update) {
#if defined(__linux__) || defined(_WIN32)
		return Gpu::collect(no_update);
#else
		return vector<Gpu::gpu_info>();
#endif
	}));
	plugin->registerHandler<int>("Gpu::get_count", std::function([]() {
#if defined(__linux__) || defined(_WIN32)
		return Gpu::count;
#else
		return 0;
#endif
	}));
	plugin->registerHandler<vector<string>>("Gpu::get_gpu_names", std::function([]() {
#if defined(__linux__) || defined(_WIN32)
		return Gpu::gpu_names;
#else
		return vector<string>();
#endif
	}));
	plugin->registerHandler<vector<int>>("Gpu::get_gpu_b_height_offsets", std::function([]() {
#if defined(__linux__) || defined(_WIN32)
		return Gpu::gpu_b_height_offsets;
#else
		return vector<int>();
#endif
	}));
	plugin->registerHandler<unordered_map<string, deque<long long>>>("Gpu::get_shared_gpu_percent", std::function([]() {
#if defined(__linux__) || defined(_WIN32)
		return Gpu::shared_gpu_percent;
#else
		return unordered_map<string, deque<long long>>();
#endif
	}));

	plugin->registerHandler<Cpu::cpu_info, bool>("Cpu::collect", std::function([](bool no_update) {
		return Cpu::collect(no_update);
	}));
	plugin->registerHandler<string>("Cpu::get_cpuHz", std::function([]() {
		return Cpu::cpuHz;
	}));
	plugin->registerHandler<bool>("Cpu::update_core_mapping", std::function([]() {
#ifndef _WIN32
		Cpu::core_mapping = Cpu::get_core_mapping();
#endif
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

	plugin->registerHandler<vector<Container::container_info>, bool>("Container::collect", std::function([](bool no_update) {
#if defined(__linux__)
		return Container::collect(no_update);
#else
		return vector<Container::container_info>();
#endif
	}));
	plugin->registerHandler<int>("Container::get_numcontainers", std::function([]() {
#if defined(__linux__)
		return Container::numcontainers.load();
#else
		return 0;
#endif
	}));
	plugin->registerHandler<bool>("Container::get_has_containers", std::function([]() {
#if defined(__linux__)
		return Container::has_containers;
#else
		return false;
#endif
	}));

	plugin->registerHandler<bool>("Shared::init", std::function([]() {
		Shared::init();
		return true;
	}));
	plugin->registerHandler<long>("Shared::get_coreCount", std::function([]() {
		return Shared::coreCount;
	}));
	plugin->registerHandler<bool>("Shared::shutdown", std::function([]() {
#if defined(_WIN32)
		Shared::WMI::shutdown();
#elif defined(__linux__)
		Gpu::Nvml::shutdown();
		Gpu::Rsmi::shutdown();
#endif
		return true;
	}));

	plugin->registerHandler<double>("Tools::system_uptime", std::function([]() {
		return Tools::system_uptime();
	}));
}

namespace Config {
	unordered_map<string, int>& get_ints() {
		static unordered_map<string, int> result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter) return result;
		result = plugin->call<unordered_map<string, int>>("Config::get_ints");
		cache_counter = plugin_cache_counter;
		return result;
	}
	void ints_set_at(const std::string_view name, const int value) {
		plugin->call<bool>("Config::ints_set_at", std::string(name), value);
	}
	unordered_map<string, bool>& get_bools() {
		static unordered_map<string, bool> result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter) return result;
		result = plugin->call<unordered_map<string, bool>>("Config::get_bools");
		cache_counter = plugin_cache_counter;
		return result;
	}
	unordered_map<string, string>& get_strings() {
		static unordered_map<string, string> result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter) return result;
		result = plugin->call<unordered_map<string, string>>("Config::get_strings");
		cache_counter = plugin_cache_counter;
		return result;
	}
	const bool& getB(const std::string& name) {
		return get_bools().at(name);
	}
	const int& getI(const std::string& name) {
		return get_ints().at(name);
	}
	const string& getS(const std::string& name) {
		return get_strings().at(name);
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

namespace Gpu {
	int get_width() {
		return plugin->call<int>("Gpu::get_width");
	}
}

namespace Npu {
	int get_width() {
		return plugin->call<int>("Npu::get_width");
	}
}

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
	void active_atomic_wait() {
		plugin->call<bool>("Runner::active_atomic_wait");
	}
}

namespace Global {
	bool get_quitting() {
		return plugin->call<bool>("Global::get_quitting");
	}
}

#else // __COSMOPOLITAN__

#include <cosmo.h>
#include <filesystem>
#include <unordered_set>
#include <sys/stat.h>
#include <spawn.h>

#include <libc/nt/runtime.h>
#include <libc/proc/ntspawn.h>

static PluginHost* pluginHost = nullptr;
static int plugin_cache_counter = 0;

static std::filesystem::path getOutputDirectory() {
	const char *homedir;
	if (IsWindows()) {
		homedir = getenv("USERPROFILE");
	} else {
		homedir = getenv("HOME");
	}
	if (homedir == nullptr) {
		return std::filesystem::temp_directory_path() / ".cosmotop";
	} else {
		return std::filesystem::path(homedir) / ".cosmotop";
	}
}

static bool isFileNewer(const std::string& filename1, const std::string& filename2) {
	struct stat stat1, stat2;
	if (stat(filename1.c_str(), &stat1) != 0) {
		// Could not stat file1
		return false;
	}
	if (stat(filename2.c_str(), &stat2) != 0) {
		// Could not stat file2
		return true;
	}
	return stat1.st_mtime > stat2.st_mtime;
}

// Helper to execute a process and wait for it, returns exit status
int spawnAndWait(const std::vector<const char*>& argv) {
    pid_t pid;
    int status, waitStatus;
    status = posix_spawnp(&pid, argv[0], nullptr, nullptr, const_cast<char* const*>(argv.data()), nullptr);
    if (status != 0)
        return status; // posix_spawnp failed, not process exit code
    if (waitpid(pid, &waitStatus, 0) == -1)
        return errno;
    if (!WIFEXITED(waitStatus))
        return -1; // did not exit normally
    return WEXITSTATUS(waitStatus);
}

int tryPythonInterpreters(const std::vector<std::string>& argList) {
	static const std::vector<std::string> pythonVersions = {
		"python3", "python3.12", "python3.11", "python3.10", "python3.9"
	};
    for (const auto& py : pythonVersions) {
        std::vector<const char*> argv;
        argv.push_back(py.c_str());
        for (const auto& arg : argList) argv.push_back(arg.c_str());
        argv.push_back(nullptr);
        int ret = spawnAndWait(argv);
        if (ret == 0) return 0;
    }
	return 1;
}

void downloadFile(const std::string& url, const std::string& outPath) {
    std::vector<const char*> curlArgv = {"curl", "-s", "-L", url.c_str(), "-o", outPath.c_str(), nullptr};
    int ret = spawnAndWait(curlArgv);
    if (ret == 0) return;

    std::vector<const char*> wgetArgv = {"wget", "-q", url.c_str(), "-O", outPath.c_str(), nullptr};
    ret = spawnAndWait(wgetArgv);
    if (ret == 0) return;

    std::string pythonCmd =
        "import urllib.request, sys; "
        "url = sys.argv[1]; "
        "out = sys.argv[2]; "
        "urllib.request.urlretrieve(url, out)";
    ret = tryPythonInterpreters({ "-c", pythonCmd, url, outPath });
	if (ret == 0) return;

    throw std::runtime_error("Failed to download file: " + url);
}

void zipEmbed(const std::string& zipPath, const std::string& fileToAdd) {
    std::vector<const char*> zipArgv = {"zip", "-quj", zipPath.c_str(), fileToAdd.c_str(), nullptr};
    int ret = spawnAndWait(zipArgv);
    if (ret == 0) return;

    std::string pythonCmd =
        "import zipfile, sys; "
        "zip = sys.argv[1]; "
        "infile = sys.argv[2]; "
        "zf = zipfile.ZipFile(zip, 'a'); "
        "zf.write(infile, arcname=infile.split('/')[-1]); "
        "zf.close()";
    ret = tryPythonInterpreters({ "-c", pythonCmd, zipPath, fileToAdd });
	if (ret == 0) return;

    throw std::runtime_error("Failed to embed file into zip: " + zipPath);
}

void create_plugin_host() {
	std::stringstream pluginName;
	pluginName << "cosmotop-";

	if (IsLinux()) {
#ifdef __x86_64__
		// Check if we are running under Blink
		string hyp = Tools::cpuid(0x40000000);
		if (hyp == "GenuineBlink") {
			string hostOS = Tools::str_to_lower(Tools::cpuid(0x40031337));
			string hostArch = Tools::str_to_lower(Tools::cpuid(0x40031338));
            if (hostOS == "xnu") hostOS = "macos";
			pluginName << hostOS << "-" << hostArch;
			goto choose_extension;
		}
#endif

		pluginName << "linux";
	} else if (IsXnu()) {
		pluginName << "macos";
	} else if (IsWindows()) {
		pluginName << "windows";
	} else if (IsFreebsd()) {
		pluginName << "freebsd";
	} else if (IsOpenbsd()) {
		pluginName << "openbsd";
	} else if (IsNetbsd()) {
		pluginName << "netbsd";
	}
	if (IsAarch64()) {
		pluginName << "-aarch64";
	} else {
		pluginName << "-x86_64";
	}

choose_extension:
	if (IsWindows()) {
		pluginName << ".dll";
	} else {
		pluginName << ".exe";
	}

	// Create output directory for cosmotop plugin
	auto outdir = getOutputDirectory();
	std::filesystem::create_directory(outdir);

	// Extract cosmotop plugin from zipos
	auto pluginPath = outdir / pluginName.str();
	auto ziposPath = std::filesystem::path("/zip/") / pluginName.str();
	std::filesystem::path currPath = std::filesystem::path(GetProgramExecutableName());
	if (!std::filesystem::exists(pluginPath) || isFileNewer(currPath, pluginPath)) {
		if (std::filesystem::exists(pluginPath)) {
			std::filesystem::remove(pluginPath);
		}

		if (!std::filesystem::exists(ziposPath)) {
			try {
				Logger::info("Plugin not found in zipos, downloading from GitHub...");
				std::string url = "https://github.com/bjia56/cosmotop/releases/download/v" + Global::Version + "/" + pluginName.str();
				downloadFile(url, pluginPath);

				if (!IsWindows()) {
					chmod(pluginPath.c_str(), 0500);
				}

				// Make temporary copy of the current executable
				std::filesystem::path tempPath = std::filesystem::path(GetProgramExecutableName()) += ".tmp";
				bool doSelfUpdate = true;

				std::vector<const char*> cpArgv = {"cp", "-f", currPath.c_str(), tempPath.c_str(), nullptr};
				int cpStatus = spawnAndWait(cpArgv);
				if (cpStatus != 0) {
					Logger::error("Failed to copy current executable: " + std::string(strerror(cpStatus)));
					doSelfUpdate = false;
				}

				if (doSelfUpdate) {
					try {
						zipEmbed(tempPath, pluginPath);
						std::filesystem::rename(tempPath, currPath);
					} catch (const std::exception& e) {
						Logger::error(e.what());
						std::filesystem::remove(tempPath);
					}
				} else {
					std::filesystem::remove(tempPath);
				}
			} catch (const std::exception& e) {
				Logger::error(e.what());
				throw;
			}
		} else {
			std::filesystem::copy_file(ziposPath, pluginPath, std::filesystem::copy_options::overwrite_existing);
			if (!IsWindows()) {
				chmod(pluginPath.c_str(), 0500);
			}
		}
	}

	// On Windows, extract extras
	if (IsWindows()) {
		auto ziposDir = std::filesystem::path("/zip/windows");
		if (!std::filesystem::exists(ziposDir)) {
			throw std::runtime_error("Windows dll directory not found in zipos: " + ziposDir.string());
		}
		for (const auto& entry : std::filesystem::directory_iterator(ziposDir)) {
			auto entryPath = outdir / entry.path().filename();
			if (!std::filesystem::exists(entryPath) || isFileNewer(currPath, entryPath)) {
				std::filesystem::copy_file(entry.path(), entryPath, std::filesystem::copy_options::overwrite_existing);
			}
		}
	}

	auto launchMethod = IsWindows() ? PluginHost::DLOPEN : PluginHost::FORK;
	pluginHost = new PluginHost(pluginPath.string(), launchMethod);

	pluginHost->registerHandler<std::unordered_map<string, int>>("Config::get_ints", std::function([]() {
		std::unordered_map<string, int> result;
		for (const auto& [key, value] : Config::ints) {
			result[string(key)] = value;
		}
		for (const auto& [key, value] : Config::intsOverrides) {
			result[string(key)] = value;
		}
		return result;
	}));
	pluginHost->registerHandler<bool, string, int>("Config::ints_set_at", std::function([](string name, int value) {
		Config::ints.at(name) = value;
		if (Config::intsOverrides.contains(name)) Config::intsOverrides.erase(name);
		return true;
	}));
	pluginHost->registerHandler<std::unordered_map<string, bool>>("Config::get_bools", std::function([]() {
		std::unordered_map<string, bool> result;
		for (const auto& [key, value] : Config::bools) {
			result[string(key)] = value;
		}
		for (const auto& [key, value] : Config::boolsOverrides) {
			result[string(key)] = value;
		}
		return result;
	}));
	pluginHost->registerHandler<std::unordered_map<string, string>>("Config::get_strings", std::function([]() {
		std::unordered_map<string, string> result;
		for (const auto& [key, value] : Config::strings) {
			result[string(key)] = value;
		}
		for (const auto& [key, value] : Config::stringsOverrides) {
			result[string(key)] = value;
		}
		return result;
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
		return Proc::select_max_rows;
	}));

	pluginHost->registerHandler<int>("Cpu::get_width", std::function([]() {
		return Cpu::width;
	}));

	pluginHost->registerHandler<int>("Gpu::get_width", std::function([]() {
		return Gpu::width;
	}));

	pluginHost->registerHandler<int>("Npu::get_width", std::function([]() {
		return Npu::width;
	}));

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
	pluginHost->registerHandler<bool>("Runner::active_atomic_wait", std::function([]() {
		Tools::atomic_wait(Runner::active);
		return true;
	}));

	pluginHost->registerHandler<bool>("Global::get_quitting", std::function([]() {
		return Global::quitting.load();
	}));

	try {
		pluginHost->initialize();
	} catch (const std::exception& e) {
		delete pluginHost;
		pluginHost = nullptr;
		if (IsWindows()) {
			throw std::runtime_error("Failed to initialize plugin: " + string(e.what()) + " (" + to_string(GetLastError()) + ")");
		} else {
			throw;
		}
	}

	if (IsWindows()) {
		char *ntpath = strdup(outdir.c_str());
		mungentpath(ntpath);
		pluginHost->call<bool>("register_cosmotop_directory", string(ntpath));
		free(ntpath);
	} else {
		pluginHost->call<bool>("register_cosmotop_directory", outdir.string());
	}
}

bool is_plugin_loaded() {
	return pluginHost != nullptr;
}

void trigger_plugin_refresh() {
	plugin_cache_counter++;
	if (pluginHost) {
		pluginHost->call<bool>("reset_cache");
	}
}

void shutdown_plugin() {
	if (pluginHost) {
		delete pluginHost;
		pluginHost = nullptr;
	}
}

string plugin_build_info() {
	return pluginHost->call<string>("build_info");
}

#define PLUGIN_FETCH_REMOTE(typ, rpc) \
	static typ result; \
	static int cache_counter = -1; \
	if (cache_counter == plugin_cache_counter) return result; \
	result = pluginHost->call<typ>(rpc); \
	cache_counter = plugin_cache_counter; \
	return result;
#define PLUGIN_FETCH_REMOTE_ARGS(typ, rpc, ...) \
	static typ result; \
	static int cache_counter = -1; \
	if (cache_counter == plugin_cache_counter) return result; \
	result = pluginHost->call<typ>(rpc, __VA_ARGS__); \
	cache_counter = plugin_cache_counter; \
	return result;
#define PLUGIN_FETCH_REMOTE_ONCE(typ, rpc) \
	static typ result; \
	static bool fetched = false; \
	if (fetched) return result; \
	result = pluginHost->call<typ>(rpc); \
	fetched = true; \
	return result;
#define PLUGIN_FETCH_REMOTE_ONCE_ARGS(typ, rpc, ...) \
	static typ result; \
	static bool fetched = false; \
	if (fetched) return result; \
	result = pluginHost->call<typ>(rpc, __VA_ARGS__); \
	fetched = true; \
	return result;

namespace Npu {
	vector<npu_info>& collect(bool no_update) {
		static vector<npu_info> result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter and no_update) return result;
		result = pluginHost->call<vector<npu_info>>("Npu::collect", std::move(no_update));
		cache_counter = plugin_cache_counter;
		return result;
	}
	int get_count() {
		typedef int result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Npu::get_count");
	}
	vector<string>& get_npu_names() {
		typedef vector<string> result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Npu::get_npu_names");
	}
	vector<int>& get_npu_b_height_offsets() {
		typedef vector<int> result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Npu::get_npu_b_height_offsets");
	}
	unordered_map<string, deque<long long>>& get_shared_npu_percent() {
		typedef unordered_map<string, deque<long long>> result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Npu::get_shared_npu_percent");
	}
}

namespace Gpu {
	vector<gpu_info>& collect(bool no_update) {
		static vector<gpu_info> result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter and no_update) return result;
		result = pluginHost->call<vector<gpu_info>>("Gpu::collect", std::move(no_update));
		cache_counter = plugin_cache_counter;
		return result;
	}
	int get_count() {
		typedef int result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Gpu::get_count");
	}
	vector<string>& get_gpu_names() {
		typedef vector<string> result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Gpu::get_gpu_names");
	}
	vector<int>& get_gpu_b_height_offsets() {
		typedef vector<int> result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Gpu::get_gpu_b_height_offsets");
	}
	unordered_map<string, deque<long long>>& get_shared_gpu_percent() {
		typedef unordered_map<string, deque<long long>> result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Gpu::get_shared_gpu_percent");
	}
}

namespace Cpu {
	cpu_info& collect(bool no_update) {
		static cpu_info result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter and no_update) return result;
		result = pluginHost->call<cpu_info>("Cpu::collect", std::move(no_update));
		cache_counter = plugin_cache_counter;
		return result;
	}
	string get_cpuHz() {
		typedef string result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Cpu::get_cpuHz");
	}
	bool update_core_mapping() {
		typedef bool result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Cpu::update_core_mapping");
	}
	bool get_has_battery() {
		typedef bool result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Cpu::get_has_battery");
	}
	bool get_got_sensors() {
		typedef bool result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Cpu::get_got_sensors");
	}
	bool get_cpu_temp_only() {
		typedef bool result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Cpu::get_cpu_temp_only");
	}
	string get_cpuName() {
		typedef string result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Cpu::get_cpuName");
	}
	vector<string>& get_available_fields() {
		typedef vector<string> result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Cpu::get_available_fields");
	}
	vector<string>& get_available_sensors() {
		typedef vector<string> result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Cpu::get_available_sensors");
	}
	tuple<int, float, long, string>& get_current_bat() {
		typedef tuple<int, float, long, string> result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Cpu::get_current_bat");
	}
}

namespace Mem {
	mem_info& collect(bool no_update) {
		static mem_info result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter and no_update) return result;
		result = pluginHost->call<mem_info>("Mem::collect", std::move(no_update));
		cache_counter = plugin_cache_counter;
		return result;
	}
	uint64_t get_totalMem() {
		typedef uint64_t result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Mem::get_totalMem");
	}
	bool get_has_swap() {
		typedef bool result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Mem::get_has_swap");
	}
	int get_disk_ios() {
		typedef int result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Mem::get_disk_ios");
	}
}

namespace Net {
	net_info& collect(bool no_update) {
		static net_info result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter and no_update) return result;
		result = pluginHost->call<net_info>("Net::collect", std::move(no_update));
		cache_counter = plugin_cache_counter;
		return result;
	}
	string get_selected_iface() {
		typedef string result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Net::get_selected_iface");
	}
	void set_selected_iface(const string& iface) {
		pluginHost->call<bool>("Net::set_selected_iface", string(iface));
	}
	vector<string>& get_interfaces() {
		typedef vector<string> result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Net::get_interfaces");
	}
	unordered_map<string, uint64_t>& get_graph_max() {
		typedef unordered_map<string, uint64_t> result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Net::get_graph_max");
	}
	void set_rescale(bool rescale) {
		pluginHost->call<bool>("Net::set_rescale", std::move(rescale));
	}
	unordered_map<string, net_info>& get_current_net() {
		typedef unordered_map<string, net_info> result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Net::get_current_net");
	}
}

namespace Proc {
	vector<proc_info>& collect(bool no_update) {
		static vector<proc_info> result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter and no_update) return result;
		result = pluginHost->call<vector<proc_info>>("Proc::collect", std::move(no_update));
		cache_counter = plugin_cache_counter;
		return result;
	}
	int get_numpids() {
		typedef int result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Proc::get_numpids");
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
		typedef detail_container result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Proc::get_detailed");
	}
}

namespace Container {
	vector<container_info>& collect(bool no_update) {
		static vector<container_info> result;
		static int cache_counter = -1;
		if (cache_counter == plugin_cache_counter and no_update) return result;
		result = pluginHost->call<vector<container_info>>("Container::collect", std::move(no_update));
		cache_counter = plugin_cache_counter;
		return result;
	}
	bool get_has_containers() {
		typedef bool result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Container::get_has_containers");
	}
	int get_numcontainers() {
		typedef int result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Container::get_numcontainers");
	}
}

namespace Shared {
	void init() {
		pluginHost->call<bool>("Shared::init");
	}
	long get_coreCount() {
		typedef long result_type;
		PLUGIN_FETCH_REMOTE_ONCE(result_type, "Shared::get_coreCount");
	}
	bool shutdown() {
		return pluginHost->call<bool>("Shared::shutdown");
	}
}

namespace Tools {
	double system_uptime() {
		typedef double result_type;
		PLUGIN_FETCH_REMOTE(result_type, "Tools::system_uptime");
	}
}

#endif // __COSMOPOLITAN__
