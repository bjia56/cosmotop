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
#include <vector>

#include "./docker_http_client.hpp"

namespace cosmotop {

DockerHttpClient::DockerHttpClient(const std::string& pipe_path)
	: pipe_path_(pipe_path), pipe_handle_(INVALID_HANDLE_VALUE) {
}

DockerHttpClient::~DockerHttpClient() {
	disconnect();
}

bool DockerHttpClient::connect_to_pipe() {
	if (pipe_handle_ != INVALID_HANDLE_VALUE) {
		return true;
	}

	pipe_handle_ = CreateFileA(
		pipe_path_.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
	);

	if (pipe_handle_ == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		if (error == ERROR_PIPE_BUSY) {
			if (WaitNamedPipeA(pipe_path_.c_str(), 2000)) {
				pipe_handle_ = CreateFileA(
					pipe_path_.c_str(),
					GENERIC_READ | GENERIC_WRITE,
					0,
					nullptr,
					OPEN_EXISTING,
					0,
					nullptr
				);
			}
		}
	}

	if (pipe_handle_ != INVALID_HANDLE_VALUE) {
		DWORD mode = PIPE_READMODE_MESSAGE;
		SetNamedPipeHandleState(pipe_handle_, &mode, nullptr, nullptr);
		return true;
	}

	return false;
}

void DockerHttpClient::disconnect() {
	if (pipe_handle_ != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe_handle_);
		pipe_handle_ = INVALID_HANDLE_VALUE;
	}
}

HttpResponse DockerHttpClient::get(const std::string& path) {
	if (!connect_to_pipe()) {
		return {0, "", "", false};
	}

	if (send_request("GET", path)) {
		return read_response();
	}

	return {0, "", "", false};
}

HttpResponse DockerHttpClient::post(const std::string& path, const std::string& body,
								   const std::string& content_type) {
	if (!connect_to_pipe()) {
		return {0, "", "", false};
	}

	if (send_request("POST", path, body, content_type)) {
		return read_response();
	}

	return {0, "", "", false};
}

bool DockerHttpClient::send_request(const std::string& method, const std::string& path,
								   const std::string& body, const std::string& content_type) {
	std::ostringstream request;
	request << method << " " << path << " HTTP/1.1\r\n";
	request << "Host: localhost\r\n";
	request << "User-Agent: cosmotop/1.0\r\n";

	if (!body.empty()) {
		request << "Content-Length: " << body.length() << "\r\n";
		if (!content_type.empty()) {
			request << "Content-Type: " << content_type << "\r\n";
		}
	}

	request << "Connection: close\r\n";
	request << "\r\n";

	if (!body.empty()) {
		request << body;
	}

	std::string request_str = request.str();
	DWORD bytes_written;

	BOOL result = WriteFile(
		pipe_handle_,
		request_str.c_str(),
		static_cast<DWORD>(request_str.length()),
		&bytes_written,
		nullptr
	);

	return result && bytes_written == request_str.length();
}

HttpResponse DockerHttpClient::read_response() {
	HttpResponse response{0, "", "", false};
	std::vector<char> buffer(BUFFER_SIZE);
	std::string raw_response;

	while (true) {
		DWORD bytes_read;
		BOOL result = ReadFile(
			pipe_handle_,
			buffer.data(),
			BUFFER_SIZE - 1,
			&bytes_read,
			nullptr
		);

		if (!result || bytes_read == 0) {
			break;
		}

		buffer[bytes_read] = '\0';
		raw_response.append(buffer.data(), bytes_read);

		if (bytes_read < BUFFER_SIZE - 1) {
			break;
		}
	}

	if (raw_response.empty()) {
		return response;
	}

	size_t header_end = raw_response.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		return response;
	}

	std::string header_section = raw_response.substr(0, header_end);
	response.body = raw_response.substr(header_end + 4);

	std::istringstream header_stream(header_section);
	std::string status_line;
	std::getline(header_stream, status_line);

	if (status_line.length() > 12) {
		std::string status_code_str = status_line.substr(9, 3);
		try {
			response.status_code = std::stoi(status_code_str);
		} catch (...) {
			response.status_code = 0;
		}
	}

	response.headers = header_section;
	response.success = response.status_code >= 200 && response.status_code < 300;

	return response;
}

}