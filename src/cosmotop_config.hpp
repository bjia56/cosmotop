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

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <istream>
#include <unordered_map>

using std::string;
using std::vector;

//* Functions and variables for reading and writing the cosmotop config file
namespace Config {

	extern std::filesystem::path conf_dir;
	extern std::filesystem::path conf_file;

	extern std::unordered_map<std::string_view, string> strings;
	extern std::unordered_map<std::string_view, string> stringsTmp;
	extern std::unordered_map<std::string_view, string> stringsOverrides;
	extern std::unordered_map<std::string_view, bool> bools;
	extern std::unordered_map<std::string_view, bool> boolsTmp;
	extern std::unordered_map<std::string_view, bool> boolsOverrides;
	extern std::unordered_map<std::string_view, int> ints;
	extern std::unordered_map<std::string_view, int> intsTmp;
	extern std::unordered_map<std::string_view, int> intsOverrides;

	const vector<string> valid_graph_symbols = { "braille", "block", "tty" };
	const vector<string> valid_graph_symbols_def = { "default", "braille", "block", "tty" };
	const vector<string> valid_boxes = {
		"cpu", "mem", "net", "proc",
		"gpu0", "gpu1", "gpu2", "gpu3", "gpu4", "gpu5",
		"npu0", "npu1", "npu2",
	};
	const vector<string> temp_scales = { "celsius", "fahrenheit", "kelvin", "rankine" };
	const vector<string> show_gpu_values = { "Auto", "On", "Off" };
	const vector<string> show_npu_values = { "Auto", "On", "Off" };
	extern vector<string> current_boxes;
	extern vector<string> preset_list;
	extern vector<string> available_batteries;
	extern int current_preset;

	void push_back_available_batteries(const string& battery);
	std::unordered_map<std::string, int>& get_ints();
	std::unordered_map<std::string, bool>& get_bools();
	std::unordered_map<std::string, string>& get_strings();
	void ints_set_at(const std::string_view name, const int value);

	constexpr int ONE_DAY_MILLIS = 1000 * 60 * 60 * 24;

	[[nodiscard]] std::optional<std::filesystem::path> get_config_dir() noexcept;

	//* Check if string only contains space separated valid names for boxes and set current_boxes
	bool set_boxes(const string& boxes);

	bool validBoxSizes(const string& boxes);

	//* Toggle box and update config string shown_boxes
	bool toggle_box(const string& box);

	//* Parse and setup config value presets
	bool presetsValid(const string& presets);

	//* Apply selected preset
	bool apply_preset(const string& preset);

	bool _locked(const std::string_view name);

#ifdef __COSMOPOLITAN__
	//* Return bool for config key <name>
	const bool& getB(const std::string_view name);

	//* Return integer for config key <name>
	const int& getI(const std::string_view name);

	//* Return string for config key <name>
	const string& getS(const std::string_view name);
#else
	// Versions of getB, getI, and getS using std::string
	const bool& getB(const string& name);
	const int& getI(const string& name);
	const string& getS(const string& name);
#endif // __COSMOPOLITAN__

	string getAsString(const std::string_view name);

	extern string validError;

	bool intValid(const std::string_view name, const string& value);
	bool stringValid(const std::string_view name, const string& value);

	//* Set config key <name> to bool <value>
	inline void set(const std::string_view name, bool value) {
		if (_locked(name)) {
			boolsTmp.insert_or_assign(name, value);
		} else {
			bools.at(name) = value;
			if (boolsOverrides.contains(name)) boolsOverrides.erase(name);
		}
	}

	//* Set config key <name> to int <value>
	inline void set(const std::string_view name, const int value) {
		if (_locked(name)) {
			intsTmp.insert_or_assign(name, value);
		} else {
			ints.at(name) = value;
			if (intsOverrides.contains(name)) intsOverrides.erase(name);
		}
	}

	//* Set config key <name> to string <value>
	inline void set(const std::string_view name, const string& value) {
		if (_locked(name)) {
			stringsTmp.insert_or_assign(name, value);
		} else {
			strings.at(name) = value;
			if (stringsOverrides.contains(name)) stringsOverrides.erase(name);
		}
	}

	//* Flip config key bool <name>
	void flip(const std::string_view name);

	//* Lock config and cache changes until unlocked
	void lock();

	//* Unlock config and write any cached values to config
	void unlock();

	//* Load the config file from disk
	void load(const std::filesystem::path& conf_file, vector<string>& load_warnings);

	//* Load config overrides from a stream
	void loadOverrides(std::istream& conf, vector<string>& load_warnings);

	//* Write the config file to disk
	void write();

	//* Write the config file to a stream
	void write(std::ostream& cwrite);
}
