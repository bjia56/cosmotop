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
#include <rfl/json.hpp>
#include <rfl.hpp>
#include <sstream>
#include <ctime>
#include <iostream>

// Include data structure definitions
#ifndef __COSMOPOLITAN__
#include <cosmo_plugin.hpp>
#endif

namespace MCP {

// JSON response structures for MCP protocol
struct ErrorResponse {
	std::string error;
};

struct ProcessInfo {
	int pid;
	std::string name;
	std::string user;
	double cpu_percent;
	uint64_t memory;
	std::string state;
	int threads;
};

struct ProcessData {
	int total_processes;
	std::vector<ProcessInfo> processes;
	std::time_t timestamp;
};

struct ProcessResponse {
	std::string status;
	ProcessData data;
};

struct CpuData {
	std::string name;
	std::string frequency;
	double usage_percent;
	double temperature;
	int temp_max;
	bool has_sensors;
	std::vector<double> load_avg;
	int core_count;
	std::time_t timestamp;
};

struct CpuResponse {
	std::string status;
	CpuData data;
};

struct MemoryData {
	uint64_t total;
	uint64_t used;
	double used_percent;
	uint64_t available;
	double available_percent;
	uint64_t cached;
	double cached_percent;
	uint64_t free;
	uint64_t swap_total;
	uint64_t swap_used;
	uint64_t swap_free;
	bool has_swap;
	std::time_t timestamp;
};

struct MemoryResponse {
	std::string status;
	MemoryData data;
};

struct NetworkData {
	std::string selected_interface;
	std::vector<std::string> interfaces;
	bool connected;
	std::string ipv4;
	std::string ipv6;
	uint64_t download_speed;
	uint64_t upload_speed;
	uint64_t download_total;
	uint64_t upload_total;
	std::time_t timestamp;
};

struct NetworkResponse {
	std::string status;
	NetworkData data;
};

struct GpuInfo {
	int index;
	std::string name;
	double usage_percent;
	double temperature;
	int temp_max;
	uint64_t memory_total;
	uint64_t memory_used;
	int power_usage;
	int power_max;
	int clock_speed;
};

struct GpuData {
	int gpu_count;
	std::vector<GpuInfo> gpus;
	std::time_t timestamp;
};

struct GpuResponse {
	std::string status;
	GpuData data;
};

struct NpuInfo {
	int index;
	std::string name;
	double usage_percent;
};

struct NpuData {
	int npu_count;
	std::vector<NpuInfo> npus;
	std::time_t timestamp;
};

struct NpuResponse {
	std::string status;
	NpuData data;
};

struct McpServerInfo {
	std::string address;
	int port;
};

struct SystemData {
	std::string name;
	std::string version;
	bool plugin_loaded;
	std::string cpu_name;
	uint64_t total_memory;
	bool has_battery;
	int gpu_count;
	int npu_count;
	McpServerInfo mcp_server;
	std::time_t timestamp;
};

struct SystemResponse {
	std::string status;
	SystemData data;
};

struct ToolCallContent {
	std::string type;
	std::string text;
};

struct ToolCallResponse {
	std::vector<ToolCallContent> content;
};

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
				ErrorResponse error;
				error.error = "Missing tool name";
				res.body = rfl::json::write(error);
				return;
			}
			
			// Extract tool name (simple approach)
			size_t quote_start = body.find("\"", name_pos + 7);
			size_t quote_end = body.find("\"", quote_start + 1);
			if (quote_start == std::string::npos || quote_end == std::string::npos) {
				res.status = 400;
				ErrorResponse error;
				error.error = "Invalid tool name format";
				res.body = rfl::json::write(error);
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
				ErrorResponse error;
				error.error = "Unknown tool: " + tool_name;
				res.body = rfl::json::write(error);
				return;
			}

			ToolCallResponse response;
			ToolCallContent content;
			content.type = "text";
			content.text = result;
			response.content.push_back(content);
			res.body = rfl::json::write(response);
			res.set_header("Content-Type", "application/json");
		} catch (const std::exception& e) {
			res.status = 500;
			ErrorResponse error;
			error.error = e.what();
			res.body = rfl::json::write(error);
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

	std::cout << "Starting MCP server on " << address_ << ":" << port_ << std::endl;
	
	if (!server.listen(address_, port_)) {
		std::cout << "Failed to start MCP server" << std::endl;
		running_.store(false);
	}
}

void Server::setup_tools() {
	// Ensure plugin is loaded
	if (!is_plugin_loaded()) {
		std::cout << "{\"error\": \"Plugin not loaded, exiting\"}" << std::endl;
		std::exit(1);
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
		
		ProcessResponse response;
		response.status = "success";
		response.data.total_processes = numpids;
		response.data.timestamp = timestamp;
		
		// Convert to ProcessInfo structs (limit to top 10 processes)
		for (size_t i = 0; i < proc_data.size() && i < 10; ++i) {
			ProcessInfo proc_info;
			proc_info.pid = proc_data[i].pid;
			proc_info.name = proc_data[i].name;
			proc_info.user = proc_data[i].user;
			proc_info.cpu_percent = proc_data[i].cpu_p;
			proc_info.memory = proc_data[i].mem;
			proc_info.state = proc_data[i].state;
			proc_info.threads = proc_data[i].threads;
			response.data.processes.push_back(proc_info);
		}
		
		return rfl::json::write(response);
	} catch (const std::exception& e) {
		ErrorResponse error;
		error.error = e.what();
		return rfl::json::write(error);
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
		
		CpuResponse response;
		response.status = "success";
		response.data.name = cpu_name;
		response.data.frequency = cpu_freq;
		response.data.usage_percent = cpu_usage;
		response.data.temperature = temp;
		response.data.temp_max = cpu_data.temp_max;
		response.data.has_sensors = has_sensors;
		response.data.load_avg = {cpu_data.load_avg[0], cpu_data.load_avg[1], cpu_data.load_avg[2]};
		response.data.core_count = cpu_data.core_percent.size();
		response.data.timestamp = timestamp;
		
		return rfl::json::write(response);
	} catch (const std::exception& e) {
		ErrorResponse error;
		error.error = e.what();
		return rfl::json::write(error);
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
		
		MemoryResponse response;
		response.status = "success";
		response.data.total = total_mem;
		response.data.used = mem_data.stats["used"];
		response.data.used_percent = used_percent;
		response.data.available = mem_data.stats["available"];
		response.data.available_percent = available_percent;
		response.data.cached = mem_data.stats["cached"];
		response.data.cached_percent = cached_percent;
		response.data.free = mem_data.stats["free"];
		response.data.swap_total = mem_data.stats["swap_total"];
		response.data.swap_used = mem_data.stats["swap_used"];
		response.data.swap_free = mem_data.stats["swap_free"];
		response.data.has_swap = has_swap;
		response.data.timestamp = timestamp;
		
		return rfl::json::write(response);
	} catch (const std::exception& e) {
		ErrorResponse error;
		error.error = e.what();
		return rfl::json::write(error);
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
		
		NetworkResponse response;
		response.status = "success";
		response.data.selected_interface = selected_iface;
		response.data.interfaces = interfaces;
		response.data.connected = net_data.connected;
		response.data.ipv4 = net_data.ipv4;
		response.data.ipv6 = net_data.ipv6;
		response.data.download_speed = download_speed;
		response.data.upload_speed = upload_speed;
		response.data.download_total = net_data.stat["download"].total;
		response.data.upload_total = net_data.stat["upload"].total;
		response.data.timestamp = timestamp;
		
		return rfl::json::write(response);
	} catch (const std::exception& e) {
		ErrorResponse error;
		error.error = e.what();
		return rfl::json::write(error);
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
		
		GpuResponse response;
		response.status = "success";
		response.data.gpu_count = gpu_count;
		response.data.timestamp = timestamp;
		
		// Build GPU info structs
		for (int i = 0; i < gpu_count && i < (int)gpu_data.size(); ++i) {
			GpuInfo gpu_info;
			gpu_info.index = i;
			gpu_info.name = i < (int)gpu_names.size() ? gpu_names[i] : "Unknown GPU";
			
			// Get current GPU utilization
			gpu_info.usage_percent = 0.0;
			if (!gpu_data[i].gpu_percent["gpu-totals"].empty()) {
				gpu_info.usage_percent = gpu_data[i].gpu_percent["gpu-totals"].back();
			}
			
			// Get current temperature
			gpu_info.temperature = 0.0;
			if (!gpu_data[i].temp.empty()) {
				gpu_info.temperature = gpu_data[i].temp.back();
			}
			
			gpu_info.temp_max = gpu_data[i].temp_max;
			gpu_info.memory_total = gpu_data[i].mem_total;
			gpu_info.memory_used = gpu_data[i].mem_used;
			gpu_info.power_usage = gpu_data[i].pwr_usage;
			gpu_info.power_max = gpu_data[i].pwr_max_usage;
			gpu_info.clock_speed = gpu_data[i].gpu_clock_speed;
			
			response.data.gpus.push_back(gpu_info);
		}
		
		return rfl::json::write(response);
	} catch (const std::exception& e) {
		ErrorResponse error;
		error.error = e.what();
		return rfl::json::write(error);
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
		
		NpuResponse response;
		response.status = "success";
		response.data.npu_count = npu_count;
		response.data.timestamp = timestamp;
		
		// Build NPU info structs
		for (int i = 0; i < npu_count && i < (int)npu_data.size(); ++i) {
			NpuInfo npu_info;
			npu_info.index = i;
			npu_info.name = i < (int)npu_names.size() ? npu_names[i] : "Unknown NPU";
			
			// Get current NPU utilization
			npu_info.usage_percent = 0.0;
			if (!npu_data[i].npu_percent["npu-totals"].empty()) {
				npu_info.usage_percent = npu_data[i].npu_percent["npu-totals"].back();
			}
			
			response.data.npus.push_back(npu_info);
		}
		
		return rfl::json::write(response);
	} catch (const std::exception& e) {
		ErrorResponse error;
		error.error = e.what();
		return rfl::json::write(error);
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
		
		SystemResponse response;
		response.status = "success";
		response.data.name = "cosmotop";
		response.data.version = Global::Version;
		response.data.plugin_loaded = is_plugin_loaded();
		response.data.cpu_name = cpu_name;
		response.data.total_memory = total_mem;
		response.data.has_battery = has_battery;
		response.data.gpu_count = gpu_count;
		response.data.npu_count = npu_count;
		response.data.mcp_server.address = Config::getS("mcp_server_address");
		response.data.mcp_server.port = Config::getI("mcp_server_port");
		response.data.timestamp = timestamp;
		
		return rfl::json::write(response);
	} catch (const std::exception& e) {
		ErrorResponse error;
		error.error = e.what();
		return rfl::json::write(error);
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