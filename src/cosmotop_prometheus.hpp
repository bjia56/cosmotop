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

#pragma once

#include "cosmotop_shared.hpp"
#include "cosmotop_tools.hpp"
#include "cosmotop_config.hpp"

#include <ctre.hpp>
#include <httplib.h>

#include <memory>
#include <string>
#include <vector>

#ifdef __COSMOPOLITAN__
#error "This file should not be included in the host binary build."
#endif

namespace Prometheus {

	std::unordered_map<string_view, std::string> prometheus_gpu_metric_names = {
		{"gpu_utilization_percent", "gpu_utilization_percent"},
		{"gpu_frequency", "gpu_frequency"},
		{"gpu_power_usage", "gpu_power_usage"},
		{"gpu_temperature", "gpu_temperature"},
	};
	std::unordered_map<string_view, std::string> prometheus_npu_metric_names = {
		{"npu_utilization_percent", "npu_utilization_percent"},
	};

	std::unordered_map<string_view, std::string> prometheus_gpu_settings = {
		{"gpu_frequency_unit", "MHz"},
		{"gpu_power_unit", "W"},
		{"gpu_temperature_unit", "C"},
	};
	std::unordered_map<string_view, std::string> prometheus_npu_settings = {};

	bool prometheus_enabled = false;
	std::unique_ptr<httplib::Client> prometheus_client;
	std::string prometheus_path;

	unsigned gpu_count = 0;
	unsigned npu_count = 0;

	enum class FrequencyUnit {
		MHz,
		GHz,
	};

	enum class PowerUnit {
		mW,
		W,
	};

	enum class TemperatureUnit {
		C,
		F,
		K,
	};

	std::optional<FrequencyUnit> inline normalize_frequency_unit(const std::string& unit) {
		std::string upper_unit = Tools::str_to_upper(unit);
		if (upper_unit == "MHZ") {
			return FrequencyUnit::MHz;
		} else if (upper_unit == "GHZ") {
			return FrequencyUnit::GHz;
		} else {
			return std::nullopt;
		}
	}

	std::optional<PowerUnit> inline normalize_power_unit(const std::string& unit) {
		std::string upper_unit = Tools::str_to_upper(unit);
		if (upper_unit == "MW") {
			return PowerUnit::mW;
		} else if (upper_unit == "W") {
			return PowerUnit::W;
		} else {
			return std::nullopt;
		}
	}

	std::optional<TemperatureUnit> inline normalize_temperature_unit(const std::string& unit) {
		std::string upper_unit = Tools::str_to_upper(unit);
		if (upper_unit == "C") {
			return TemperatureUnit::C;
		} else if (upper_unit == "F") {
			return TemperatureUnit::F;
		} else if (upper_unit == "K") {
			return TemperatureUnit::K;
		} else {
			return std::nullopt;
		}
	}

	// Helper function to extract metric value from Prometheus text format
	std::optional<double> extract_metric(const vector<string_view>& lines, const string& metric_name) {
		// Prometheus text format: metric_name{labels} value timestamp
		for (const auto& line : lines) {
			// Skip comment lines
			if (line.empty() || line[0] == '#') continue;

			// Check if line starts with metric name
			if (!line.starts_with(metric_name)) continue;

			// Find where the value starts (after metric name and optional labels)
			size_t value_start = metric_name.length();

			// Skip optional labels {...}
			if (value_start < line.size() && line[value_start] == '{') {
				size_t label_end = line.find('}', value_start);
				if (label_end == string_view::npos) continue;
				value_start = label_end + 1;
			}

			// Skip whitespace
			while (value_start < line.size() && (line[value_start] == ' ' || line[value_start] == '\t')) {
				value_start++;
			}

			// Find end of value (whitespace or end of line)
			size_t value_end = value_start;
			while (value_end < line.size() && line[value_end] != ' ' && line[value_end] != '\t') {
				value_end++;
			}

			if (value_end > value_start) {
				try {
					return std::stod(string(line.substr(value_start, value_end - value_start)));
				} catch (...) {
					continue;
				}
			}
		}
		return std::nullopt;
	}

	template <bool is_init>
	bool collect_gpu(Gpu::gpu_info* gpus_slice) {
		if (!prometheus_enabled || gpu_count == 0) return false;

		auto response = prometheus_client->Get(prometheus_path);
		if (!response || response->status != 200) {
			if constexpr (!is_init) {
				Logger::warning("Prometheus: Failed to fetch GPU metrics (status: " +
							   (response ? std::to_string(response->status) : "no response") + ")");
			}
			return false;
		}

		// Split response into lines for easier parsing
		auto lines = Tools::ssplit<string_view>(response->body, '\n');

		if constexpr (is_init) {
			// Validation pass: double-check metrics are parseable
			if (gpus_slice[0].supported_functions.gpu_utilization) {
				if (!extract_metric(lines, prometheus_gpu_metric_names["gpu_utilization_percent"]).has_value()) {
					gpus_slice[0].supported_functions.gpu_utilization = false;
					Logger::warning("Prometheus: gpu_utilization_percent found but not parseable");
				}
			}
			if (gpus_slice[0].supported_functions.gpu_clock) {
				if (!extract_metric(lines, prometheus_gpu_metric_names["gpu_frequency"]).has_value()) {
					gpus_slice[0].supported_functions.gpu_clock = false;
					Logger::warning("Prometheus: gpu_frequency found but not parseable");
				}
			}
			if (gpus_slice[0].supported_functions.pwr_usage) {
				if (!extract_metric(lines, prometheus_gpu_metric_names["gpu_power_usage"]).has_value()) {
					gpus_slice[0].supported_functions.pwr_usage = false;
					Logger::warning("Prometheus: gpu_power_usage found but not parseable");
				}
			}
			if (gpus_slice[0].supported_functions.temp_info) {
				if (!extract_metric(lines, prometheus_gpu_metric_names["gpu_temperature"]).has_value()) {
					gpus_slice[0].supported_functions.temp_info = false;
					Logger::warning("Prometheus: gpu_temperature found but not parseable");
				}
			}
		}

		// Collect actual values only for supported functions
		if (gpus_slice[0].supported_functions.gpu_utilization) {
			if (auto val = extract_metric(lines, prometheus_gpu_metric_names["gpu_utilization_percent"])) {
				gpus_slice[0].gpu_percent.at("gpu-totals").push_back(
					static_cast<long long>(*val)
				);
			}
		}

		if (gpus_slice[0].supported_functions.gpu_clock) {
			if (auto val = extract_metric(lines, prometheus_gpu_metric_names["gpu_frequency"])) {
				const auto& unit = prometheus_gpu_settings["gpu_frequency_unit"];
				gpus_slice[0].gpu_clock_speed =
					(unit == "GHz") ? static_cast<unsigned int>(*val * 1000) : static_cast<unsigned int>(*val);
			}
		}

		if (gpus_slice[0].supported_functions.pwr_usage) {
			if (auto val = extract_metric(lines, prometheus_gpu_metric_names["gpu_power_usage"])) {
				const auto& unit = prometheus_gpu_settings["gpu_power_unit"];
				gpus_slice[0].pwr_usage =
					(unit == "W") ? static_cast<long long>(*val * 1000) : static_cast<long long>(*val);
			}
		}

		if (gpus_slice[0].supported_functions.temp_info) {
			if (auto val = extract_metric(lines, prometheus_gpu_metric_names["gpu_temperature"])) {
				const auto& unit = prometheus_gpu_settings["gpu_temperature_unit"];
				long long temp_val = static_cast<long long>(*val);
				if (unit == "F") {
					temp_val = (temp_val - 32) * 5 / 9; // Convert F to C
				} else if (unit == "K") {
					temp_val = temp_val - 273; // Convert K to C
				}
				gpus_slice[0].temp.push_back(temp_val);
			}
		}

		return true;
	}

	template <bool is_init>
	bool collect_npu(Npu::npu_info* npus_slice) {
		if (!prometheus_enabled || npu_count == 0) return false;

		auto response = prometheus_client->Get(prometheus_path);
		if (!response || response->status != 200) {
			if constexpr (!is_init) {
				Logger::warning("Prometheus: Failed to fetch NPU metrics (status: " +
							   (response ? std::to_string(response->status) : "no response") + ")");
			}
			return false;
		}

		// Split response into lines for easier parsing
		auto lines = Tools::ssplit<string_view>(response->body, '\n');

		if constexpr (is_init) {
			// Validation pass: double-check metric is parseable
			if (npus_slice[0].supported_functions.npu_utilization) {
				if (!extract_metric(lines, prometheus_npu_metric_names["npu_utilization_percent"]).has_value()) {
					npus_slice[0].supported_functions.npu_utilization = false;
					Logger::warning("Prometheus: npu_utilization_percent found but not parseable");
				}
			}
		}

		// Collect actual value
		if (npus_slice[0].supported_functions.npu_utilization) {
			if (auto val = extract_metric(lines, prometheus_npu_metric_names["npu_utilization_percent"])) {
				npus_slice[0].npu_percent.at("npu-totals").push_back(
					static_cast<long long>(*val)
				);
			}
		}

		return true;
	}

	void init(
		vector<Gpu::gpu_info>& gpus,
		vector<string>& gpu_names,
		vector<Npu::npu_info>& npus,
		vector<string>& npu_names
	) {
		const auto& prometheus_endpoint = Config::getS("prometheus_endpoint");
		if (prometheus_endpoint.empty()) {
			return;
		}

		const auto& prometheus_mapping = Config::getS("prometheus_mapping");
		if (!prometheus_mapping.empty()) {
			auto parts = Tools::ssplit<string_view>(prometheus_mapping, ',');
			for (const auto& part : parts) {
				auto kv = Tools::ssplit<string_view>(part, ':');
				if (kv.size() != 2) {
					continue;
				}
				if (prometheus_gpu_metric_names.find(kv[0]) != prometheus_gpu_metric_names.end()) {
					prometheus_gpu_metric_names[kv[0]] = kv[1];
				} else {
					Logger::error("Unknown metric name in prometheus_mapping: " + string(kv[0]));
				}
			}
		}

		const auto& prometheus_settings = Config::getS("prometheus_settings");
		if (!prometheus_settings.empty()) {
			auto parts = Tools::ssplit<string_view>(prometheus_settings, ',');
			for (const auto& part : parts) {
				auto kv = Tools::ssplit<string_view>(part, ':');
				if (kv.size() != 2) {
					continue;
				}
				if (prometheus_gpu_settings.find(kv[0]) != prometheus_gpu_settings.end()) {
					prometheus_gpu_settings[kv[0]] = kv[1];
				} else {
					Logger::error("Unknown setting name in prometheus_settings: " + string(kv[0]));
				}
			}
		}

		// Verify units can be normalized
		if (auto freq_unit = normalize_frequency_unit(prometheus_gpu_settings["gpu_frequency_unit"])) {
			prometheus_gpu_settings["gpu_frequency_unit"] =
				(*freq_unit == FrequencyUnit::MHz) ? "MHz" : "GHz";
		} else {
			Logger::error("Invalid gpu_frequency_unit in prometheus_settings: " +
						 prometheus_gpu_settings["gpu_frequency_unit"]);
			return;
		}
		if (auto power_unit = normalize_power_unit(prometheus_gpu_settings["gpu_power_unit"])) {
			prometheus_gpu_settings["gpu_power_unit"] =
				(*power_unit == PowerUnit::mW) ? "mW" : "W";
		} else {
			Logger::error("Invalid gpu_power_unit in prometheus_settings: " +
						 prometheus_gpu_settings["gpu_power_unit"]);
			return;
		}
		if (auto temp_unit = normalize_temperature_unit(prometheus_gpu_settings["gpu_temperature_unit"])) {
			prometheus_gpu_settings["gpu_temperature_unit"] =
				(*temp_unit == TemperatureUnit::C) ? "C" :
				(*temp_unit == TemperatureUnit::F) ? "F" : "K";
		} else {
			Logger::error("Invalid gpu_temperature_unit in prometheus_settings: " +
						 prometheus_gpu_settings["gpu_temperature_unit"]);
			return;
		}

		// Match: scheme://host:port/path or scheme://host/path
		// Captures: 1=scheme://host:port or scheme://host, 2=path (if present)
		if (const auto m = ctre::match<R"(^((?:https?://)?[^/]+)(/.*)?$)">(prometheus_endpoint)) {
			const auto scheme_host_port = m.get<1>().to_string();
			const auto path = m.get<2>() ? m.get<2>().to_string() : std::string("");

			prometheus_client = std::make_unique<httplib::Client>(scheme_host_port);
			prometheus_path = path;

			// Test endpoint by sending a request
			auto test_response = prometheus_client->Get(prometheus_path);
			if (!test_response || test_response->status != 200) {
				Logger::error("Prometheus endpoint test failed: " +
							 (test_response ? std::to_string(test_response->status) : "no response"));
				prometheus_enabled = false;
				return;
			}

			// Helper function to check if metric exists in response
			auto has_metric = [&](const string& metric_name) -> bool {
				return test_response->body.find(metric_name) != string::npos;
			};

			// Check for GPU metrics and track which ones exist
			bool has_gpu_utilization = has_metric(prometheus_gpu_metric_names["gpu_utilization_percent"]);
			bool has_gpu_frequency = has_metric(prometheus_gpu_metric_names["gpu_frequency"]);
			bool has_gpu_power = has_metric(prometheus_gpu_metric_names["gpu_power_usage"]);
			bool has_gpu_temp = has_metric(prometheus_gpu_metric_names["gpu_temperature"]);

			bool has_any_gpu_metrics = has_gpu_utilization || has_gpu_frequency ||
									   has_gpu_power || has_gpu_temp;

			// Add virtual GPU device only if at least one metric exists
			if (has_any_gpu_metrics) {
				gpu_count = 1;
				gpus.resize(gpus.size() + 1);
				gpu_names.push_back("Prometheus-monitored GPU");

				auto& new_gpu = gpus.back();

				// Set supported functions based on available metrics
				new_gpu.supported_functions = {
					has_gpu_utilization,	// gpu_utilization
					false,					// mem_utilization
					has_gpu_frequency,		// gpu_clock
					false,					// mem_clock
					has_gpu_power,			// pwr_usage
					false,					// pwr_state
					has_gpu_temp,			// temp_info
					false,					// mem_total
					false,					// mem_used
					false					// pcie_txrx
				};

				// Call initialization collection to verify and potentially refine
				collect_gpu<true>(&new_gpu);

				Logger::debug("Added Prometheus GPU device with " +
							 std::to_string(static_cast<int>(has_gpu_utilization) +
										  static_cast<int>(has_gpu_frequency) +
										  static_cast<int>(has_gpu_power) +
										  static_cast<int>(has_gpu_temp)) + " metrics");
			}

			// Check for NPU metrics
			bool has_npu_utilization = has_metric(prometheus_npu_metric_names["npu_utilization_percent"]);

			if (has_npu_utilization) {
				npu_count = 1;
				npus.resize(npus.size() + 1);
				npu_names.push_back("Prometheus-monitored NPU");

				auto& new_npu = npus.back();

				new_npu.supported_functions = {
					has_npu_utilization  // npu_utilization
				};

				collect_npu<true>(&new_npu);

				Logger::debug("Added Prometheus NPU device");
			}

			prometheus_enabled = true;
			Logger::debug("Prometheus client initialized at endpoint " + scheme_host_port + " with path " + path);
		} else {
			// Invalid format
			Logger::error("Prometheus endpoint format invalid: " + prometheus_endpoint);
			return;
		}
	}

}