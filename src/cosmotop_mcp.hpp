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

#include <string>
#include <memory>
#include <thread>
#include <atomic>

namespace MCP {

	class Server {
	public:
		Server(const std::string& address, int port);
		~Server();

		bool start();
		void stop();
		bool is_running() const;

	private:
		std::string address_;
		int port_;
		std::atomic<bool> running_{false};
		std::unique_ptr<std::thread> server_thread_;
		
		void run_server();
		void setup_tools();
		
		// Tool implementations
		std::string get_processes();
		std::string get_cpu_info();
		std::string get_memory_info();
		std::string get_network_info();
		std::string get_gpu_info();
		std::string get_npu_info();
		std::string get_system_info();
	};

	// Global MCP server instance
	extern std::unique_ptr<Server> server_instance;

	// Utility functions
	bool init_mcp_server();
	void shutdown_mcp_server();
}