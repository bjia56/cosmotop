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

#include <Entity/Server.h>
#include <Task/BasicTask.h>
#include <Message/Request.h>
#include <Public/PublicDef.h>
#include <Public/StringHelper.h>

#include "cosmotop_mcp.hpp"
#include "cosmotop_config.hpp"
#include "cosmotop_shared.hpp"
#include "cosmotop_plugin.hpp"

namespace TinyMCP = MCP;

namespace Mcp {

// Task classes for each tool
class ProcessToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_processes";
	static constexpr const char* TOOL_DESCRIPTION = "Get list of running processes";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"({"type":"object","properties":{},"required":[]})";

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
						auto proc_data = Proc::collect(false);
						int numpids = Proc::get_numpids();
						std::time_t timestamp = std::time(nullptr);

						result << "Process Information (Total: " << numpids << ")\\n";
						result << "Timestamp: " << timestamp << "\\n\\n";

						for (size_t i = 0; i < proc_data.size() && i < 10; ++i) {
							result << "PID: " << proc_data[i].pid << "\\n";
							result << "Name: " << proc_data[i].name << "\\n";
							result << "User: " << proc_data[i].user << "\\n";
							result << "CPU: " << proc_data[i].cpu_p << "%\\n";
							result << "Memory: " << proc_data[i].mem << " bytes\\n";
							result << "State: " << proc_data[i].state << "\\n";
							result << "Threads: " << proc_data[i].threads << "\\n\\n";
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
	static constexpr const char* TOOL_INPUT_SCHEMA = R"({"type":"object","properties":{},"required":[]})";

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
						auto cpu_data = Cpu::collect(false);
						string cpu_name = Cpu::get_cpuName();
						string cpu_freq = Cpu::get_cpuHz();
						bool has_sensors = Cpu::get_got_sensors();
						std::time_t timestamp = std::time(nullptr);

						double cpu_usage = 0.0;
						if (!cpu_data.cpu_percent.at("total").empty()) {
							cpu_usage = cpu_data.cpu_percent.at("total").back();
						}

						double temp = 0.0;
						if (has_sensors && !cpu_data.temp.empty() && !cpu_data.temp[0].empty()) {
							temp = cpu_data.temp[0].back();
						}

						result << "CPU Information\\n";
						result << "Name: " << cpu_name << "\\n";
						result << "Frequency: " << cpu_freq << "\\n";
						result << "Usage: " << cpu_usage << "%\\n";
						result << "Temperature: " << temp << "°C (Max: " << cpu_data.temp_max << "°C)\\n";
						result << "Has Sensors: " << (has_sensors ? "Yes" : "No") << "\\n";
						result << "Load Average: " << cpu_data.load_avg[0] << ", " << cpu_data.load_avg[1] << ", " << cpu_data.load_avg[2] << "\\n";
						result << "Core Count: " << cpu_data.core_percent.size() << "\\n";
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
						auto mem_data = Mem::collect(false);
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
						result << "Has Swap: " << (has_swap ? "Yes" : "No") << "\\n";
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
	static constexpr const char* TOOL_INPUT_SCHEMA = R"({"type":"object","properties":{},"required":[]})";

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
						auto net_data = Net::collect(false);
						string selected_iface = Net::get_selected_iface();
						auto interfaces = Net::get_interfaces();
						auto current_net = Net::get_current_net();
						std::time_t timestamp = std::time(nullptr);

						// Get current network speeds
						uint64_t download_speed = 0, upload_speed = 0;
						if (!net_data.bandwidth["download"].empty()) {
							download_speed = net_data.bandwidth["download"].back();
						}
						if (!net_data.bandwidth["upload"].empty()) {
							upload_speed = net_data.bandwidth["upload"].back();
						}

						result << "Network Information\\n";
						result << "Selected Interface: " << selected_iface << "\\n";
						result << "Connected: " << (net_data.connected ? "Yes" : "No") << "\\n";
						result << "IPv4: " << net_data.ipv4 << "\\n";
						result << "IPv6: " << net_data.ipv6 << "\\n";
						result << "Download Speed: " << download_speed << " bytes/s\\n";
						result << "Upload Speed: " << upload_speed << " bytes/s\\n";
						result << "Download Total: " << net_data.stat["download"].total << " bytes\\n";
						result << "Upload Total: " << net_data.stat["upload"].total << " bytes\\n";
						result << "Available Interfaces: ";
						for (const auto& iface : interfaces) {
							result << iface << " ";
						}
						result << "\\n";
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

class GpuToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_gpu_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get GPU utilization and information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"({"type":"object","properties":{},"required":[]})";

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
					try {
						auto gpu_data = Gpu::collect(false);
						int gpu_count = Gpu::get_count();
						auto gpu_names = Gpu::get_gpu_names();
						std::time_t timestamp = std::time(nullptr);

						result << "GPU Information\\n";
						result << "GPU Count: " << gpu_count << "\\n";
						result << "Timestamp: " << timestamp << "\\n\\n";

						// Show GPU info
						for (int i = 0; i < gpu_count && i < (int)gpu_data.size(); ++i) {
							result << "GPU " << i << ":\\n";
							result << "  Name: " << (i < (int)gpu_names.size() ? gpu_names[i] : "Unknown GPU") << "\\n";

							// Get current GPU utilization
							double usage = 0.0;
							if (!gpu_data[i].gpu_percent["gpu-totals"].empty()) {
								usage = gpu_data[i].gpu_percent["gpu-totals"].back();
							}
							result << "  Usage: " << usage << "%\\n";

							// Get current temperature
							double temp = 0.0;
							if (!gpu_data[i].temp.empty()) {
								temp = gpu_data[i].temp.back();
							}
							result << "  Temperature: " << temp << "°C (Max: " << gpu_data[i].temp_max << "°C)\\n";
							result << "  Memory Total: " << gpu_data[i].mem_total << " bytes\\n";
							result << "  Memory Used: " << gpu_data[i].mem_used << " bytes\\n";
							result << "  Power Usage: " << gpu_data[i].pwr_usage << "W (Max: " << gpu_data[i].pwr_max_usage << "W)\\n";
							result << "  Clock Speed: " << gpu_data[i].gpu_clock_speed << " MHz\\n\\n";
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

class NpuToolTask : public TinyMCP::ProcessCallToolRequest {
public:
	static constexpr const char* TOOL_NAME = "get_npu_info";
	static constexpr const char* TOOL_DESCRIPTION = "Get NPU utilization and information";
	static constexpr const char* TOOL_INPUT_SCHEMA = R"({"type":"object","properties":{},"required":[]})";

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
					try {
						auto npu_data = Npu::collect(false);
						int npu_count = Npu::get_count();
						auto npu_names = Npu::get_npu_names();
						std::time_t timestamp = std::time(nullptr);

						result << "NPU Information\\n";
						result << "NPU Count: " << npu_count << "\\n";
						result << "Timestamp: " << timestamp << "\\n\\n";

						// Show NPU info
						for (int i = 0; i < npu_count && i < (int)npu_data.size(); ++i) {
							result << "NPU " << i << ":\\n";
							result << "  Name: " << (i < (int)npu_names.size() ? npu_names[i] : "Unknown NPU") << "\\n";

							// Get current NPU utilization
							double usage = 0.0;
							if (!npu_data[i].npu_percent["npu-totals"].empty()) {
								usage = npu_data[i].npu_percent["npu-totals"].back();
							}
							result << "  Usage: " << usage << "%\\n\\n";
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
						string cpu_name = Cpu::get_cpuName();
						uint64_t total_mem = Mem::get_totalMem();
						bool has_battery = Cpu::get_has_battery();
						int gpu_count = Gpu::get_count();
						int npu_count = Npu::get_count();

						result << "System Information\\n";
						result << "Name: cosmotop\\n";
						result << "Version: " << Global::Version << "\\n";
						result << "Plugin Loaded: " << (is_plugin_loaded() ? "Yes" : "No") << "\\n";
						result << "CPU Name: " << cpu_name << "\\n";
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

		// GPU tool
		TinyMCP::Tool gpuTool;
		gpuTool.strName = GpuToolTask::TOOL_NAME;
		gpuTool.strDescription = GpuToolTask::TOOL_DESCRIPTION;
		if (!reader.parse(GpuToolTask::TOOL_INPUT_SCHEMA, gpuTool.jInputSchema) || !gpuTool.jInputSchema.isObject())
			return TinyMCP::ERRNO_PARSE_ERROR;
		vecTools.push_back(gpuTool);

		// NPU tool
		TinyMCP::Tool npuTool;
		npuTool.strName = NpuToolTask::TOOL_NAME;
		npuTool.strDescription = NpuToolTask::TOOL_DESCRIPTION;
		if (!reader.parse(NpuToolTask::TOOL_INPUT_SCHEMA, npuTool.jInputSchema) || !npuTool.jInputSchema.isObject())
			return TinyMCP::ERRNO_PARSE_ERROR;
		vecTools.push_back(npuTool);

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
		RegisterToolsTasks(GpuToolTask::TOOL_NAME, std::make_shared<GpuToolTask>(nullptr));
		RegisterToolsTasks(NpuToolTask::TOOL_NAME, std::make_shared<NpuToolTask>(nullptr));
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