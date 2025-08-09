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

#include <windows.h>

namespace cosmotop {

struct HttpResponse {
	int status_code;
	std::string headers;
	std::string body;
	bool success;
};

class DockerHttpClient {
public:
	explicit DockerHttpClient(const std::string& pipe_path = "\\\\.\\pipe\\docker_engine");
	~DockerHttpClient();

	DockerHttpClient(const DockerHttpClient&) = delete;
	DockerHttpClient& operator=(const DockerHttpClient&) = delete;
	DockerHttpClient(DockerHttpClient&&) = delete;
	DockerHttpClient& operator=(DockerHttpClient&&) = delete;

	HttpResponse get(const std::string& path);
	HttpResponse post(const std::string& path, const std::string& body = "",
					 const std::string& content_type = "application/json");

	bool is_connected() const { return pipe_handle_ != INVALID_HANDLE_VALUE; }

private:
	bool connect_to_pipe();
	void disconnect();
	bool send_request(const std::string& method, const std::string& path,
					 const std::string& body = "", const std::string& content_type = "");
	HttpResponse read_response();

	std::string pipe_path_;
	HANDLE pipe_handle_;
	static constexpr DWORD BUFFER_SIZE = 8192;
};

}