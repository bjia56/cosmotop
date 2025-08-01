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

namespace Mcp {

	class CosmotopMCPServer;
	class ProcessToolTask;
	class CpuToolTask;
	class MemoryToolTask;
	class NetworkToolTask;
	class GpuToolTask;
	class NpuToolTask;
	class SystemToolTask;

	// Utility functions for TinyMCP server
	bool init_mcp_server();
	void shutdown_mcp_server();

}