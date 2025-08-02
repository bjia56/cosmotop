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
#include <ctime>
#include <iostream>
#include <algorithm>
#include <string>

#include <Entity/Server.h>
#include <Task/BasicTask.h>
#include <Message/Request.h>
#include <Public/PublicDef.h>
#include <Public/StringHelper.h>

#include "cosmotop_mcp.hpp"
#include "cosmotop_config.hpp"
#include "cosmotop_shared.hpp"
#include "cosmotop_plugin.hpp"
#include "cosmotop_tools.hpp"

namespace TinyMCP = MCP;

namespace Mcp {

// Task classes for each tool
class ProcessToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_process_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get list of running processes";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"EOF(
{
	"type": "object",
	"properties": {
		"filter_name": {
			"type": "string",
			"description": "Filter processes by name (partial match)"
		},
		"filter_user": {
			"type": "string",
			"description": "Filter processes by user"
		},
		"min_cpu": {
			"type": "number",
			"description": "Minimum CPU percentage"
		},
		"min_memory": {
			"type": "number",
			"description": "Minimum memory usage in bytes"
		},
		"sort_by": {
			"type": "string",
			"enum": ["cpu", "memory", "name", "pid", "user"],
			"description": "Sort processes by field",
			"default": "pid"
		},
		"limit": {
			"type": "integer",
			"description": "Maximum number of processes to return (-1 for all)",
			"default": -1
		}
	},
	"required": []
}
)EOF";

	ProcessToolTask(const std::shared_ptr<TinyMCP::Request>& spRequest)
		: ProcessCallToolRequest(spRequest) {}

	std::shared_ptr<TinyMCP::CMCPTask> Clone() const override {
		auto spClone = std::make_shared<ProcessToolTask>(nullptr);
		if (spClone) {
			*spClone = *this;
		}
		return spClone;
	}

	int Cancel() override {
		return TinyMCP::ERRNO_OK;
	}

	int Execute() override {
		int iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		if (!IsValid())
			return iErrCode;

		auto spCallToolRequest = std::dynamic_pointer_cast<TinyMCP::CallToolRequest>(m_spRequest);
		if (!spCallToolRequest || spCallToolRequest->strName.compare(TOOL_NAME) != 0)
			goto PROC_END;

		try {
			if (!is_plugin_loaded()) {
				iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
				goto PROC_END;
			}

			trigger_plugin_refresh();
			iErrCode = TinyMCP::ERRNO_OK;

		} catch (const std::exception& e) {
			iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		}

	PROC_END:
		auto spExecuteResult = BuildResult();
		if (spExecuteResult) {
			TinyMCP::TextContent textContent;
			textContent.strType = TinyMCP::CONST_TEXT;
			if (TinyMCP::ERRNO_OK == iErrCode) {
				spExecuteResult->bIsError = false;
				std::ostringstream result;
				if (!is_plugin_loaded()) {
					result << "Plugin not loaded";
				} else {
					try {
						// Parse input arguments
						string filter_name, filter_user, sort_by = "pid";
						double min_cpu = 0.0;
						uint64_t min_memory = 0;
						int limit = -1;

						if (spCallToolRequest->jArguments.isObject()) {
							if (spCallToolRequest->jArguments.isMember("filter_name") &&
								spCallToolRequest->jArguments["filter_name"].isString()) {
								filter_name = spCallToolRequest->jArguments["filter_name"].asString();
							}
							if (spCallToolRequest->jArguments.isMember("filter_user") &&
								spCallToolRequest->jArguments["filter_user"].isString()) {
								filter_user = spCallToolRequest->jArguments["filter_user"].asString();
							}
							if (spCallToolRequest->jArguments.isMember("min_cpu") &&
								spCallToolRequest->jArguments["min_cpu"].isNumeric()) {
								min_cpu = spCallToolRequest->jArguments["min_cpu"].asDouble();
							}
							if (spCallToolRequest->jArguments.isMember("min_memory") &&
								spCallToolRequest->jArguments["min_memory"].isNumeric()) {
								min_memory = spCallToolRequest->jArguments["min_memory"].asUInt64();
							}
							if (spCallToolRequest->jArguments.isMember("sort_by") &&
								spCallToolRequest->jArguments["sort_by"].isString()) {
								sort_by = spCallToolRequest->jArguments["sort_by"].asString();
							}
							if (spCallToolRequest->jArguments.isMember("limit") &&
								spCallToolRequest->jArguments["limit"].isIntegral()) {
								limit = spCallToolRequest->jArguments["limit"].asInt();
							}
						}

						const auto& proc_data = Proc::collect(false);
						int total_processes = Proc::get_numpids();
						std::time_t timestamp = std::time(nullptr);

						// Filter processes
						vector<Proc::proc_info> filtered_processes;
						for (const auto& proc : proc_data) {
							bool include = true;

							// Apply filters
							if (!filter_name.empty()) {
								if (proc.name.find(filter_name) == string::npos) {
									include = false;
								}
							}
							if (!filter_user.empty() && include) {
								if (proc.user != filter_user) {
									include = false;
								}
							}
							if (min_cpu > 0.0 && include) {
								if (proc.cpu_p < min_cpu) {
									include = false;
								}
							}
							if (min_memory > 0 && include) {
								if (proc.mem < min_memory) {
									include = false;
								}
							}

							if (include) {
								filtered_processes.push_back(proc);
							}
						}

						// Sort processes
						if (sort_by == "cpu") {
							std::sort(filtered_processes.begin(), filtered_processes.end(),
								[](const Proc::proc_info& a, const Proc::proc_info& b) {
									return a.cpu_p > b.cpu_p;
								});
						} else if (sort_by == "memory") {
							std::sort(filtered_processes.begin(), filtered_processes.end(),
								[](const Proc::proc_info& a, const Proc::proc_info& b) {
									return a.mem > b.mem;
								});
						} else if (sort_by == "name") {
							std::sort(filtered_processes.begin(), filtered_processes.end(),
								[](const Proc::proc_info& a, const Proc::proc_info& b) {
									return a.name < b.name;
								});
						} else if (sort_by == "pid") {
							std::sort(filtered_processes.begin(), filtered_processes.end(),
								[](const Proc::proc_info& a, const Proc::proc_info& b) {
									return a.pid < b.pid;
								});
						} else if (sort_by == "user") {
							std::sort(filtered_processes.begin(), filtered_processes.end(),
								[](const Proc::proc_info& a, const Proc::proc_info& b) {
									return a.user < b.user;
								});
						}

						// Apply limit
						size_t processes_to_show = filtered_processes.size();
						if (limit > 0 && limit < (int)processes_to_show) {
							processes_to_show = limit;
						}

						result << "Process Information\\n";
						result << "Total Processes: " << total_processes << "\\n";
						result << "Filtered Processes: " << filtered_processes.size() << "\\n";
						result << "Showing: " << processes_to_show << "\\n";
						result << "Timestamp: " << timestamp << "\\n";
						if (!filter_name.empty()) result << "Name Filter: '" << filter_name << "'\\n";
						if (!filter_user.empty()) result << "User Filter: '" << filter_user << "'\\n";
						if (min_cpu > 0.0) result << "Min CPU: " << min_cpu << "%\\n";
						if (min_memory > 0) result << "Min Memory: " << min_memory << " bytes\\n";
						result << "Sort By: " << sort_by << "\\n\\n";

						if (processes_to_show == 0 && (!filter_name.empty() || !filter_user.empty() || min_cpu > 0.0 || min_memory > 0)) {
							result << "No processes found matching the specified filters.\\n";
						} else {
							for (size_t i = 0; i < processes_to_show; ++i) {
								const auto& proc = filtered_processes[i];
								result << "PID: " << proc.pid << "\\n";
								result << "PPID: " << proc.ppid << "\\n";
								result << "Name: " << proc.name << "\\n";
								result << "User: " << proc.user << "\\n";
								result << "CPU: " << proc.cpu_p << "%\\n";
								result << "Memory: " << proc.mem << " bytes\\n";
								result << "State: " << proc.state << "\\n";
								result << "Threads: " << proc.threads << "\\n";
								if (!proc.cmd.empty()) {
									result << "Command: " << proc.cmd << "\\n";
								}
								result << "\\n";
							}
						}

					} catch (const std::exception& e) {
						result << "Error: " << e.what();
					}
				}
				textContent.strText = result.str();
			} else {
				spExecuteResult->bIsError = true;
				textContent.strText = "Unfortunately, the execution failed. Error code: " + std::to_string(iErrCode);
			}
			spExecuteResult->vecTextContent.push_back(textContent);
			iErrCode = NotifyResult(spExecuteResult);
		}

		return iErrCode;
	}
};

class CpuToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_cpu_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get CPU utilization and information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"EOF(
{
	"type": "object",
	"properties": {
		"cpu": {
			"type": "integer",
			"description": "Specific CPU core to report (-1 for all cores)",
			"default": -1
		}
	},
	"required": []
}
)EOF";

	CpuToolTask(const std::shared_ptr<TinyMCP::Request>& spRequest)
		: ProcessCallToolRequest(spRequest) {}

	std::shared_ptr<TinyMCP::CMCPTask> Clone() const override {
		auto spClone = std::make_shared<CpuToolTask>(nullptr);
		if (spClone) {
			*spClone = *this;
		}
		return spClone;
	}

	int Cancel() override {
		return TinyMCP::ERRNO_OK;
	}

	int Execute() override {
		int iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		if (!IsValid())
			return iErrCode;

		auto spCallToolRequest = std::dynamic_pointer_cast<TinyMCP::CallToolRequest>(m_spRequest);
		if (!spCallToolRequest || spCallToolRequest->strName.compare(TOOL_NAME) != 0)
			goto PROC_END;

		try {
			if (!is_plugin_loaded()) {
				iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
				goto PROC_END;
			}

			trigger_plugin_refresh();
			iErrCode = TinyMCP::ERRNO_OK;

		} catch (const std::exception& e) {
			iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		}

	PROC_END:
		auto spExecuteResult = BuildResult();
		if (spExecuteResult) {
			TinyMCP::TextContent textContent;
			textContent.strType = TinyMCP::CONST_TEXT;
			if (TinyMCP::ERRNO_OK == iErrCode) {
				spExecuteResult->bIsError = false;
				std::ostringstream result;
				if (!is_plugin_loaded()) {
					result << "Plugin not loaded";
				} else {
					try {
						// Parse input arguments
						int cpu_core = -1;
						if (spCallToolRequest->jArguments.isObject()) {
							if (spCallToolRequest->jArguments.isMember("cpu") &&
								spCallToolRequest->jArguments["cpu"].isIntegral()) {
								cpu_core = spCallToolRequest->jArguments["cpu"].asInt();
							}
						}

						const auto& cpu_data = Cpu::collect(false);
						string cpu_name = Cpu::get_cpuName();
						string cpu_freq = Cpu::get_cpuHz();
						bool has_sensors = Cpu::get_got_sensors();
						std::time_t timestamp = std::time(nullptr);

						double total_cpu_usage = 0.0;
						if (!cpu_data.cpu_percent.at("total").empty()) {
							total_cpu_usage = cpu_data.cpu_percent.at("total").back();
						}

						result << "CPU Information\\n";
						result << "Name: " << cpu_name << "\\n";
						result << "Frequency: " << cpu_freq << "\\n";
						result << "Total Usage: " << total_cpu_usage << "%\\n";
						result << "Load Average: " << cpu_data.load_avg[0] << ", " << cpu_data.load_avg[1] << ", " << cpu_data.load_avg[2] << "\\n";
						result << "Core Count: " << cpu_data.core_percent.size() << "\\n";

						if (has_sensors && !cpu_data.temp.empty()) {
							result << "Temperature Max: " << cpu_data.temp_max << "°C\\n";
						}

						result << "Timestamp: " << timestamp << "\\n\\n";

						// Report specific core or all cores
						if (cpu_core >= 0) {
							if (cpu_core < (int)cpu_data.core_percent.size()) {
								// Report specific core
								result << "Core " << cpu_core << ": ";
								if (!cpu_data.core_percent[cpu_core].empty()) {
									result << cpu_data.core_percent[cpu_core].back() << "%";
								} else {
									result << "N/A";
								}

								if (has_sensors && cpu_core < (int)cpu_data.temp.size() && !cpu_data.temp[cpu_core].empty()) {
									result << " (Temp: " << cpu_data.temp[cpu_core].back() << "°C)";
								}
								result << "\\n";
							} else {
								// Invalid CPU core specified
								result << "Invalid CPU core " << cpu_core << " specified. Available cores: 0-" << (cpu_data.core_percent.size() - 1) << "\\n";
							}
						} else {
							// Report all cores
							for (size_t i = 0; i < cpu_data.core_percent.size(); ++i) {
								result << "Core " << i << ": ";
								if (!cpu_data.core_percent[i].empty()) {
									result << cpu_data.core_percent[i].back() << "%";
								} else {
									result << "N/A";
								}

								if (has_sensors && i < cpu_data.temp.size() && !cpu_data.temp[i].empty()) {
									result << " (Temp: " << cpu_data.temp[i].back() << "°C)";
								}
								result << "\\n";
							}
						}

						// Add CPU state breakdown
						result << "\\nCPU State Breakdown:\\n";
						const vector<string> cpu_states = {"user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal", "guest", "guest_nice"};
						for (const auto& state : cpu_states) {
							if (cpu_data.cpu_percent.count(state) && !cpu_data.cpu_percent.at(state).empty()) {
								result << state << ": " << cpu_data.cpu_percent.at(state).back() << "%\\n";
							}
						}

					} catch (const std::exception& e) {
						result << "Error: " << e.what();
					}
				}
				textContent.strText = result.str();
			} else {
				spExecuteResult->bIsError = true;
				textContent.strText = "Unfortunately, the execution failed. Error code: " + std::to_string(iErrCode);
			}
			spExecuteResult->vecTextContent.push_back(textContent);
			iErrCode = NotifyResult(spExecuteResult);
		}

		return iErrCode;
	}
};

class MemoryToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_memory_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get memory usage information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"({"type":"object","properties":{},"required":[]})";

	MemoryToolTask(const std::shared_ptr<TinyMCP::Request>& spRequest)
		: ProcessCallToolRequest(spRequest) {}

	std::shared_ptr<TinyMCP::CMCPTask> Clone() const override {
		auto spClone = std::make_shared<MemoryToolTask>(nullptr);
		if (spClone) {
			*spClone = *this;
		}
		return spClone;
	}

	int Cancel() override {
		return TinyMCP::ERRNO_OK;
	}

	int Execute() override {
		int iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		if (!IsValid())
			return iErrCode;

		auto spCallToolRequest = std::dynamic_pointer_cast<TinyMCP::CallToolRequest>(m_spRequest);
		if (!spCallToolRequest || spCallToolRequest->strName.compare(TOOL_NAME) != 0)
			goto PROC_END;

		try {
			if (!is_plugin_loaded()) {
				iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
				goto PROC_END;
			}

			trigger_plugin_refresh();
			iErrCode = TinyMCP::ERRNO_OK;

		} catch (const std::exception& e) {
			iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		}

	PROC_END:
		auto spExecuteResult = BuildResult();
		if (spExecuteResult) {
			TinyMCP::TextContent textContent;
			textContent.strType = TinyMCP::CONST_TEXT;
			if (TinyMCP::ERRNO_OK == iErrCode) {
				spExecuteResult->bIsError = false;
				std::ostringstream result;
				if (!is_plugin_loaded()) {
					result << "Plugin not loaded";
				} else {
					try {
						auto& mem_data = Mem::collect(false);
						uint64_t total_mem = Mem::get_totalMem();
						bool has_swap = Mem::get_has_swap();
						std::time_t timestamp = std::time(nullptr);

						// Calculate memory usage percentages
						double used_percent = 0.0, available_percent = 0.0, cached_percent = 0.0;
						if (total_mem > 0) {
							used_percent = (double)mem_data.stats["used"] * 100.0 / total_mem;
							available_percent = (double)mem_data.stats["available"] * 100.0 / total_mem;
							cached_percent = (double)mem_data.stats["cached"] * 100.0 / total_mem;
						}

						result << "Memory Information\\n";
						result << "Total: " << total_mem << " bytes\\n";
						result << "Used: " << mem_data.stats["used"] << " bytes (" << used_percent << "%)\\n";
						result << "Available: " << mem_data.stats["available"] << " bytes (" << available_percent << "%)\\n";
						result << "Cached: " << mem_data.stats["cached"] << " bytes (" << cached_percent << "%)\\n";
						result << "Free: " << mem_data.stats["free"] << " bytes\\n";
						if (has_swap) {
							result << "Swap Total: " << mem_data.stats["swap_total"] << " bytes\\n";
							result << "Swap Used: " << mem_data.stats["swap_used"] << " bytes\\n";
							result << "Swap Free: " << mem_data.stats["swap_free"] << " bytes\\n";
						}
						result << "Timestamp: " << timestamp << "\\n";
					} catch (const std::exception& e) {
						result << "Error: " << e.what();
					}
				}
				textContent.strText = result.str();
			} else {
				spExecuteResult->bIsError = true;
				textContent.strText = "Unfortunately, the execution failed. Error code: " + std::to_string(iErrCode);
			}
			spExecuteResult->vecTextContent.push_back(textContent);
			iErrCode = NotifyResult(spExecuteResult);
		}

		return iErrCode;
	}
};

class NetworkToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_network_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get network interface information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"EOF(
{
	"type": "object",
	"properties": {
		"interface": {
			"type": "string",
			"description": "Specific network interface to report (empty for all interfaces)"
		}
	},
	"required": []
}
)EOF";

	NetworkToolTask(const std::shared_ptr<TinyMCP::Request>& spRequest)
		: ProcessCallToolRequest(spRequest) {}

	std::shared_ptr<TinyMCP::CMCPTask> Clone() const override {
		auto spClone = std::make_shared<NetworkToolTask>(nullptr);
		if (spClone) {
			*spClone = *this;
		}
		return spClone;
	}

	int Cancel() override {
		return TinyMCP::ERRNO_OK;
	}

	int Execute() override {
		int iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		if (!IsValid())
			return iErrCode;

		auto spCallToolRequest = std::dynamic_pointer_cast<TinyMCP::CallToolRequest>(m_spRequest);
		if (!spCallToolRequest || spCallToolRequest->strName.compare(TOOL_NAME) != 0)
			goto PROC_END;

		try {
			if (!is_plugin_loaded()) {
				iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
				goto PROC_END;
			}

			trigger_plugin_refresh();
			iErrCode = TinyMCP::ERRNO_OK;

		} catch (const std::exception& e) {
			iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		}

	PROC_END:
		auto spExecuteResult = BuildResult();
		if (spExecuteResult) {
			TinyMCP::TextContent textContent;
			textContent.strType = TinyMCP::CONST_TEXT;
			if (TinyMCP::ERRNO_OK == iErrCode) {
				spExecuteResult->bIsError = false;
				std::ostringstream result;
				if (!is_plugin_loaded()) {
					result << "Plugin not loaded";
				} else {
					try {
						// Parse input arguments
						string interface_filter;
						if (spCallToolRequest->jArguments.isObject()) {
							if (spCallToolRequest->jArguments.isMember("interface") &&
								spCallToolRequest->jArguments["interface"].isString()) {
								interface_filter = spCallToolRequest->jArguments["interface"].asString();
							}
						}

						const auto &interfaces = Net::get_interfaces();
						std::time_t timestamp = std::time(nullptr);

						result << "Network Information\\n";
						result << "Timestamp: " << timestamp << "\\n";
						if (!interface_filter.empty()) {
							result << "Filter: '" << interface_filter << "'\\n";
						}
						result << "\\n";

						bool found_interface = false;

						// Iterate through all interfaces
						for (const auto& iface : interfaces) {
							// Apply filter if specified
							if (!interface_filter.empty() && iface != interface_filter) {
								continue;
							}

							found_interface = true;

							// Set interface and collect data
							Net::set_selected_iface(iface);
							auto& net_data = Net::collect(false);

							result << "Interface: " << iface << "\\n";
							result << "Connected: " << (net_data.connected ? "Yes" : "No") << "\\n";

							// IP addresses
							if (!net_data.ipv4.empty()) {
								result << "IPv4: " << net_data.ipv4 << "\\n";
							}
							if (!net_data.ipv6.empty()) {
								result << "IPv6: " << net_data.ipv6 << "\\n";
							}

							if (!net_data.connected) {
								result << "\\n";
								continue;
							}

							// Current speeds
							uint64_t download_speed = 0, upload_speed = 0;
							if (!net_data.bandwidth["download"].empty()) {
								download_speed = net_data.bandwidth["download"].back();
							}
							if (!net_data.bandwidth["upload"].empty()) {
								upload_speed = net_data.bandwidth["upload"].back();
							}

							result << "Download Speed: " << download_speed << " bytes/s\\n";
							result << "Upload Speed: " << upload_speed << " bytes/s\\n";
							result << "\\n";
						}

						// Check if interface filter didn't match anything
						if (!interface_filter.empty() && !found_interface) {
							result << "No interface found matching '" << interface_filter << "'\\n";
							result << "Available interfaces: ";
							for (const auto& iface : interfaces) {
								result << iface << " ";
							}
							result << "\\n";
						}

					} catch (const std::exception& e) {
						result << "Error: " << e.what();
					}
				}
				textContent.strText = result.str();
			} else {
				spExecuteResult->bIsError = true;
				textContent.strText = "Unfortunately, the execution failed. Error code: " + std::to_string(iErrCode);
			}
			spExecuteResult->vecTextContent.push_back(textContent);
			iErrCode = NotifyResult(spExecuteResult);
		}

		return iErrCode;
	}
};

class DiskToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_disk_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get disk usage and I/O information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"EOF(
{
	"type": "object",
	"properties": {
		"disk": {
			"type": "string",
			"description": "Specific disk or mount point to report (empty for all disks)"
		}
	},
	"required": []
}
)EOF";

	DiskToolTask(const std::shared_ptr<TinyMCP::Request>& spRequest)
		: ProcessCallToolRequest(spRequest) {}

	std::shared_ptr<TinyMCP::CMCPTask> Clone() const override {
		auto spClone = std::make_shared<DiskToolTask>(nullptr);
		if (spClone) {
			*spClone = *this;
		}
		return spClone;
	}

	int Cancel() override {
		return TinyMCP::ERRNO_OK;
	}

	int Execute() override {
		int iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		if (!IsValid())
			return iErrCode;

		auto spCallToolRequest = std::dynamic_pointer_cast<TinyMCP::CallToolRequest>(m_spRequest);
		if (!spCallToolRequest || spCallToolRequest->strName.compare(TOOL_NAME) != 0)
			goto PROC_END;

		try {
			if (!is_plugin_loaded()) {
				iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
				goto PROC_END;
			}

			trigger_plugin_refresh();
			iErrCode = TinyMCP::ERRNO_OK;

		} catch (const std::exception& e) {
			iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		}

	PROC_END:
		auto spExecuteResult = BuildResult();
		if (spExecuteResult) {
			TinyMCP::TextContent textContent;
			textContent.strType = TinyMCP::CONST_TEXT;
			if (TinyMCP::ERRNO_OK == iErrCode) {
				spExecuteResult->bIsError = false;
				std::ostringstream result;
				if (!is_plugin_loaded()) {
					result << "Plugin not loaded";
				} else {
					try {
						// Parse input arguments
						string disk_filter;
						if (spCallToolRequest->jArguments.isObject()) {
							if (spCallToolRequest->jArguments.isMember("disk") &&
								spCallToolRequest->jArguments["disk"].isString()) {
								disk_filter = spCallToolRequest->jArguments["disk"].asString();
							}
						}

						auto mem_data = Mem::collect(false);
						std::time_t timestamp = std::time(nullptr);

						result << "Disk Information\\n";
						result << "Timestamp: " << timestamp << "\\n";
						if (!disk_filter.empty()) {
							result << "Filter: '" << disk_filter << "'\\n";
						}
						result << "\\n";

						bool found_disk = false;
						for (const auto& disk_name : mem_data.disks_order) {
							const auto& disk = mem_data.disks.at(disk_name);

							// Apply filter if specified
							if (!disk_filter.empty()) {
								if (disk.name.find(disk_filter) == string::npos &&
									disk_name.find(disk_filter) == string::npos) {
									continue;
								}
							}

							found_disk = true;

							result << "Disk: " << disk.name << "\\n";
							result << "Mount Point: " << disk_name << "\\n";
							if (!disk.dev.empty()) {
								result << "Device: " << disk.dev << "\\n";
							}
							result << "Filesystem: " << disk.fstype << "\\n";
							result << "Total: " << disk.total << " bytes\\n";
							result << "Used: " << disk.used << " bytes (" << disk.used_percent << "%)\\n";
							result << "Free: " << disk.free << " bytes (" << disk.free_percent << "%)\\n";

							// I/O Statistics
							if (!disk.io_read.empty()) {
								result << "Read Speed: " << disk.io_read.back() << " bytes/s\\n";
							}
							if (!disk.io_write.empty()) {
								result << "Write Speed: " << disk.io_write.back() << " bytes/s\\n";
							}
							if (!disk.io_activity.empty()) {
								result << "I/O Activity: " << disk.io_activity.back() << "%\\n";
							}

							result << "\\n";
						}

						if (!disk_filter.empty() && !found_disk) {
							result << "No disks found matching filter '" << disk_filter << "'\\n";
						}

					} catch (const std::exception& e) {
						result << "Error: " << e.what();
					}
				}
				textContent.strText = result.str();
			} else {
				spExecuteResult->bIsError = true;
				textContent.strText = "Unfortunately, the execution failed. Error code: " + std::to_string(iErrCode);
			}
			spExecuteResult->vecTextContent.push_back(textContent);
			iErrCode = NotifyResult(spExecuteResult);
		}

		return iErrCode;
	}
};

class GpuToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_gpu_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get GPU utilization and information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"EOF(
{
	"type": "object",
	"properties": {
		"gpu": {
			"type": "integer",
			"description": "Specific GPU device to report (-1 for all GPUs)",
			"default": -1
		}
	},
	"required": []
}
)EOF";

	GpuToolTask(const std::shared_ptr<TinyMCP::Request>& spRequest)
		: ProcessCallToolRequest(spRequest) {}

	std::shared_ptr<TinyMCP::CMCPTask> Clone() const override {
		auto spClone = std::make_shared<GpuToolTask>(nullptr);
		if (spClone) {
			*spClone = *this;
		}
		return spClone;
	}

	int Cancel() override {
		return TinyMCP::ERRNO_OK;
	}

	int Execute() override {
		int iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		if (!IsValid())
			return iErrCode;

		auto spCallToolRequest = std::dynamic_pointer_cast<TinyMCP::CallToolRequest>(m_spRequest);
		if (!spCallToolRequest || spCallToolRequest->strName.compare(TOOL_NAME) != 0)
			goto PROC_END;

		try {
			if (!is_plugin_loaded()) {
				iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
				goto PROC_END;
			}

			trigger_plugin_refresh();
			iErrCode = TinyMCP::ERRNO_OK;

		} catch (const std::exception& e) {
			iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		}

	PROC_END:
		auto spExecuteResult = BuildResult();
		if (spExecuteResult) {
			TinyMCP::TextContent textContent;
			textContent.strType = TinyMCP::CONST_TEXT;
			if (TinyMCP::ERRNO_OK == iErrCode) {
				spExecuteResult->bIsError = false;
				std::ostringstream result;
				if (!is_plugin_loaded()) {
					result << "Plugin not loaded";
				} else {
					// Parse input arguments
					int gpu_device = -1;
					if (spCallToolRequest->jArguments.isObject()) {
						if (spCallToolRequest->jArguments.isMember("gpu") &&
							spCallToolRequest->jArguments["gpu"].isIntegral()) {
							gpu_device = spCallToolRequest->jArguments["gpu"].asInt();
						}
					}

					if (Gpu::get_count() == 0) {
						result << "No GPU devices found";
					} else {
						try {
							auto& gpu_data = Gpu::collect(false);
							int gpu_count = Gpu::get_count();
							const auto& gpu_names = Gpu::get_gpu_names();
							std::time_t timestamp = std::time(nullptr);

							result << "GPU Information\\n";
							result << "GPU Count: " << gpu_count << "\\n";
							result << "Timestamp: " << timestamp << "\\n\\n";

							// Report specific GPU or all GPUs
							if (gpu_device >= 0) {
								if (gpu_device < gpu_count && gpu_device < (int)gpu_data.size()) {
									// Report specific GPU
									result << "GPU " << gpu_device << ":\\n";
									result << "Name: " << (gpu_device < (int)gpu_names.size() ? gpu_names[gpu_device] : "Unknown GPU") << "\\n";

									// Get current GPU utilization
									double usage = 0.0;
									if (!gpu_data[gpu_device].gpu_percent["gpu-totals"].empty()) {
										usage = gpu_data[gpu_device].gpu_percent["gpu-totals"].back();
									}
									result << "Usage: " << usage << "%\\n";

									// Get current temperature
									double temp = 0.0;
									if (!gpu_data[gpu_device].temp.empty()) {
										temp = gpu_data[gpu_device].temp.back();
									}
									result << "Temperature: " << temp << "°C (Max: " << gpu_data[gpu_device].temp_max << "°C)\\n";
									result << "Memory Total: " << gpu_data[gpu_device].mem_total << " bytes\\n";
									result << "Memory Used: " << gpu_data[gpu_device].mem_used << " bytes\\n";
									result << "Power Usage: " << gpu_data[gpu_device].pwr_usage << "W (Max: " << gpu_data[gpu_device].pwr_max_usage << "W)\\n";
									result << "Clock Speed: " << gpu_data[gpu_device].gpu_clock_speed << " MHz\\n";
								} else {
									// Invalid GPU device specified
									result << "Invalid GPU device " << gpu_device << " specified. Available GPUs: 0-" << (gpu_count - 1) << "\\n";
								}
							} else {
								// Show all GPUs
								for (int i = 0; i < gpu_count && i < (int)gpu_data.size(); ++i) {
									result << "GPU " << i << ":\\n";
									result << "Name: " << (i < (int)gpu_names.size() ? gpu_names[i] : "Unknown GPU") << "\\n";

									// Get current GPU utilization
									double usage = 0.0;
									if (!gpu_data[i].gpu_percent["gpu-totals"].empty()) {
										usage = gpu_data[i].gpu_percent["gpu-totals"].back();
									}
									result << "Usage: " << usage << "%\\n";

									// Get current temperature
									double temp = 0.0;
									if (!gpu_data[i].temp.empty()) {
										temp = gpu_data[i].temp.back();
									}
									result << "Temperature: " << temp << "°C (Max: " << gpu_data[i].temp_max << "°C)\\n";
									result << "Memory Total: " << gpu_data[i].mem_total << " bytes\\n";
									result << "Memory Used: " << gpu_data[i].mem_used << " bytes\\n";
									result << "Power Usage: " << gpu_data[i].pwr_usage << "W (Max: " << gpu_data[i].pwr_max_usage << "W)\\n";
									result << "Clock Speed: " << gpu_data[i].gpu_clock_speed << " MHz\\n\\n";
								}
							}
						} catch (const std::exception& e) {
							result << "Error: " << e.what();
						}
					}
				}
				textContent.strText = result.str();
			} else {
				spExecuteResult->bIsError = true;
				textContent.strText = "Unfortunately, the execution failed. Error code: " + std::to_string(iErrCode);
			}
			spExecuteResult->vecTextContent.push_back(textContent);
			iErrCode = NotifyResult(spExecuteResult);
		}

		return iErrCode;
	}
};

class NpuToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_npu_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get NPU utilization and information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"EOF(
{
	"type": "object",
	"properties": {
		"npu": {
			"type": "integer",
			"description": "Specific NPU device to report (-1 for all NPUs)",
			"default": -1
		}
	},
	"required": []
}
)EOF";

	NpuToolTask(const std::shared_ptr<TinyMCP::Request>& spRequest)
		: ProcessCallToolRequest(spRequest) {}

	std::shared_ptr<TinyMCP::CMCPTask> Clone() const override {
		auto spClone = std::make_shared<NpuToolTask>(nullptr);
		if (spClone) {
			*spClone = *this;
		}
		return spClone;
	}

	int Cancel() override {
		return TinyMCP::ERRNO_OK;
	}

	int Execute() override {
		int iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		if (!IsValid())
			return iErrCode;

		auto spCallToolRequest = std::dynamic_pointer_cast<TinyMCP::CallToolRequest>(m_spRequest);
		if (!spCallToolRequest || spCallToolRequest->strName.compare(TOOL_NAME) != 0)
			goto PROC_END;

		try {
			if (!is_plugin_loaded()) {
				iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
				goto PROC_END;
			}

			trigger_plugin_refresh();
			iErrCode = TinyMCP::ERRNO_OK;

		} catch (const std::exception& e) {
			iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		}

	PROC_END:
		auto spExecuteResult = BuildResult();
		if (spExecuteResult) {
			TinyMCP::TextContent textContent;
			textContent.strType = TinyMCP::CONST_TEXT;
			if (TinyMCP::ERRNO_OK == iErrCode) {
				spExecuteResult->bIsError = false;
				std::ostringstream result;
				if (!is_plugin_loaded()) {
					result << "Plugin not loaded";
				} else {
					// Parse input arguments
					int npu_device = -1;
					if (spCallToolRequest->jArguments.isObject()) {
						if (spCallToolRequest->jArguments.isMember("npu") &&
							spCallToolRequest->jArguments["npu"].isIntegral()) {
							npu_device = spCallToolRequest->jArguments["npu"].asInt();
						}
					}

					if (Npu::get_count() == 0) {
						result << "No NPU devices found";
					} else {
						try {
							auto npu_data = Npu::collect(false);
							int npu_count = Npu::get_count();
							auto npu_names = Npu::get_npu_names();
							std::time_t timestamp = std::time(nullptr);

							result << "NPU Information\\n";
							result << "NPU Count: " << npu_count << "\\n";
							result << "Timestamp: " << timestamp << "\\n\\n";

							// Report specific NPU or all NPUs
							if (npu_device >= 0) {
								if (npu_device < npu_count && npu_device < (int)npu_data.size()) {
									// Report specific NPU
									result << "NPU " << npu_device << ":\\n";
									result << "Name: " << (npu_device < (int)npu_names.size() ? npu_names[npu_device] : "Unknown NPU") << "\\n";

									// Get current NPU utilization
									double usage = 0.0;
									if (!npu_data[npu_device].npu_percent["npu-totals"].empty()) {
										usage = npu_data[npu_device].npu_percent["npu-totals"].back();
									}
									result << "Usage: " << usage << "%\\n";
								} else {
									// Invalid NPU device specified
									result << "Invalid NPU device " << npu_device << " specified. Available NPUs: 0-" << (npu_count - 1) << "\\n";
								}
							} else {
								// Show all NPUs
								for (int i = 0; i < npu_count && i < (int)npu_data.size(); ++i) {
									result << "NPU " << i << ":\\n";
									result << "Name: " << (i < (int)npu_names.size() ? npu_names[i] : "Unknown NPU") << "\\n";

									// Get current NPU utilization
									double usage = 0.0;
									if (!npu_data[i].npu_percent["npu-totals"].empty()) {
										usage = npu_data[i].npu_percent["npu-totals"].back();
									}
									result << "Usage: " << usage << "%\\n\\n";
								}
							}
						} catch (const std::exception& e) {
							result << "Error: " << e.what();
						}
					}
				}
				textContent.strText = result.str();
			} else {
				spExecuteResult->bIsError = true;
				textContent.strText = "Unfortunately, the execution failed. Error code: " + std::to_string(iErrCode);
			}
			spExecuteResult->vecTextContent.push_back(textContent);
			iErrCode = NotifyResult(spExecuteResult);
		}

		return iErrCode;
	}
};

class SystemToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_system_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get general system information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"({"type":"object","properties":{},"required":[]})";

	SystemToolTask(const std::shared_ptr<TinyMCP::Request>& spRequest)
		: ProcessCallToolRequest(spRequest) {}

	std::shared_ptr<TinyMCP::CMCPTask> Clone() const override {
		auto spClone = std::make_shared<SystemToolTask>(nullptr);
		if (spClone) {
			*spClone = *this;
		}
		return spClone;
	}

	int Cancel() override {
		return TinyMCP::ERRNO_OK;
	}

	int Execute() override {
		int iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		if (!IsValid())
			return iErrCode;

		auto spCallToolRequest = std::dynamic_pointer_cast<TinyMCP::CallToolRequest>(m_spRequest);
		if (!spCallToolRequest || spCallToolRequest->strName.compare(TOOL_NAME) != 0)
			goto PROC_END;

		try {
			if (!is_plugin_loaded()) {
				iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
				goto PROC_END;
			}

			iErrCode = TinyMCP::ERRNO_OK;

		} catch (const std::exception& e) {
			iErrCode = TinyMCP::ERRNO_INTERNAL_ERROR;
		}

	PROC_END:
		auto spExecuteResult = BuildResult();
		if (spExecuteResult) {
			TinyMCP::TextContent textContent;
			textContent.strType = TinyMCP::CONST_TEXT;
			if (TinyMCP::ERRNO_OK == iErrCode) {
				spExecuteResult->bIsError = false;
				std::ostringstream result;
				if (!is_plugin_loaded()) {
					result << "Plugin not loaded";
				} else {
					try {
						std::time_t timestamp = std::time(nullptr);

						// Get additional system information
						const auto& cpu_data = Cpu::collect(false);
						string cpu_name = Cpu::get_cpuName();
						string hostname = Tools::hostname();
						uint64_t total_mem = Mem::get_totalMem();
						bool has_battery = Cpu::get_has_battery();
						int gpu_count = Gpu::get_count();
						int npu_count = Npu::get_count();

						result << "System Information\\n";
						result << "Name: cosmotop v" << Global::Version << "\\n";
						result << "Hostname: " << hostname << "\\n";
						result << "CPU Name: " << cpu_name << "\\n";
						result << "CPU Cores: " << cpu_data.core_percent.size() << "\\n";
						result << "Total Memory: " << total_mem << " bytes\\n";
						result << "Has Battery: " << (has_battery ? "Yes" : "No") << "\\n";
						result << "GPU Count: " << gpu_count << "\\n";
						result << "NPU Count: " << npu_count << "\\n";
						result << "Timestamp: " << timestamp << "\\n";
					} catch (const std::exception& e) {
						result << "Error: " << e.what();
					}
				}
				textContent.strText = result.str();
			} else {
				spExecuteResult->bIsError = true;
				textContent.strText = "Unfortunately, the execution failed. Error code: " + std::to_string(iErrCode);
			}
			spExecuteResult->vecTextContent.push_back(textContent);
			iErrCode = NotifyResult(spExecuteResult);
		}

		return iErrCode;
	}
};

// TinyMCP server implementation
class CosmotopTinyMCPServer : public TinyMCP::CMCPServer<CosmotopTinyMCPServer> {
public:
	static constexpr const char* SERVER_NAME = "cosmotop TinyMCP Server";

	int Initialize() override {
		// Set server info
		TinyMCP::Implementation serverInfo;
		serverInfo.strName = SERVER_NAME;
		serverInfo.strVersion = Global::Version;
		SetServerInfo(serverInfo);

		// Register server capabilities
		TinyMCP::Tools tools;
		RegisterServerToolsCapabilities(tools);

		// Register tools
		std::vector<TinyMCP::Tool> vecTools;

		// Process tool
		TinyMCP::Tool processTool;
		processTool.strName = ProcessToolTask::TOOL_NAME;
		processTool.strDescription = ProcessToolTask::TOOL_DESCRIPTION;
		Json::Reader reader;
		if (!reader.parse(ProcessToolTask::TOOL_INPUT_SCHEMA, processTool.jInputSchema) || !processTool.jInputSchema.isObject())
			return TinyMCP::ERRNO_PARSE_ERROR;
		vecTools.push_back(processTool);

		// CPU tool
		TinyMCP::Tool cpuTool;
		cpuTool.strName = CpuToolTask::TOOL_NAME;
		cpuTool.strDescription = CpuToolTask::TOOL_DESCRIPTION;
		if (!reader.parse(CpuToolTask::TOOL_INPUT_SCHEMA, cpuTool.jInputSchema) || !cpuTool.jInputSchema.isObject())
			return TinyMCP::ERRNO_PARSE_ERROR;
		vecTools.push_back(cpuTool);

		// Memory tool
		TinyMCP::Tool memoryTool;
		memoryTool.strName = MemoryToolTask::TOOL_NAME;
		memoryTool.strDescription = MemoryToolTask::TOOL_DESCRIPTION;
		if (!reader.parse(MemoryToolTask::TOOL_INPUT_SCHEMA, memoryTool.jInputSchema) || !memoryTool.jInputSchema.isObject())
			return TinyMCP::ERRNO_PARSE_ERROR;
		vecTools.push_back(memoryTool);

		// Network tool
		TinyMCP::Tool networkTool;
		networkTool.strName = NetworkToolTask::TOOL_NAME;
		networkTool.strDescription = NetworkToolTask::TOOL_DESCRIPTION;
		if (!reader.parse(NetworkToolTask::TOOL_INPUT_SCHEMA, networkTool.jInputSchema) || !networkTool.jInputSchema.isObject())
			return TinyMCP::ERRNO_PARSE_ERROR;
		vecTools.push_back(networkTool);

		// Disk tool
		TinyMCP::Tool diskTool;
		diskTool.strName = DiskToolTask::TOOL_NAME;
		diskTool.strDescription = DiskToolTask::TOOL_DESCRIPTION;
		if (!reader.parse(DiskToolTask::TOOL_INPUT_SCHEMA, diskTool.jInputSchema) || !diskTool.jInputSchema.isObject())
			return TinyMCP::ERRNO_PARSE_ERROR;
		vecTools.push_back(diskTool);

		// GPU tool (only if GPUs are available)
		if (Gpu::get_count() > 0) {
			TinyMCP::Tool gpuTool;
			gpuTool.strName = GpuToolTask::TOOL_NAME;
			gpuTool.strDescription = GpuToolTask::TOOL_DESCRIPTION;
			if (!reader.parse(GpuToolTask::TOOL_INPUT_SCHEMA, gpuTool.jInputSchema) || !gpuTool.jInputSchema.isObject())
				return TinyMCP::ERRNO_PARSE_ERROR;
			vecTools.push_back(gpuTool);
		}

		// NPU tool (only if NPUs are available)
		if (Npu::get_count() > 0) {
			TinyMCP::Tool npuTool;
			npuTool.strName = NpuToolTask::TOOL_NAME;
			npuTool.strDescription = NpuToolTask::TOOL_DESCRIPTION;
			if (!reader.parse(NpuToolTask::TOOL_INPUT_SCHEMA, npuTool.jInputSchema) || !npuTool.jInputSchema.isObject())
				return TinyMCP::ERRNO_PARSE_ERROR;
			vecTools.push_back(npuTool);
		}

		// System tool
		TinyMCP::Tool systemTool;
		systemTool.strName = SystemToolTask::TOOL_NAME;
		systemTool.strDescription = SystemToolTask::TOOL_DESCRIPTION;
		if (!reader.parse(SystemToolTask::TOOL_INPUT_SCHEMA, systemTool.jInputSchema) || !systemTool.jInputSchema.isObject())
			return TinyMCP::ERRNO_PARSE_ERROR;
		vecTools.push_back(systemTool);

		RegisterServerTools(vecTools, false);

		// Register tool tasks
		RegisterToolsTasks(ProcessToolTask::TOOL_NAME, std::make_shared<ProcessToolTask>(nullptr));
		RegisterToolsTasks(CpuToolTask::TOOL_NAME, std::make_shared<CpuToolTask>(nullptr));
		RegisterToolsTasks(MemoryToolTask::TOOL_NAME, std::make_shared<MemoryToolTask>(nullptr));
		RegisterToolsTasks(NetworkToolTask::TOOL_NAME, std::make_shared<NetworkToolTask>(nullptr));
		RegisterToolsTasks(DiskToolTask::TOOL_NAME, std::make_shared<DiskToolTask>(nullptr));
		if (Gpu::get_count() > 0) {
			RegisterToolsTasks(GpuToolTask::TOOL_NAME, std::make_shared<GpuToolTask>(nullptr));
		}
		if (Npu::get_count() > 0) {
			RegisterToolsTasks(NpuToolTask::TOOL_NAME, std::make_shared<NpuToolTask>(nullptr));
		}
		RegisterToolsTasks(SystemToolTask::TOOL_NAME, std::make_shared<SystemToolTask>(nullptr));

		return TinyMCP::ERRNO_OK;
	}

private:
	friend class TinyMCP::CMCPServer<CosmotopTinyMCPServer>;
	CosmotopTinyMCPServer() = default;
	static CosmotopTinyMCPServer s_Instance;
};

CosmotopTinyMCPServer CosmotopTinyMCPServer::s_Instance;

bool init_mcp_server() {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		return false;
	}

	auto& server = CosmotopTinyMCPServer::GetInstance();
	int iErrCode = server.Initialize();
	if (TinyMCP::ERRNO_OK == iErrCode) {
		std::thread serverThread([]() {
			CosmotopTinyMCPServer::GetInstance().Start();
		});
		serverThread.detach();
		return true;
	}

	return false;
}

void shutdown_mcp_server() {
	auto& server = CosmotopTinyMCPServer::GetInstance();
	server.Stop();
}

} // namespace Mcp