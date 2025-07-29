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

#include <httplib.h>
#include <fmt/core.h>
#include <sstream>
#include <ctime>

// Include data structure definitions
#ifndef __COSMOPOLITAN__
#include <cosmo_plugin.hpp>
#endif

namespace MCP {

std::unique_ptr<Server> server_instance;

Server::Server(const std::string& address, int port) 
	: address_(address), port_(port) {
}

Server::~Server() {
	stop();
}

bool Server::start() {
	if (running_.load()) {
		return false;
	}

	running_.store(true);
	server_thread_ = std::make_unique<std::thread>(&Server::run_server, this);
	return true;
}

void Server::stop() {
	if (running_.load()) {
		running_.store(false);
		if (server_thread_ && server_thread_->joinable()) {
			server_thread_->join();
		}
	}
}

bool Server::is_running() const {
	return running_.load();
}

void Server::run_server() {
	httplib::Server server;
	
	setup_tools();

	// MCP protocol endpoints
	server.Post("/tools/call", [this](const httplib::Request& req, httplib::Response& res) {
		try {
			// Simple JSON parsing - look for "name" field
			std::string body = req.body;
			size_t name_pos = body.find("\"name\":");
			if (name_pos == std::string::npos) {
				res.status = 400;
				res.body = "{\"error\": \"Missing tool name\"}";
				return;
			}
			
			// Extract tool name (simple approach)
			size_t quote_start = body.find("\"", name_pos + 7);
			size_t quote_end = body.find("\"", quote_start + 1);
			if (quote_start == std::string::npos || quote_end == std::string::npos) {
				res.status = 400;
				res.body = "{\"error\": \"Invalid tool name format\"}";
				return;
			}
			
			std::string tool_name = body.substr(quote_start + 1, quote_end - quote_start - 1);
			
			std::string result;
			if (tool_name == "get_processes") {
				result = get_processes();
			} else if (tool_name == "get_cpu_info") {
				result = get_cpu_info();
			} else if (tool_name == "get_memory_info") {
				result = get_memory_info();
			} else if (tool_name == "get_network_info") {
				result = get_network_info();
			} else if (tool_name == "get_gpu_info") {
				result = get_gpu_info();
			} else if (tool_name == "get_npu_info") {
				result = get_npu_info();
			} else if (tool_name == "get_system_info") {
				result = get_system_info();
			} else {
				res.status = 400;
				res.body = fmt::format("{{\"error\": \"Unknown tool: {}\"}}", tool_name);
				return;
			}

			res.body = fmt::format("{{\"content\": [{{\"type\": \"text\", \"text\": \"{}\"}}]}}", result);
			res.set_header("Content-Type", "application/json");
		} catch (const std::exception& e) {
			res.status = 500;
			res.body = fmt::format("{{\"error\": \"{}\"}}", e.what());
		}
	});

	// List available tools
	server.Get("/tools/list", [](const httplib::Request&, httplib::Response& res) {
		std::string tools_json = R"({
			"tools": [
				{
					"name": "get_processes",
					"description": "Get list of running processes",
					"inputSchema": {
						"type": "object",
						"properties": {}
					}
				},
				{
					"name": "get_cpu_info", 
					"description": "Get CPU utilization and information",
					"inputSchema": {
						"type": "object",
						"properties": {}
					}
				},
				{
					"name": "get_memory_info",
					"description": "Get memory usage information", 
					"inputSchema": {
						"type": "object",
						"properties": {}
					}
				},
				{
					"name": "get_network_info",
					"description": "Get network interface information",
					"inputSchema": {
						"type": "object", 
						"properties": {}
					}
				},
				{
					"name": "get_gpu_info",
					"description": "Get GPU utilization and information",
					"inputSchema": {
						"type": "object",
						"properties": {}
					}
				},
				{
					"name": "get_npu_info",
					"description": "Get NPU utilization and information",
					"inputSchema": {
						"type": "object",
						"properties": {}
					}
				},
				{
					"name": "get_system_info",
					"description": "Get general system information",
					"inputSchema": {
						"type": "object",
						"properties": {}
					}
				}
			]
		})";

		res.body = tools_json;
		res.set_header("Content-Type", "application/json");
	});

	Logger::info(fmt::format("Starting MCP server on {}:{}", address_, port_));
	
	if (!server.listen(address_, port_)) {
		Logger::error("Failed to start MCP server");
		running_.store(false);
	}
}

void Server::setup_tools() {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		Logger::error("Plugin not loaded, MCP tools may not work correctly");
	}
}

std::string Server::get_processes() {
	try {
		trigger_plugin_refresh();
		
		// Get process data via RPC
		#ifdef __COSMOPOLITAN__
		auto proc_data = Proc::collect(false);
		int numpids = Proc::get_numpids();
		#else
		auto proc_data = vector<proc_info>();
		int numpids = 0;
		#endif
		
		std::time_t timestamp = std::time(nullptr);
		std::stringstream processes_json;
		processes_json << "[";
		
		for (size_t i = 0; i < proc_data.size() && i < 10; ++i) { // Limit to top 10 processes
			if (i > 0) processes_json << ",";
			processes_json << fmt::format(R"({{
				"pid": {},
				"name": "{}",
				"user": "{}",
				"cpu_percent": {:.1f},
				"memory": {},
				"state": "{}",
				"threads": {}
			}})", proc_data[i].pid, proc_data[i].name, proc_data[i].user, 
			       proc_data[i].cpu_p, proc_data[i].mem, proc_data[i].state, proc_data[i].threads);
		}
		processes_json << "]";
		
		return fmt::format(R"({{
			"status": "success",
			"data": {{
				"total_processes": {},
				"processes": {},
				"timestamp": {}
			}}
		}})", numpids, processes_json.str(), timestamp);
	} catch (const std::exception& e) {
		return fmt::format("{{\"error\": \"{}\"}}", e.what());
	}
}

std::string Server::get_cpu_info() {
	try {
		trigger_plugin_refresh();
		
		// Get CPU data via RPC
		#ifdef __COSMOPOLITAN__
		auto cpu_data = Cpu::collect(false);
		string cpu_name = Cpu::get_cpuName();
		string cpu_freq = Cpu::get_cpuHz();
		bool has_sensors = Cpu::get_got_sensors();
		#else
		cpu_info cpu_data;
		string cpu_name = "Unknown CPU";
		string cpu_freq = "0 MHz";
		bool has_sensors = false;
		#endif
		
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
		
		return fmt::format(R"({{
			"status": "success",
			"data": {{
				"name": "{}",
				"frequency": "{}",
				"usage_percent": {:.1f},
				"temperature": {:.1f},
				"temp_max": {},
				"has_sensors": {},
				"load_avg": [{:.2f}, {:.2f}, {:.2f}],
				"core_count": {},
				"timestamp": {}
			}}
		}})", cpu_name, cpu_freq, cpu_usage, temp, cpu_data.temp_max, 
		     has_sensors ? "true" : "false",
		     cpu_data.load_avg[0], cpu_data.load_avg[1], cpu_data.load_avg[2],
		     cpu_data.core_percent.size(), timestamp);
	} catch (const std::exception& e) {
		return fmt::format("{{\"error\": \"{}\"}}", e.what());
	}
}

std::string Server::get_memory_info() {
	try {
		trigger_plugin_refresh();
		
		// Get memory data via RPC
		#ifdef __COSMOPOLITAN__
		auto mem_data = Mem::collect(false);
		uint64_t total_mem = Mem::get_totalMem();
		bool has_swap = Mem::get_has_swap();
		#else
		mem_info mem_data;
		uint64_t total_mem = 0;
		bool has_swap = false;
		#endif
		
		std::time_t timestamp = std::time(nullptr);
		
		// Calculate memory usage percentages
		double used_percent = 0.0, available_percent = 0.0, cached_percent = 0.0;
		if (total_mem > 0) {
			used_percent = (double)mem_data.stats["used"] * 100.0 / total_mem;
			available_percent = (double)mem_data.stats["available"] * 100.0 / total_mem;
			cached_percent = (double)mem_data.stats["cached"] * 100.0 / total_mem;
		}
		
		return fmt::format(R"({{
			"status": "success",
			"data": {{
				"total": {},
				"used": {},
				"used_percent": {:.1f},
				"available": {},
				"available_percent": {:.1f},
				"cached": {},
				"cached_percent": {:.1f},
				"free": {},
				"swap_total": {},
				"swap_used": {},
				"swap_free": {},
				"has_swap": {},
				"timestamp": {}
			}}
		}})", total_mem, mem_data.stats["used"], used_percent,
		     mem_data.stats["available"], available_percent,
		     mem_data.stats["cached"], cached_percent,
		     mem_data.stats["free"], mem_data.stats["swap_total"],
		     mem_data.stats["swap_used"], mem_data.stats["swap_free"],
		     has_swap ? "true" : "false", timestamp);
	} catch (const std::exception& e) {
		return fmt::format("{{\"error\": \"{}\"}}", e.what());
	}
}

std::string Server::get_network_info() {
	try {
		trigger_plugin_refresh();
		
		// Get network data via RPC
		#ifdef __COSMOPOLITAN__
		auto net_data = Net::collect(false);
		string selected_iface = Net::get_selected_iface();
		auto interfaces = Net::get_interfaces();
		auto current_net = Net::get_current_net();
		#else
		net_info net_data;
		string selected_iface = "unknown";
		vector<string> interfaces;
		unordered_map<string, net_info> current_net;
		#endif
		
		std::time_t timestamp = std::time(nullptr);
		
		// Get current network speeds
		uint64_t download_speed = 0, upload_speed = 0;
		if (!net_data.bandwidth["download"].empty()) {
			download_speed = net_data.bandwidth["download"].back();
		}
		if (!net_data.bandwidth["upload"].empty()) {
			upload_speed = net_data.bandwidth["upload"].back();
		}
		
		// Build interfaces JSON
		std::stringstream interfaces_json;
		interfaces_json << "[";
		for (size_t i = 0; i < interfaces.size(); ++i) {
			if (i > 0) interfaces_json << ",";
			interfaces_json << "\"" << interfaces[i] << "\"";
		}
		interfaces_json << "]";
		
		return fmt::format(R"({{
			"status": "success",
			"data": {{
				"selected_interface": "{}",
				"interfaces": {},
				"connected": {},
				"ipv4": "{}",
				"ipv6": "{}",
				"download_speed": {},
				"upload_speed": {},
				"download_total": {},
				"upload_total": {},
				"timestamp": {}
			}}
		}})", selected_iface, interfaces_json.str(), 
		     net_data.connected ? "true" : "false",
		     net_data.ipv4, net_data.ipv6,
		     download_speed, upload_speed,
		     net_data.stat["download"].total, net_data.stat["upload"].total,
		     timestamp);
	} catch (const std::exception& e) {
		return fmt::format("{{\"error\": \"{}\"}}", e.what());
	}
}

std::string Server::get_gpu_info() {
	try {
		trigger_plugin_refresh();
		
		// Get GPU data via RPC
		#ifdef __COSMOPOLITAN__
		auto gpu_data = Gpu::collect(false);
		int gpu_count = Gpu::get_count();
		auto gpu_names = Gpu::get_gpu_names();
		#else
		vector<gpu_info> gpu_data;
		int gpu_count = 0;
		vector<string> gpu_names;
		#endif
		
		std::time_t timestamp = std::time(nullptr);
		
		// Build GPU info JSON
		std::stringstream gpus_json;
		gpus_json << "[";
		for (int i = 0; i < gpu_count && i < (int)gpu_data.size(); ++i) {
			if (i > 0) gpus_json << ",";
			
			// Get current GPU utilization
			double gpu_usage = 0.0;
			if (!gpu_data[i].gpu_percent["gpu-totals"].empty()) {
				gpu_usage = gpu_data[i].gpu_percent["gpu-totals"].back();
			}
			
			// Get current temperature
			double temp = 0.0;
			if (!gpu_data[i].temp.empty()) {
				temp = gpu_data[i].temp.back();
			}
			
			string gpu_name = i < (int)gpu_names.size() ? gpu_names[i] : "Unknown GPU";
			
			gpus_json << fmt::format(R"({{
				"index": {},
				"name": "{}",
				"usage_percent": {:.1f},
				"temperature": {:.1f},
				"temp_max": {},
				"memory_total": {},
				"memory_used": {},
				"power_usage": {},
				"power_max": {},
				"clock_speed": {}
			}})", i, gpu_name, gpu_usage, temp, gpu_data[i].temp_max,
			         gpu_data[i].mem_total, gpu_data[i].mem_used,
			         gpu_data[i].pwr_usage, gpu_data[i].pwr_max_usage,
			         gpu_data[i].gpu_clock_speed);
		}
		gpus_json << "]";
		
		return fmt::format(R"({{
			"status": "success",
			"data": {{
				"gpu_count": {},
				"gpus": {},
				"timestamp": {}
			}}
		}})", gpu_count, gpus_json.str(), timestamp);
	} catch (const std::exception& e) {
		return fmt::format("{{\"error\": \"{}\"}}", e.what());
	}
}

std::string Server::get_npu_info() {
	try {
		trigger_plugin_refresh();
		
		// Get NPU data via RPC
		#ifdef __COSMOPOLITAN__
		auto npu_data = Npu::collect(false);
		int npu_count = Npu::get_count();
		auto npu_names = Npu::get_npu_names();
		#else
		vector<npu_info> npu_data;
		int npu_count = 0;
		vector<string> npu_names;
		#endif
		
		std::time_t timestamp = std::time(nullptr);
		
		// Build NPU info JSON
		std::stringstream npus_json;
		npus_json << "[";
		for (int i = 0; i < npu_count && i < (int)npu_data.size(); ++i) {
			if (i > 0) npus_json << ",";
			
			// Get current NPU utilization
			double npu_usage = 0.0;
			if (!npu_data[i].npu_percent["npu-totals"].empty()) {
				npu_usage = npu_data[i].npu_percent["npu-totals"].back();
			}
			
			string npu_name = i < (int)npu_names.size() ? npu_names[i] : "Unknown NPU";
			
			npus_json << fmt::format(R"({{
				"index": {},
				"name": "{}",
				"usage_percent": {:.1f}
			}})", i, npu_name, npu_usage);
		}
		npus_json << "]";
		
		return fmt::format(R"({{
			"status": "success",
			"data": {{
				"npu_count": {},
				"npus": {},
				"timestamp": {}
			}}
		}})", npu_count, npus_json.str(), timestamp);
	} catch (const std::exception& e) {
		return fmt::format("{{\"error\": \"{}\"}}", e.what());
	}
}

std::string Server::get_system_info() {
	try {
		std::time_t timestamp = std::time(nullptr);
		
		// Get additional system information
		#ifdef __COSMOPOLITAN__
		string cpu_name = Cpu::get_cpuName();
		uint64_t total_mem = Mem::get_totalMem();
		bool has_battery = Cpu::get_has_battery();
		int gpu_count = Gpu::get_count();
		int npu_count = Npu::get_count();
		#else
		string cpu_name = "Unknown CPU";
		uint64_t total_mem = 0;
		bool has_battery = false;
		int gpu_count = 0;
		int npu_count = 0;
		#endif
		
		return fmt::format(R"({{
			"status": "success",
			"data": {{
				"name": "cosmotop",
				"version": "{}",
				"plugin_loaded": {},
				"cpu_name": "{}",
				"total_memory": {},
				"has_battery": {},
				"gpu_count": {},
				"npu_count": {},
				"mcp_server": {{
					"address": "{}",
					"port": {}
				}},
				"timestamp": {}
			}}
		}})", Global::Version, is_plugin_loaded() ? "true" : "false",
		     cpu_name, total_mem, has_battery ? "true" : "false",
		     gpu_count, npu_count,
		     Config::getS("mcp_server_address"), Config::getI("mcp_server_port"),
		     timestamp);
	} catch (const std::exception& e) {
		return fmt::format("{{\"error\": \"{}\"}}", e.what());
	}
}

bool init_mcp_server() {
	if (server_instance) {
		return false; // Already initialized
	}

	std::string address = Config::getS("mcp_server_address");
	int port = Config::getI("mcp_server_port");
	
	server_instance = std::make_unique<Server>(address, port);
	return server_instance->start();
}

void shutdown_mcp_server() {
	if (server_instance) {
		server_instance->stop();
		server_instance.reset();
	}
}

} // namespace MCP