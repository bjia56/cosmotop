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

#include "cosmotop_mcp.hpp"
#include "cosmotop_config.hpp"
#include "cosmotop_shared.hpp"
#include "cosmotop_plugin.hpp"

#include "mcp_server.h"
#include <sstream>
#include <ctime>
#include <iostream>

// Include data structure definitions

namespace MCP {

std::unique_ptr<mcp::server> server_instance;

// Tool handler functions
mcp::json get_processes_handler(const mcp::json& params, const std::string /* session_id */) {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, "Plugin not loaded");
	}
	
	try {
		trigger_plugin_refresh();
		
		// Get process data via RPC
		auto proc_data = Proc::collect(false);
		int numpids = Proc::get_numpids();
		
		std::time_t timestamp = std::time(nullptr);
		
		std::ostringstream result;
		result << "Process Information (Total: " << numpids << ")\\n";
		result << "Timestamp: " << timestamp << "\\n\\n";
		
		// Show top 10 processes
		for (size_t i = 0; i < proc_data.size() && i < 10; ++i) {
			result << "PID: " << proc_data[i].pid << "\\n";
			result << "Name: " << proc_data[i].name << "\\n";
			result << "User: " << proc_data[i].user << "\\n";
			result << "CPU: " << proc_data[i].cpu_p << "%\\n";
			result << "Memory: " << proc_data[i].mem << " bytes\\n";
			result << "State: " << proc_data[i].state << "\\n";
			result << "Threads: " << proc_data[i].threads << "\\n\\n";
		}
		
		return {{{"type", "text"}, {"text", result.str()}}};
	} catch (const std::exception& e) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
	}
}

mcp::json get_cpu_info_handler(const mcp::json& params, const std::string /* session_id */) {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, "Plugin not loaded");
	}
	
	try {
		trigger_plugin_refresh();
		
		// Get CPU data via RPC
		auto cpu_data = Cpu::collect(false);
		string cpu_name = Cpu::get_cpuName();
		string cpu_freq = Cpu::get_cpuHz();
		bool has_sensors = Cpu::get_got_sensors();
		
		std::time_t timestamp = std::time(nullptr);
		
		// Get current CPU utilization (latest value from total percent)
		double cpu_usage = 0.0;
		if (!cpu_data.cpu_percent.at("total").empty()) {
			cpu_usage = cpu_data.cpu_percent.at("total").back();
		}
		
		// Get temperature if available
		double temp = 0.0;
		if (has_sensors && !cpu_data.temp.empty() && !cpu_data.temp[0].empty()) {
			temp = cpu_data.temp[0].back();
		}
		
		std::ostringstream result;
		result << "CPU Information\\n";
		result << "Name: " << cpu_name << "\\n";
		result << "Frequency: " << cpu_freq << "\\n";
		result << "Usage: " << cpu_usage << "%\\n";
		result << "Temperature: " << temp << "°C (Max: " << cpu_data.temp_max << "°C)\\n";
		result << "Has Sensors: " << (has_sensors ? "Yes" : "No") << "\\n";
		result << "Load Average: " << cpu_data.load_avg[0] << ", " << cpu_data.load_avg[1] << ", " << cpu_data.load_avg[2] << "\\n";
		result << "Core Count: " << cpu_data.core_percent.size() << "\\n";
		result << "Timestamp: " << timestamp << "\\n";
		
		return {{{"type", "text"}, {"text", result.str()}}};
	} catch (const std::exception& e) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
	}
}

mcp::json get_memory_info_handler(const mcp::json& params, const std::string /* session_id */) {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, "Plugin not loaded");
	}
	
	try {
		trigger_plugin_refresh();
		
		// Get memory data via RPC
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
		
		std::ostringstream result;
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
		
		return {{{"type", "text"}, {"text", result.str()}}};
	} catch (const std::exception& e) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
	}
}

mcp::json get_network_info_handler(const mcp::json& params, const std::string /* session_id */) {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, "Plugin not loaded");
	}
	
	try {
		trigger_plugin_refresh();
		
		// Get network data via RPC
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
		
		std::ostringstream result;
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
		
		return {{{"type", "text"}, {"text", result.str()}}};
	} catch (const std::exception& e) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
	}
}

mcp::json get_gpu_info_handler(const mcp::json& params, const std::string /* session_id */) {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, "Plugin not loaded");
	}
	
	try {
		trigger_plugin_refresh();
		
		// Get GPU data via RPC
		auto gpu_data = Gpu::collect(false);
		int gpu_count = Gpu::get_count();
		auto gpu_names = Gpu::get_gpu_names();
		
		std::time_t timestamp = std::time(nullptr);
		
		std::ostringstream result;
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
		
		return {{{"type", "text"}, {"text", result.str()}}};
	} catch (const std::exception& e) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
	}
}

mcp::json get_npu_info_handler(const mcp::json& params, const std::string /* session_id */) {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, "Plugin not loaded");
	}
	
	try {
		trigger_plugin_refresh();
		
		// Get NPU data via RPC
		auto npu_data = Npu::collect(false);
		int npu_count = Npu::get_count();
		auto npu_names = Npu::get_npu_names();
		
		std::time_t timestamp = std::time(nullptr);
		
		std::ostringstream result;
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
		
		return {{{"type", "text"}, {"text", result.str()}}};
	} catch (const std::exception& e) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
	}
}

mcp::json get_system_info_handler(const mcp::json& params, const std::string /* session_id */) {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, "Plugin not loaded");
	}
	
	try {
		std::time_t timestamp = std::time(nullptr);
		
		// Get additional system information
		string cpu_name = Cpu::get_cpuName();
		uint64_t total_mem = Mem::get_totalMem();
		bool has_battery = Cpu::get_has_battery();
		int gpu_count = Gpu::get_count();
		int npu_count = Npu::get_count();
		
		std::ostringstream result;
		result << "System Information\\n";
		result << "Name: cosmotop\\n";
		result << "Version: " << Global::Version << "\\n";
		result << "Plugin Loaded: " << (is_plugin_loaded() ? "Yes" : "No") << "\\n";
		result << "CPU Name: " << cpu_name << "\\n";
		result << "Total Memory: " << total_mem << " bytes\\n";
		result << "Has Battery: " << (has_battery ? "Yes" : "No") << "\\n";
		result << "GPU Count: " << gpu_count << "\\n";
		result << "NPU Count: " << npu_count << "\\n";
		result << "MCP Server Address: " << Config::getS("mcp_server_address") << "\\n";
		result << "MCP Server Port: " << Config::getI("mcp_server_port") << "\\n";
		result << "Timestamp: " << timestamp << "\\n";
		
		return {{{"type", "text"}, {"text", result.str()}}};
	} catch (const std::exception& e) {
		throw mcp::mcp_exception(mcp::error_code::internal_error, e.what());
	}
}

bool init_mcp_server() {
	if (server_instance) {
		return false; // Already initialized
	}

	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		std::cout << "Plugin not loaded, exiting" << std::endl;
		std::exit(1);
	}

	std::string address = Config::getS("mcp_server_address");
	int port = Config::getI("mcp_server_port");
	
	// Create and configure the server
	server_instance = std::make_unique<mcp::server>(address, port);
	server_instance->set_server_info("cosmotop MCP Server", Global::Version);

	// Register tools
	mcp::tool processes_tool = mcp::tool_builder("get_processes")
		.with_description("Get list of running processes")
		.build();
	server_instance->register_tool(processes_tool, get_processes_handler);

	mcp::tool cpu_tool = mcp::tool_builder("get_cpu_info")
		.with_description("Get CPU utilization and information")
		.build();
	server_instance->register_tool(cpu_tool, get_cpu_info_handler);

	mcp::tool memory_tool = mcp::tool_builder("get_memory_info")
		.with_description("Get memory usage information")
		.build();
	server_instance->register_tool(memory_tool, get_memory_info_handler);

	mcp::tool network_tool = mcp::tool_builder("get_network_info")
		.with_description("Get network interface information")
		.build();
	server_instance->register_tool(network_tool, get_network_info_handler);

	mcp::tool gpu_tool = mcp::tool_builder("get_gpu_info")
		.with_description("Get GPU utilization and information")
		.build();
	server_instance->register_tool(gpu_tool, get_gpu_info_handler);

	mcp::tool npu_tool = mcp::tool_builder("get_npu_info")
		.with_description("Get NPU utilization and information")
		.build();
	server_instance->register_tool(npu_tool, get_npu_info_handler);

	mcp::tool system_tool = mcp::tool_builder("get_system_info")
		.with_description("Get general system information")
		.build();
	server_instance->register_tool(system_tool, get_system_info_handler);

	std::cout << "Starting MCP server on " << address << ":" << port << std::endl;
	
	// Start the server in blocking mode
	server_instance->start(true);
	
	return true;
}

void shutdown_mcp_server() {
	if (server_instance) {
		server_instance.reset();
	}
}

} // namespace MCP