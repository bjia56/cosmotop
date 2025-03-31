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

#include <algorithm>
#include <csignal>
#include <clocale>
#include <pthread.h>
#include <thread>
#include <numeric>
#include <unistd.h>
#include <cmath>
#include <iostream>
#include <exception>
#include <tuple>
#include <regex>
#include <chrono>
#include <utility>
#include <condition_variable>

#include <cosmo.h>
#include <libc/calls/struct/utsname.h>
#include <libc/runtime/runtime.h>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "cosmotop_shared.hpp"
#include "cosmotop_tools.hpp"
#include "cosmotop_config.hpp"
#include "cosmotop_input.hpp"
#include "cosmotop_theme.hpp"
#include "cosmotop_draw.hpp"
#include "cosmotop_menu.hpp"
#include "cosmotop_plugin.hpp"
#include "config.h"

using std::atomic;
using std::cout;
using std::flush;
using std::min;
using std::string;
using std::string_view;
using std::to_string;
using std::vector;

namespace fs = std::filesystem;

using namespace Tools;
using namespace std::chrono_literals;
using namespace std::literals;

namespace Global {
	const vector<array<string, 4>> Banner_src = {
		{"#ced7e0", "██████╗ ██████╗ ██████╗ ██╗     ██╗ ██████╗ ", "#A9A9A9", "██████╗ ██████╗ ██████╗"},
		{"#9ccddc", "██╔═══╝ ██╔═██║ ██╔═══╝ ████╗ ████║ ██╔═██║ ", "#989898", "╚═██╔═╝ ██╔═██║ ██╔═██║"},
		{"#5591a9", "██║     ██║ ██║ ██████╗ ██╔═██╔═██║ ██║ ██║ ", "#888888", "  ██║   ██║ ██║ ██████║"},
		{"#054569", "██║     ██║ ██║ ╚═══██║ ██║ ╚═╝ ██║ ██║ ██║ ", "#777777", "  ██║   ██║ ██║ ██╔═══╝"},
		{"#062c43", "██████╗ ██████║ ██████║ ██║     ██║ ██████║ ", "#666666", "  ██║   ██████║ ██║"},
		{"#000000", "╚═════╝ ╚═════╝ ╚═════╝ ╚═╝     ╚═╝ ╚═════╝ ", "#000000", "  ╚═╝   ╚═════╝ ╚═╝"},
	};
	const string Version = "0.7.0";

	int coreCount;
	string overlay;
	string clock;

	string bg_black = "\x1b[0;40m";
	string fg_white = "\x1b[1;97m";
	string fg_green = "\x1b[1;92m";
	string fg_red = "\x1b[0;91m";

	uid_t real_uid, set_uid;

	fs::path self_path;

	string exit_error_msg;
	atomic<bool> thread_exception (false);

	bool debuginit{};
	bool debug{};
	bool utf_force{};

	uint64_t start_time;

	atomic<bool> resized (false);
	atomic<bool> quitting (false);
	atomic<bool> should_quit (false);
	atomic<bool> should_sleep (false);
	atomic<bool> _runner_started (false);
	atomic<bool> init_conf (false);
	atomic<bool> reload_conf (false);

	bool arg_tty{};
	bool arg_low_color{};
	int arg_preset = -1;
	int arg_update = 0;
}

static void print_version() {
	if constexpr (GIT_COMMIT.empty()) {
		fmt::println("cosmotop version: {}", Global::Version);
	} else {
		fmt::println("cosmotop version: {}+{}", Global::Version, GIT_COMMIT);
	}
}

static void print_version_with_build_info() {
	print_version();
	fmt::println("Compiled with: {} ({})", COMPILER, COMPILER_VERSION);

	struct utsname un;
	if (uname(&un) == 0) {
		auto tokens = ssplit(un.version, ';');
		if (tokens.size() >= 1) {
			fmt::println("{}", trim(tokens[0]));
		}
		if (tokens.size() >= 2) {
			fmt::println("Host platform: {} {} {} {}", un.sysname, un.release, trim(tokens[1]), un.machine);
		} else {
			fmt::println("Host platform: {} {} {}", un.sysname, un.release, un.machine);
		}
	}

	if(!is_plugin_loaded()) {
		try {
			create_plugin_host();
			string plugin_info = plugin_build_info();
			fmt::println("{}", plugin_info);
			shutdown_plugin();
		} catch (const std::exception& e) {
			fmt::println("Unable to load platform-specific plugin: {}", e.what());
		}
	}
}

static void print_usage() {
	fmt::println("\033[1;4mUsage:\033[0;1m cosmotop\033[0m [OPTIONS]\n");
}

static void print_help() {
	print_usage();
	fmt::println(
			"{0}{1}Options:{2}\n"
			"  {0}-h,  --help          {2}show this help message and exit\n"
			"  {0}-v,  --version       {2}show version info and exit\n"
			"  {0}-lc, --low-color     {2}disable truecolor, converts 24-bit colors to 256-color\n"
			"  {0}-t,  --tty_on        {2}force (ON) tty mode, max 16 colors and tty friendly graph symbols\n"
			"  {0}+t,  --tty_off       {2}force (OFF) tty mode\n"
			"  {0}-p,  --preset <id>   {2}start with preset, integer value between 0-9\n"
			"  {0}-u,  --update <ms>   {2}set the program update rate in milliseconds\n"
			"  {0}-o,  --option        {2}override a configuration option in KEY=VALUE format, can use multiple times\n"
			"  {0}     --utf-force     {2}force start even if no UTF-8 locale was detected\n"
			"  {0}     --show-defaults {2}print default configuration values to stdout\n"
			"  {0}     --show-themes   {2}list all available themes\n"
			"  {0}     --licenses      {2}display licenses of open-source software used in cosmotop\n"
			"  {0}     --debug         {2}start in DEBUG mode: shows microsecond timer for information collect\n"
			"  {0}                     {2}and screen draw functions and sets loglevel to DEBUG",
			"\033[1m", "\033[4m", "\033[0m"
	);
}

static void print_help_hint() {
	fmt::println("For more information, try '{0}--help{1}'", "\033[1m", "\033[0m");
}

static std::string find_in_path(const std::string& command) {
    const char* path_env = getenv("PATH");
    if (!path_env) return "";

    std::istringstream path_stream(path_env);
    std::string path;
    while (std::getline(path_stream, path, ':')) {
        std::filesystem::path full_path = std::filesystem::path(path) / command;
        if (std::filesystem::exists(full_path) && access(full_path.c_str(), X_OK) == 0) {
            return full_path.string();
        }
    }
    return "";
}

static void print_with_pager(const std::string& text) {
    const char* pager_env = getenv("PAGER");
    std::string pager_cmd;

    if (pager_env == nullptr || *pager_env == '\0') {
        // Check for "less" or "more" in the system PATH
        std::string less_path = find_in_path("less");
        if (!less_path.empty()) {
            pager_cmd = less_path + " -R";
        } else {
            std::string more_path = find_in_path("more");
            if (!more_path.empty()) {
                pager_cmd = more_path;
            }
        }
    } else {
        pager_cmd = pager_env; // Use the PAGER environment variable
    }

    if (pager_cmd.empty()) {
        // No pager found, fallback to direct output
        fmt::print("{}\n", text);
        return;
    }

    // Open the pager as a pipe
    FILE* pipe = popen(pager_cmd.c_str(), "w");
    if (pipe == nullptr) {
        // If pipe fails, fallback to direct output
        fmt::print("{}\n", text);
    } else {
        // Write the text to the pager
        fmt::print(pipe, "{}\n", text);
        pclose(pipe);
    }
}

static void print_licenses() {
	auto trimBlankLines = [](const std::string& input) {
		std::ostringstream result;
		std::vector<std::string> lines = ssplit(input, '\n', false);

		// Remove blank lines from the front
		size_t start = 0;
		while (start < lines.size() && is_blank(lines[start])) {
			++start;
		}

		// Remove blank lines from the back
		size_t end = lines.size();
		while (end > start && is_blank(lines[end - 1])) {
			--end;
		}

		// Collect the trimmed lines
		for (size_t i = start; i < end; ++i) {
			result << lines[i];
			if (i < end - 1) {
				result << '\n'; // Add newline except after the last line
			}
		}

		return result.str();
	};

	auto licensesPath = fs::path("/zip/licenses");
	if (fs::exists(licensesPath)) {
		vector<string> ossNames;
		for (const auto& entry : fs::directory_iterator(licensesPath)) {
			if (entry.is_regular_file()) {
				ossNames.push_back(entry.path().filename().string());
			}
		}
		if (ossNames.empty()) {
			fmt::print("No licenses found\n");
			return;
		}

		std::stringstream licensesText;

		auto cosmotopLicense = find(ossNames.begin(), ossNames.end(), "cosmotop");
		if (cosmotopLicense != ossNames.end()) {
			ossNames.erase(cosmotopLicense);

			licensesText << fmt::format("{0}{1}{2}{3}\n\n", "\033[1m", "\033[4m", "cosmotop", "\033[0m");
			auto licensePath = licensesPath / "cosmotop";
			std::ifstream licenseFile(licensePath);
			string license((std::istreambuf_iterator<char>(licenseFile)), std::istreambuf_iterator<char>());
			if (!license.empty()) {
				licensesText << fmt::format("{0}\n\n", trimBlankLines(license));
			}
		}

		if (ossNames.empty()) {
			print_with_pager(licensesText.str());
			return;
		}

		sort(ossNames.begin(), ossNames.end());

		for (const auto& name : ossNames) {
			licensesText << fmt::format("{0}{1}{2}{3}\n\n", "\033[1m", "\033[4m", name, "\033[0m");

			auto licensePath = licensesPath / name;
			std::ifstream licenseFile(licensePath);
			string license((std::istreambuf_iterator<char>(licenseFile)), std::istreambuf_iterator<char>());
			if (!license.empty()) {
				licensesText << fmt::format("{0}\n\n", trimBlankLines(license));
			}
		}

		print_with_pager(licensesText.str());
	} else {
		fmt::print("No licenses found\n");
	}
}

//* Config dir init
void init_config_dir() {
	const auto config_dir = Config::get_config_dir();
	if (config_dir.has_value()) {
		Config::conf_dir = config_dir.value();
		Config::conf_file = Config::conf_dir / "cosmotop.conf";
		Logger::logfile = Config::conf_dir / "cosmotop.log";
		Theme::user_theme_dir = Config::conf_dir / "themes";

		// If necessary create the user theme directory
		std::error_code error;
		if (not fs::exists(Theme::user_theme_dir, error) and not fs::create_directories(Theme::user_theme_dir, error)) {
			Theme::user_theme_dir.clear();
			Logger::warning("Failed to create user theme directory: " + error.message());
		}
	}
}

//* A simple argument parser
void argumentParser(const int argc, char **argv, string& configOverrides) {
	for(int i = 1; i < argc; i++) {
		const string argument = argv[i];
		if (is_in(argument, "-h", "--help")) {
		  print_help();
			exit(0);
		}
		else if (is_in(argument, "-v")) {
			print_version();
			exit(0);
		}
		else if (is_in(argument, "--version")) {
			print_version_with_build_info();
			exit(0);
		}
		else if(is_in(argument, "--licenses")) {
			print_licenses();
			exit(0);
		}
		else if (is_in(argument, "--show-defaults")) {
			Config::write(std::cout);
			exit(0);
		}
		else if (is_in(argument, "--show-themes")) {
			init_config_dir();

			fmt::println("{0}{1}System themes:{2}", "\033[1m", "\033[4m", "\033[0m");
			for (const auto& theme : Theme::getSystemThemes()) {
				fmt::println("{}", theme);
			}

			// Remove paths and extensions so only the theme names are shown
			auto cleanupThemePaths = [](vector<string>&& themePaths) {
				std::sort(themePaths.begin(), themePaths.end());
				for (auto& themePath : themePaths) {
					fs::path path(themePath);
					themePath = path.filename().string();
					themePath = themePath.substr(0, themePath.rfind(".theme"));
				}
				return themePaths;
			};

			fmt::println("\n{0}{1}Bundled themes:{2}", "\033[1m", "\033[4m", "\033[0m");
			for (const auto& theme : cleanupThemePaths(Theme::getBundledThemes())) {
				fmt::println("{}", theme);
			}

			fmt::println("\n{0}{1}User themes:{2}", "\033[1m", "\033[4m", "\033[0m");
			for (const auto& theme : cleanupThemePaths(Theme::getUserThemes())) {
				fmt::println("{}", theme);
			}

			exit(0);
		}
		else if (is_in(argument, "-lc", "--low-color")) {
			Global::arg_low_color = true;
		}
		else if (is_in(argument, "-o", "--option")) {
			if (++i >= argc) {
				fmt::println("{0}error:{1} Option needs an argument\n", "\033[1;31m", "\033[0m");
				print_usage();
				print_help_hint();
				exit(1);
			}
			configOverrides += argv[i];
			configOverrides += '\n';
		}
		else if (is_in(argument, "-t", "--tty_on")) {
			Config::set("tty_mode", true);
			Global::arg_tty = true;
		}
		else if (is_in(argument, "+t", "--tty_off")) {
			Config::set("tty_mode", false);
			Global::arg_tty = true;
		}
		else if (is_in(argument, "-p", "--preset")) {
			if (++i >= argc) {
				fmt::println("{0}error:{1} Preset option needs an argument\n", "\033[1;31m", "\033[0m");
				print_usage();
				print_help_hint();
				exit(1);
			}
			else if (const string val = argv[i]; isint(val) and val.size() == 1) {
				Global::arg_preset = std::clamp(stoi(val), 0, 9);
			}
			else {
				fmt::println("{0}error: {1}Preset option only accepts an integer value between 0-9\n", "\033[1;31m", "\033[0m");
				print_usage();
				print_help_hint();
				exit(1);
			}
		}
		else if (is_in(argument, "-u", "--update")) {
			if (++i >= argc) {
				fmt::println("{0}error:{1} Update option needs an argument\n", "\033[1;31m", "\033[0m");
				print_usage();
				print_help_hint();
				exit(1);
			}
			const std::string value = argv[i];
			if (isint(value)) {
				Global::arg_update = std::clamp(std::stoi(value), 100, Config::ONE_DAY_MILLIS);
			} else {
				fmt::println("{0}error:{1} Invalid update rate\n", "\033[1;31m", "\033[0m");
				print_usage();
				print_help_hint();
				exit(1);
			}
		}
		else if (argument == "--utf-force")
			Global::utf_force = true;
		else if (argument == "--debug")
			Global::debug = true;
		else {
			fmt::println("{0}error:{2} unexpected argument '{1}{3}{2}' found\n", "\033[1;31m", "\033[33m", "\033[0m", argument);
			print_usage();
			print_help_hint();
			exit(1);
		}
	}
}

//* Handler for SIGWINCH and general resizing events, does nothing if terminal hasn't been resized unless force=true
void term_resize(bool force) {
	static atomic<bool> resizing (false);
	if (Input::polling) {
		Global::resized = true;
		Input::interrupt();
		return;
	}
	atomic_lock lck(resizing, true);
	if (auto refreshed = Term::refresh(true); refreshed or force) {
		if (force and refreshed) force = false;
	}
	else return;
	static const array<string, 10> all_boxes = {"npu2", "cpu", "mem", "net", "proc", "gpu0", "gpu1", "gpu2", "npu0", "npu1"};
	Global::resized = true;
	if (Runner::active) Runner::stop();
	Term::refresh();
	Config::unlock();

	auto boxes = Config::getS("shown_boxes");
	auto min_size = Term::get_min_size(boxes);
	auto minWidth = min_size.at(0), minHeight = min_size.at(1);

	while (not force or (Term::width < minWidth or Term::height < minHeight)) {
		sleep_ms(100);
		if (Term::width < minWidth or Term::height < minHeight) {
			int width = Term::width, height = Term::height;
			cout << fmt::format("{clear}{bg_black}{fg_white}"
					"{mv1}Terminal size too small:"
					"{mv2} Width = {fg_width}{width} {fg_white}Height = {fg_height}{height}"
					"{mv3}{fg_white}Needed for current config:"
					"{mv4}Width = {minWidth} Height = {minHeight}",
					"clear"_a = Term::clear, "bg_black"_a = Global::bg_black, "fg_white"_a = Global::fg_white,
					"mv1"_a = Mv::to((height / 2) - 2, (width / 2) - 11),
					"mv2"_a = Mv::to((height / 2) - 1, (width / 2) - 10),
						"fg_width"_a = (width < minWidth ? Global::fg_red : Global::fg_green),
						"width"_a = width,
						"fg_height"_a = (height < minHeight ? Global::fg_red : Global::fg_green),
						"height"_a = height,
					"mv3"_a = Mv::to((height / 2) + 1, (width / 2) - 12),
					"mv4"_a = Mv::to((height / 2) + 2, (width / 2) - 10),
						"minWidth"_a = minWidth,
						"minHeight"_a = minHeight
			) << std::flush;

			bool got_key = false;
			for (; not Term::refresh() and not got_key; got_key = Input::poll(10));
			if (got_key) {
				auto key = Input::get();
				if (key == "q")
					clean_quit(0);
				else if (key.size() == 1 and isint(key)) {
					auto intKey = stoi(key);
					const auto gpu_count = Gpu::get_count();
					const auto npu_count = Npu::get_count();
					if ((intKey >= 5 and intKey <= 7 and intKey - 4 <= gpu_count)) {
						auto box = all_boxes.at(intKey);
						Config::current_preset = -1;
						Config::toggle_box(box);
						boxes = Config::getS("shown_boxes");
					} else if ((intKey == 0 and npu_count >= 3) or (intKey >= 8 and intKey - 7 <= npu_count)) {
						auto box = all_boxes.at(intKey);
						Config::current_preset = -1;
						Config::toggle_box(box);
						boxes = Config::getS("shown_boxes");
					}
				}
			}
			min_size = Term::get_min_size(boxes);
			minWidth = min_size.at(0), minHeight = min_size.at(1);
		}
		else if (not Term::refresh()) break;
	}

	Input::interrupt();
}

//* Exit handler; stops threads, restores terminal and saves config changes
void clean_quit(int sig) {
	if (Global::quitting) return;
	Global::quitting = true;
	Runner::stop();

	if (Global::_runner_started) {
		struct timespec ts;
		ts.tv_sec = 5;
		if (pthread_timedjoin_np(Runner::runner_id, nullptr, &ts) != 0) {
			Logger::warning("Failed to join _runner thread on exit!");
			pthread_cancel(Runner::runner_id);
		}
	}

	Shared::shutdown();

	Config::write();

	if (Term::initialized) {
		Input::clear();
		Term::restore();
	}

	shutdown_plugin();

	if (not Global::exit_error_msg.empty()) {
		sig = 1;
		Logger::error(Global::exit_error_msg);
		fmt::println(std::cerr, "{}ERROR: {}{}{}", Global::fg_red, Global::fg_white, Global::exit_error_msg, Fx::reset);
	}
	Logger::info("Quitting! Runtime: " + sec_to_dhms(time_s() - Global::start_time));

	const auto excode = (sig != -1 ? sig : 0);

	quick_exit(excode);
}

//* Handler for SIGTSTP; stops threads, restores terminal and sends SIGSTOP
void _sleep() {
	Runner::stop();
	Term::restore();
	std::raise(SIGSTOP);
}

//* Handler for SIGCONT; re-initialize terminal and force a resize event
void _resume() {
	Term::init();
	term_resize(true);
}

void _exit_handler() {
	clean_quit(-1);
}

void _signal_handler(const int sig) {
	switch (sig) {
		case SIGINT:
			if (Runner::active) {
				Global::should_quit = true;
				Runner::stopping = true;
				Input::interrupt();
			}
			else {
				clean_quit(0);
			}
			break;
		case SIGTSTP:
			if (Runner::active) {
				Global::should_sleep = true;
				Runner::stopping = true;
				Input::interrupt();
			}
			else {
				_sleep();
			}
			break;
		case SIGCONT:
			_resume();
			break;
		case SIGWINCH:
			term_resize();
			break;
		case SIGUSR1:
			// Input::poll interrupt
			break;
		case SIGUSR2:
			Global::reload_conf = true;
			Input::interrupt();
			break;
	}
}

//* Config init
void init_config(const string& configOverrides) {
	atomic_lock lck(Global::init_conf);
	vector<string> load_warnings;
	Config::load(Config::conf_file, load_warnings);
	if (!configOverrides.empty()) {
		std::istringstream iss(configOverrides);
		Config::loadOverrides(iss, load_warnings);
	}

	Config::set("lowcolor", (Global::arg_low_color ? true : not Config::getB("truecolor")));

	static bool first_init = true;

	if (Global::debug and first_init) {
		Logger::set("DEBUG");
		Logger::debug("Running in DEBUG mode!");
	}
	else Logger::set(Config::getS("log_level"));

	static string log_level;
	if (const string current_level = Config::getS("log_level"); log_level != current_level) {
		log_level = current_level;
		Logger::info("Logger set to " + (Global::debug ? "DEBUG" : log_level));
	}

	for (const auto& err_str : load_warnings) Logger::warning(err_str);
	first_init = false;
}

//* Manages secondary thread for collection and drawing of boxes
namespace Runner {
	atomic<bool> active (false);
	atomic<bool> stopping (false);
	atomic<bool> waiting (false);
	atomic<bool> redraw (false);
	atomic<bool> coreNum_reset (false);

	//* Setup cv for triggering thread to do work
	std::condition_variable do_work;
	std::mutex work_mutex;
	bool triggered (false);
	inline void thread_wait() {
		std::unique_lock<std::mutex> lock(work_mutex);
		do_work.wait(lock, [] { return triggered; });
		triggered = false;
	}
	inline void thread_trigger() {
		std::unique_lock<std::mutex> lock(work_mutex);
		triggered = true;
		do_work.notify_all();
	}

	//* RAII wrapper for pthread_mutex locking
	class thread_lock {
		pthread_mutex_t& pt_mutex;
	public:
		int status;
		thread_lock(pthread_mutex_t& mtx) : pt_mutex(mtx) {
			pthread_mutex_init(&pt_mutex, nullptr);
			status = pthread_mutex_lock(&pt_mutex);
		}
		~thread_lock() {
			if (status == 0)
				pthread_mutex_unlock(&pt_mutex);
		}
	};

	//* Wrapper for raising privileges when using SUID bit
	class gain_priv {
		int status = -1;
	public:
		gain_priv() {
			if (Global::real_uid != Global::set_uid)
				this->status = seteuid(Global::set_uid);
		}
		~gain_priv() {
			if (status == 0)
				status = seteuid(Global::real_uid);
		}
	};

	string output;
	string empty_bg;
	bool pause_output{};
	sigset_t mask;
	pthread_t runner_id;
	pthread_mutex_t mtx;

	enum debug_actions {
		collect_begin,
		collect_done,
		draw_begin,
		draw_begin_only,
		draw_done
	};

	enum debug_array {
		collect,
		draw
	};

	string debug_bg;
	std::unordered_map<string, array<uint64_t, 2>> debug_times;

	class MyNumPunct : public std::numpunct<char>
	{
	protected:
		virtual char do_thousands_sep() const { return '\''; }
		virtual std::string do_grouping() const { return "\03"; }
	};


	struct runner_conf {
		vector<string> boxes;
		bool no_update;
		bool force_redraw;
		bool background_update;
		string overlay;
		string clock;
	};

	struct runner_conf current_conf;

	void debug_timer(const char* name, const int action) {
		switch (action) {
			case collect_begin:
				debug_times[name].at(collect) = time_micros();
				return;
			case collect_done:
				debug_times[name].at(collect) = time_micros() - debug_times[name].at(collect);
				debug_times["total"].at(collect) += debug_times[name].at(collect);
				return;
			case draw_begin_only:
				debug_times[name].at(draw) = time_micros();
				return;
			case draw_begin:
				debug_times[name].at(draw) = time_micros();
				debug_times[name].at(collect) = debug_times[name].at(draw) - debug_times[name].at(collect);
				debug_times["total"].at(collect) += debug_times[name].at(collect);
				return;
			case draw_done:
				debug_times[name].at(draw) = time_micros() - debug_times[name].at(draw);
				debug_times["total"].at(draw) += debug_times[name].at(draw);
				return;
		}
	}

	//? ------------------------------- Secondary thread: async launcher and drawing ----------------------------------
	void * _runner(void *) {
		//? Block some signals in this thread to avoid deadlock from any signal handlers trying to stop this thread
		sigemptyset(&mask);
		// sigaddset(&mask, SIGINT);
		// sigaddset(&mask, SIGTSTP);
		sigaddset(&mask, SIGWINCH);
		sigaddset(&mask, SIGTERM);
		pthread_sigmask(SIG_BLOCK, &mask, nullptr);

		//? pthread_mutex_lock to lock thread and monitor health from main thread
		thread_lock pt_lck(mtx);
		if (pt_lck.status != 0) {
			Global::exit_error_msg = "Exception in runner thread -> pthread_mutex_lock error id: " + to_string(pt_lck.status);
			Global::thread_exception = true;
			Input::interrupt();
			stopping = true;
		}

		//* ----------------------------------------------- THREAD LOOP -----------------------------------------------
		while (not Global::quitting) {
			thread_wait();
			atomic_wait_for(active, true, 5000);
			if (active) {
				Global::exit_error_msg = "Runner thread failed to get active lock!";
				Global::thread_exception = true;
				Input::interrupt();
				stopping = true;
			}
			if (stopping or Global::resized) {
				sleep_ms(1);
				continue;
			}

			//? Atomic lock used for blocking non thread-safe actions in main thread
			atomic_lock lck(active);

			//? Set effective user if SUID bit is set
			gain_priv powers{};

			auto& conf = current_conf;

			//! DEBUG stats
			if (Global::debug) {
                if (debug_bg.empty() or redraw)
                    Runner::debug_bg = Draw::createBox(2, 2, 33, 9, "", true, "μs");

				debug_times.clear();
				debug_times["total"] = {0, 0};
			}

			output.clear();

			//* Run collection and draw functions for all boxes
			try {
				//? GPU data collection
				const bool gpu_in_cpu_panel = Gpu::get_gpu_names().size() > 0 and (
					Config::getS("cpu_graph_lower").starts_with("gpu-") or Config::getS("cpu_graph_upper").starts_with("gpu-")
					or (Gpu::shown == 0 and Config::getS("show_gpu_info") != "Off")
				);

				vector<unsigned int> gpu_panels = {};
				for (auto& box : conf.boxes)
					if (box.starts_with("gpu"))
						gpu_panels.push_back(box.back()-'0');

				vector<Gpu::gpu_info> gpus;
				if (gpu_in_cpu_panel or not gpu_panels.empty()) {
					if (Global::debug) debug_timer("gpu", collect_begin);
					gpus = Gpu::collect(conf.no_update);
					if (Global::debug) debug_timer("gpu", collect_done);
				}
				auto& gpus_ref = gpus;

				//? NPU data collection
				const bool npu_in_cpu_panel = Npu::get_npu_names().size() > 0 and (
					Config::getS("cpu_graph_lower").starts_with("npu-") or Config::getS("cpu_graph_upper").starts_with("npu-")
					or (Npu::shown == 0 and Config::getS("show_npu_info") != "Off")
				);

				vector<unsigned int> npu_panels = {};
				for (auto& box : conf.boxes)
					if (box.starts_with("npu"))
						npu_panels.push_back(box.back()-'0');

				vector<Npu::npu_info> npus;
				if (npu_in_cpu_panel or not npu_panels.empty()) {
					if (Global::debug) debug_timer("npu", collect_begin);
					npus = Npu::collect(conf.no_update);
					if (Global::debug) debug_timer("npu", collect_done);
				}
				auto& npus_ref = npus;

				//? CPU
				if (v_contains(conf.boxes, "cpu")) {
					try {
						if (Global::debug) debug_timer("cpu", collect_begin);

						//? Start collect
						auto cpu = Cpu::collect(conf.no_update);

						if (coreNum_reset) {
							coreNum_reset = false;
							Cpu::update_core_mapping();
							Global::resized = true;
							Input::interrupt();
							continue;
						}

						if (Global::debug) debug_timer("cpu", draw_begin);

						//? Draw box
						if (not pause_output) output += Cpu::draw(cpu, gpus_ref, npus_ref, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("cpu", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Cpu:: -> " + string{e.what()});
					}
				}

				//? GPU
				if (not gpu_panels.empty() and not gpus_ref.empty()) {
					try {
						if (Global::debug) debug_timer("gpu", draw_begin_only);

						//? Draw box
						if (not pause_output)
							for (unsigned long i = 0; i < gpu_panels.size(); ++i)
								output += Gpu::draw(gpus_ref[gpu_panels[i]], i, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("gpu", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Gpu:: -> " + string{e.what()});
					}
				}

				//? NPU
				if (not npu_panels.empty() and not npus_ref.empty()) {
					try {
						if (Global::debug) debug_timer("npu", draw_begin_only);

						//? Draw box
						if (not pause_output)
							for (unsigned long i = 0; i < npu_panels.size(); ++i)
								output += Npu::draw(npus_ref[npu_panels[i]], i, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("npu", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Npu:: -> " + string{e.what()});
					}
				}

				//? MEM
				if (v_contains(conf.boxes, "mem")) {
					try {
						if (Global::debug) debug_timer("mem", collect_begin);

						//? Start collect
						auto mem = Mem::collect(conf.no_update);

						if (Global::debug) debug_timer("mem", draw_begin);

						//? Draw box
						if (not pause_output) output += Mem::draw(mem, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("mem", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Mem:: -> " + string{e.what()});
					}
				}

				//? NET
				if (v_contains(conf.boxes, "net")) {
					try {
						if (Global::debug) debug_timer("net", collect_begin);

						//? Start collect
						auto net = Net::collect(conf.no_update);

						if (Global::debug) debug_timer("net", draw_begin);

						//? Draw box
						if (not pause_output) output += Net::draw(net, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("net", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Net:: -> " + string{e.what()});
					}
				}

				//? PROC
				if (v_contains(conf.boxes, "proc")) {
					try {
						if (Global::debug) debug_timer("proc", collect_begin);

						//? Start collect
						auto proc = Proc::collect(conf.no_update);

						if (Global::debug) debug_timer("proc", draw_begin);

						//? Draw box
						if (not pause_output) output += Proc::draw(proc, conf.force_redraw, conf.no_update);

						if (Global::debug) debug_timer("proc", draw_done);
					}
					catch (const std::exception& e) {
						throw std::runtime_error("Proc:: -> " + string{e.what()});
					}
				}

			}
			catch (const std::exception& e) {
				Global::exit_error_msg = "Exception in runner thread -> " + string{e.what()};
				Global::thread_exception = true;
				Input::interrupt();
				stopping = true;
			}

			if (stopping) {
				continue;
			}

			if (redraw or conf.force_redraw) {
				empty_bg.clear();
				redraw = false;
			}

			if (not pause_output) output += conf.clock;
			if (not conf.overlay.empty() and not conf.background_update) pause_output = true;
			if (output.empty() and not pause_output) {
				if (empty_bg.empty()) {
					const int x = Term::width / 2 - 10, y = Term::height / 2 - 10;
					output += Term::clear;
					empty_bg = fmt::format(
						"{banner}"
						"{mv1}{titleFg}{b}No boxes shown!"
						"{mv2}{hiFg}1 {mainFg}| Show CPU box"
						"{mv3}{hiFg}2 {mainFg}| Show MEM box"
						"{mv4}{hiFg}3 {mainFg}| Show NET box"
						"{mv5}{hiFg}4 {mainFg}| Show PROC box"
						"{mv6}{hiFg}5-0 {mainFg}| Show GPU boxes"
						"{mv7}{hiFg}esc {mainFg}| Show menu"
						"{mv8}{hiFg}q {mainFg}| Quit",
						"banner"_a = Draw::banner_gen(y, 0, true),
						"titleFg"_a = Theme::c("title"), "b"_a = Fx::b, "hiFg"_a = Theme::c("hi_fg"), "mainFg"_a = Theme::c("main_fg"),
						"mv1"_a = Mv::to(y+6, x),
						"mv2"_a = Mv::to(y+8, x),
						"mv3"_a = Mv::to(y+9, x),
						"mv4"_a = Mv::to(y+10, x),
						"mv5"_a = Mv::to(y+11, x),
						"mv6"_a = Mv::to(y+12, x-2),
						"mv7"_a = Mv::to(y+13, x-2),
						"mv8"_a = Mv::to(y+14, x)
					);
				}
				output += empty_bg;
			}

			//! DEBUG stats -->
			if (Global::debug and not Menu::active) {
				output += fmt::format("{pre}{box:5.5} {collect:>12.12} {draw:>12.12}{post}",
					"pre"_a = debug_bg + Theme::c("title") + Fx::b,
					"box"_a = "box", "collect"_a = "collect", "draw"_a = "draw",
					"post"_a = Theme::c("main_fg") + Fx::ub
				);
				static auto loc = std::locale(std::locale::classic(), new MyNumPunct);
				for (const string name : {"cpu", "mem", "net", "proc", "gpu", "total"}) {
					if (not debug_times.contains(name)) debug_times[name] = {0,0};
					const auto& [time_collect, time_draw] = debug_times.at(name);
					if (name == "total") output += Fx::b;
					output += fmt::format(loc, "{mvLD}{name:5.5} {collect:12L} {draw:12L}",
						"mvLD"_a = Mv::l(31) + Mv::d(1),
						"name"_a = name,
						"collect"_a = time_collect,
						"draw"_a = time_draw
					);
				}
			}

			//? If overlay isn't empty, print output without color and then print overlay on top
			cout << Term::sync_start << (conf.overlay.empty()
					? output
					: (output.empty() ? "" : Fx::ub + Theme::c("inactive_fg") + Fx::uncolor(output)) + conf.overlay)
				<< Term::sync_end << flush;
		}
		//* ----------------------------------------------- THREAD LOOP -----------------------------------------------
		return {};
	}
	//? ------------------------------------------ Secondary thread end -----------------------------------------------

	//* Runs collect and draw in a secondary thread, unlocks and locks config to update cached values
	void run(const string& box, bool no_update, bool force_redraw) {
		atomic_wait_for(active, true, 5000);
		if (active) {
			Logger::error("Stall in runner thread, exiting!");
			active = false;
			Global::exit_error_msg = "Stall in runner thread!";
			clean_quit(1);
		}
		if (stopping or Global::resized) return;

		if (box == "overlay") {
			cout << Term::sync_start << Global::overlay << Term::sync_end << flush;
		}
		else if (box == "clock") {
			cout << Term::sync_start << Global::clock << Term::sync_end << flush;
		}
		else {
			Config::unlock();
			Config::lock();

			current_conf = {
				(box == "all" ? Config::current_boxes : vector{box}),
				no_update, force_redraw,
				(not Config::getB("tty_mode") and Config::getB("background_update")),
				Global::overlay,
				Global::clock
			};

			if (Menu::active and not current_conf.background_update) Global::overlay.clear();

			thread_trigger();
			atomic_wait_for(active, false, 10);
		}


	}

	//* Stops any work being done in runner thread and checks for thread errors
	void stop() {
		stopping = true;
		int ret = pthread_mutex_trylock(&mtx);
		if (ret != EBUSY and not Global::quitting) {
			if (active) active = false;
			Global::exit_error_msg = "Runner thread died unexpectedly!";
			clean_quit(1);
		}
		else if (ret == EBUSY) {
			atomic_wait_for(active, true, 5000);
			if (active) {
				active = false;
				if (Global::quitting) {
					return;
				}
				else {
					Global::exit_error_msg = "No response from Runner thread, quitting!";
					clean_quit(1);
				}
			}
			thread_trigger();
			atomic_wait_for(active, false, 100);
			atomic_wait_for(active, true, 100);
		}
		stopping = false;
	}

}


//* --------------------------------------------- Main starts here! ---------------------------------------------------
int main(int argc, char **argv) {
	ShowCrashReports();

	//? ------------------------------------------------ INIT ---------------------------------------------------------

	Global::start_time = time_s();

	//? Save real and effective userid's and drop privileges until needed if running with SUID bit set
	Global::real_uid = getuid();
	Global::set_uid = geteuid();
	if (Global::real_uid != Global::set_uid) {
		if (seteuid(Global::real_uid) != 0) {
			Global::real_uid = Global::set_uid;
			Global::exit_error_msg = "Failed to change effective user ID. Unset cosmotop SUID bit to ensure security on this system. Quitting!";
			clean_quit(1);
		}
	}

	//? Call argument parser if launched with arguments
	string configOverrides;
	if (argc > 1) argumentParser(argc, argv, configOverrides);

	//? Config dir init
	init_config_dir();

	//? Config init
	init_config(configOverrides);

	//? Plugin init
	create_plugin_host();

	//? Try to find and set a UTF-8 locale
	if (std::setlocale(LC_ALL, "") != nullptr and not s_contains((string)std::setlocale(LC_ALL, ""), ";")
		and str_to_upper(s_replace((string)std::setlocale(LC_ALL, ""), "-", "")).ends_with("UTF8")) {
		Logger::debug("Using locale " + (string)std::setlocale(LC_ALL, ""));
	} else {
		string found;
		bool set_failure{};
		for (const auto loc_env : array{"LANG", "LC_ALL", "LC_CTYPE"}) {
			if (std::getenv(loc_env) != nullptr and str_to_upper(s_replace((string)std::getenv(loc_env), "-", "")).ends_with("UTF8")) {
				found = std::getenv(loc_env);
				if (std::setlocale(LC_ALL, found.c_str()) == nullptr) {
					set_failure = true;
					Logger::warning("Failed to set locale " + found + " continuing anyway.");
				}
			}
		}
		if (found.empty()) {
			if (setenv("LC_ALL", "", 1) == 0 and setenv("LANG", "", 1) == 0) {
				try {
					if (const auto loc = std::locale("").name(); not loc.empty() and loc != "*") {
						for (auto& l : ssplit(loc, ';')) {
							if (str_to_upper(s_replace(l, "-", "")).ends_with("UTF8")) {
								found = l.substr(l.find('=') + 1);
								if (std::setlocale(LC_ALL, found.c_str()) != nullptr) {
									break;
								}
							}
						}
					}
				}
				catch (...) { found.clear(); }
			}
		}
		if (found.empty() and IsWindows())
			Logger::warning("No UTF-8 locale detected! Assuming we can continue on Windows.");
		else if (found.empty() and Global::utf_force)
			Logger::warning("No UTF-8 locale detected! Forcing start with --utf-force argument.");
		else if (found.empty()) {
			Global::exit_error_msg = "No UTF-8 locale detected!\nUse --utf-force argument to force start if you're sure your terminal can handle it.";
			clean_quit(1);
		}
		else if (not set_failure)
			Logger::debug("Setting LC_ALL=" + found);
	}

	//? Initialize terminal and set options
	if (not Term::init()) {
		Global::exit_error_msg = "No tty detected!\ncosmotop needs an interactive shell to run.";
		clean_quit(1);
	}

	if (Term::current_tty != "unknown") Logger::info("Running on " + Term::current_tty);
	if (not Global::arg_tty and Config::getB("force_tty")) {
		Config::set("tty_mode", true);
		Logger::info("Forcing tty mode: setting 16 color mode and using tty friendly graph symbols");
	}
	else if (not Global::arg_tty and Term::current_tty.starts_with("/dev/tty")) {
		Config::set("tty_mode", true);
		Logger::info("Real tty detected: setting 16 color mode and using tty friendly graph symbols");
	}

	//? Check for valid terminal dimensions
	{
		int t_count = 0;
		while (Term::width <= 0 or Term::width > 10000 or Term::height <= 0 or Term::height > 10000) {
			sleep_ms(10);
			Term::refresh();
			if (++t_count == 100) {
				Global::exit_error_msg = "Failed to get size of terminal!";
				clean_quit(1);
			}
		}
	}

	//? Platform dependent init and error check
	try {
		Shared::init();
	}
	catch (const std::exception& e) {
		Global::exit_error_msg = "Exception in Shared::init() -> " + string{e.what()};
		clean_quit(1);
	}

	if (not Config::set_boxes(Config::getS("shown_boxes"))) {
		Config::set_boxes("cpu mem net proc");
		Config::set("shown_boxes", "cpu mem net proc"s);
	}

	//? Update list of available themes and generate the selected theme
	Theme::updateThemes();
	Theme::setTheme();

	//? Setup signal handlers for CTRL-C, CTRL-Z, resume and terminal resize
	std::atexit(_exit_handler);
	std::signal(SIGINT, _signal_handler);
	std::signal(SIGTSTP, _signal_handler);
	std::signal(SIGCONT, _signal_handler);
	std::signal(SIGWINCH, _signal_handler);
	std::signal(SIGUSR1, _signal_handler);
	std::signal(SIGUSR2, _signal_handler);

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &mask, &Input::signal_mask);

	//? Start runner thread
	if (pthread_create(&Runner::runner_id, nullptr, &Runner::_runner, nullptr) != 0) {
		Global::exit_error_msg = "Failed to create _runner thread!";
		clean_quit(1);
	}
	else {
		Global::_runner_started = true;
	}

	//? Calculate sizes of all boxes
	Config::presetsValid(Config::getS("presets"));
	if (Global::arg_preset >= 0) {
		Config::current_preset = min(Global::arg_preset, (int)Config::preset_list.size() - 1);
		Config::apply_preset(Config::preset_list.at(Config::current_preset));
	}

	{
		const auto [x, y] = Term::get_min_size(Config::getS("shown_boxes"));
		if (Term::height < y or Term::width < x) {
			pthread_sigmask(SIG_SETMASK, &Input::signal_mask, &mask);
			term_resize(true);
			pthread_sigmask(SIG_SETMASK, &mask, nullptr);
			Global::resized = false;
		}

	}

	Draw::calcSizes();

	//? Print out box outlines
	cout << Term::sync_start << Cpu::box << Mem::box << Net::box << Proc::box << Term::sync_end << flush;


	//? ------------------------------------------------ MAIN LOOP ----------------------------------------------------

	if (Global::arg_update != 0) {
		Config::set("update_ms", Global::arg_update);
	}
	uint64_t update_ms = Config::getI("update_ms");
	auto future_time = time_ms();

	try {
		while (not true not_eq not false) {
			//? Check for exceptions in secondary thread and exit with fail signal if true
			if (Global::thread_exception) {
				clean_quit(1);
			}
			else if (Global::should_quit) {
				clean_quit(0);
			}
			else if (Global::should_sleep) {
				Global::should_sleep = false;
				_sleep();
			}
			//? Hot reload config from CTRL + R or SIGUSR2
			else if (Global::reload_conf) {
				Global::reload_conf = false;
				if (Runner::active) Runner::stop();
				Config::unlock();
				init_config(""s);
				Theme::updateThemes();
				Theme::setTheme();
				Draw::banner_gen(0, 0, false, true);
				Global::resized = true;
			}

			//? Make sure terminal size hasn't changed (in case of SIGWINCH not working properly)
			term_resize(Global::resized);

			//? Trigger secondary thread to redraw if terminal has been resized
			if (Global::resized) {
				Draw::calcSizes();
				Draw::update_clock(true);
				Global::resized = false;
				if (Menu::active) Menu::process();
				else Runner::run("all", true, true);
				atomic_wait_for(Runner::active, true, 1000);
			}

			//? Update clock if needed
			if (Draw::update_clock() and not Menu::active) {
				Runner::run("clock");
			}

			//? Start secondary collect & draw thread at the interval set by <update_ms> config value
			if (time_ms() >= future_time and not Global::resized) {
				Runner::run("all");
				update_ms = Config::getI("update_ms");
				future_time = time_ms() + update_ms;
			}

			//? Loop over input polling and input action processing
			for (auto current_time = time_ms(); current_time < future_time; current_time = time_ms()) {

				//? Check for external clock changes and for changes to the update timer
				if (std::cmp_not_equal(update_ms, Config::getI("update_ms"))) {
					update_ms = Config::getI("update_ms");
					future_time = time_ms() + update_ms;
				}
				else if (future_time - current_time > update_ms) {
					future_time = current_time;
				}
				//? Poll for input and process any input detected
				else if (Input::poll(min((uint64_t)1000, future_time - current_time))) {
					if (not Runner::active) Config::unlock();

					if (Menu::active) Menu::process(Input::get());
					else Input::process(Input::get());
				}

				//? Break the loop at 1000ms intervals or if input polling was interrupted
				else break;

			}

		}
	}
	catch (const std::exception& e) {
		Global::exit_error_msg = "Exception in main loop -> " + string{e.what()};
		clean_quit(1);
	}

}
