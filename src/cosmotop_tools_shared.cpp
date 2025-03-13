/* Copyright 2021 Aristocratos (jakob@qvantnet.com)
   Copyright 2025 Brett Jia (dev.bjia56@gmail.com)

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

#include <cmath>
#include <codecvt>
#include <iostream>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <utility>
#include <unordered_map>

#include <widechar_width.h>

#include "cosmotop_shared.hpp"
#include "cosmotop_tools.hpp"
#include "cosmotop_config.hpp"

using std::cout;
using std::floor;
using std::flush;
using std::max;
using std::string_view;
using std::to_string;

using namespace std::literals; // to use operator""s

namespace fs = std::filesystem;

//? ------------------------------------------------- NAMESPACES ------------------------------------------------------

//? --------------------------------------------------- FUNCTIONS -----------------------------------------------------

// ! Disabled due to issue when compiling with musl, reverted back to using regex
// namespace Fx {
// 	string uncolor(const string& s) {
// 		string out = s;
// 		for (size_t offset = 0, start_pos = 0, end_pos = 0;;) {
// 			start_pos = (offset == 0) ? out.find('\x1b') : offset;
// 			if (start_pos == string::npos)
// 				break;
// 			offset = start_pos + 1;
// 			end_pos = out.find('m', offset);
// 			if (end_pos == string::npos)
// 				break;
// 			else if (auto next_pos = out.find('\x1b', offset); not isdigit(out[end_pos - 1]) or end_pos > next_pos) {
// 			 	offset = next_pos;
// 				continue;
// 			}

// 			out.erase(start_pos, (end_pos - start_pos)+1);
// 			offset = 0;
// 		}
// 		out.shrink_to_fit();
// 		return out;
// 	}
// }

namespace Tools {

	size_t wide_ulen(const string& str) {
		unsigned int chars = 0;
		try {
			std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
			auto w_str = conv.from_bytes((str.size() > 10000 ? str.substr(0, 10000).c_str() : str.c_str()));

			for (auto c : w_str) {
				chars += widechar_wcwidth(c);
			}
		}
		catch (...) {
			return ulen(str);
		}

		return chars;
	}

	size_t wide_ulen(const std::wstring& w_str) {
		unsigned int chars = 0;

		for (auto c : w_str) {
			chars += widechar_wcwidth(c);
		}

		return chars;
	}

	string uresize(string str, const size_t len, bool wide) {
		if (len < 1 or str.empty())
			return "";

		if (wide) {
			try {
				std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
				auto w_str = conv.from_bytes((str.size() > 10000 ? str.substr(0, 10000).c_str() : str.c_str()));
				while (wide_ulen(w_str) > len)
					w_str.pop_back();
				string n_str = conv.to_bytes(w_str);
				return n_str;
			}
			catch (...) {
				return uresize(str, len, false);
			}
		}
		else {
			for (size_t x = 0, i = 0; i < str.size(); i++) {
				if ((static_cast<unsigned char>(str.at(i)) & 0xC0) != 0x80) x++;
				if (x >= len + 1) {
					str.resize(i);
					break;
				}
			}
		}
		str.shrink_to_fit();
		return str;
	}

	string luresize(string str, const size_t len, bool wide) {
		if (len < 1 or str.empty())
			return "";

		for (size_t x = 0, last_pos = 0, i = str.size() - 1; i > 0 ; i--) {
			if (wide and static_cast<unsigned char>(str.at(i)) > 0xef) {
				x += 2;
				last_pos = max((size_t)0, i - 1);
			}
			else if ((static_cast<unsigned char>(str.at(i)) & 0xC0) != 0x80) {
				x++;
				last_pos = i;
			}
			if (x >= len) {
				str = str.substr(last_pos);
				str.shrink_to_fit();
				break;
			}
		}
		return str;
	}

	string s_replace(const string& str, const string& from, const string& to) {
		string out = str;
		for (size_t start_pos = out.find(from); start_pos != std::string::npos; start_pos = out.find(from)) {
			out.replace(start_pos, from.length(), to);
		}
		return out;
	}

	string ltrim(const string& str, const string& t_str) {
		std::string_view str_v{str};
		while (str_v.starts_with(t_str))
			str_v.remove_prefix(t_str.size());

		return string{str_v};
	}

	string rtrim(const string& str, const string& t_str) {
		std::string_view str_v{str};
		while (str_v.ends_with(t_str))
			str_v.remove_suffix(t_str.size());

		return string{str_v};
	}

	auto ssplit(const string& str, const char& delim, bool discard_empty) -> vector<string> {
		vector<string> out;
		for (const auto& s : str 	| rng::views::split(delim)
									| rng::views::transform([](auto &&rng) {
										if (rng.begin() == rng.end()) return std::string_view();
										return std::string_view(&*rng.begin(), rng::distance(rng));
		})) {
			if (not s.empty() or not discard_empty) out.emplace_back(s);
		}
		return out;
	}

	string ljust(string str, const size_t x, bool utf, bool wide, bool limit) {
		if (utf) {
			if (limit and ulen(str, wide) > x)
				return uresize(str, x, wide);

			return str + string(max((int)(x - ulen(str)), 0), ' ');
		}
		else {
			if (limit and str.size() > x) {
				str.resize(x);
				return str;
			}
			return str + string(max((int)(x - str.size()), 0), ' ');
		}
	}

	string rjust(string str, const size_t x, bool utf, bool wide, bool limit) {
		if (utf) {
			if (limit and ulen(str, wide) > x)
				return uresize(str, x, wide);

			return string(max((int)(x - ulen(str)), 0), ' ') + str;
		}
		else {
			if (limit and str.size() > x) {
				str.resize(x);
				return str;
			};
			return string(max((int)(x - str.size()), 0), ' ') + str;
		}
	}

	string cjust(string str, const size_t x, bool utf, bool wide, bool limit) {
		if (utf) {
			if (limit and ulen(str, wide) > x)
				return uresize(str, x, wide);

			return string(max((int)ceil((double)(x - ulen(str)) / 2), 0), ' ') + str + string(max((int)floor((double)(x - ulen(str)) / 2), 0), ' ');
		}
		else {
			if (limit and str.size() > x) {
				str.resize(x);
				return str;
			}
			return string(max((int)ceil((double)(x - str.size()) / 2), 0), ' ') + str + string(max((int)floor((double)(x - str.size()) / 2), 0), ' ');
		}
	}

	string trans(const string& str) {
		std::string_view oldstr{str};
		string newstr;
		newstr.reserve(str.size());
		for (size_t pos; (pos = oldstr.find(' ')) != string::npos;) {
			newstr.append(oldstr.substr(0, pos));
			size_t x = 0;
			while (pos + x < oldstr.size() and oldstr.at(pos + x) == ' ') x++;
			newstr.append(Mv::r(x));
			oldstr.remove_prefix(pos + x);
		}
		return (newstr.empty()) ? str : newstr + string{oldstr};
	}

	string sec_to_dhms(size_t seconds, bool no_days, bool no_seconds) {
		size_t days = seconds / 86400; seconds %= 86400;
		size_t hours = seconds / 3600; seconds %= 3600;
		size_t minutes = seconds / 60; seconds %= 60;
		string out 	= (not no_days and days > 0 ? to_string(days) + "d " : "")
					+ (hours < 10 ? "0" : "") + to_string(hours) + ':'
					+ (minutes < 10 ? "0" : "") + to_string(minutes)
					+ (not no_seconds ? ":" + string(std::cmp_less(seconds, 10) ? "0" : "") + to_string(seconds) : "");
		return out;
	}

	string floating_humanizer(uint64_t value, bool shorten, size_t start, bool bit, bool per_second) {
		string out;
		const size_t mult = (bit) ? 8 : 1;
		bool mega = Config::getB("base_10_sizes");

		// taking advantage of type deduction for array creation (since C++17)
		// combined with string literals (operator""s)
		static const array mebiUnits_bit {
			"bit"s, "Kib"s, "Mib"s,
			"Gib"s, "Tib"s, "Pib"s,
			"Eib"s, "Zib"s, "Yib"s,
			"Bib"s, "GEb"s
		};
		static const array mebiUnits_byte {
			"Byte"s, "KiB"s, "MiB"s,
			"GiB"s, "TiB"s, "PiB"s,
			"EiB"s, "ZiB"s, "YiB"s,
			"BiB"s, "GEB"s
		};
		static const array megaUnits_bit {
			"bit"s, "Kb"s, "Mb"s,
			"Gb"s, "Tb"s, "Pb"s,
			"Eb"s, "Zb"s, "Yb"s,
			"Bb"s, "Gb"s
		};
		static const array megaUnits_byte {
			"Byte"s, "KB"s, "MB"s,
			"GB"s, "TB"s, "PB"s,
			"EB"s, "ZB"s, "YB"s,
			"BB"s, "GB"s
		};
		const auto& units = (bit) ? ( mega ? megaUnits_bit : mebiUnits_bit) : ( mega ? megaUnits_byte : mebiUnits_byte);

		value *= 100 * mult;

		if (mega) {
			while (value >= 100000) {
				value /= 1000;
				if (value < 100) {
					out = to_string(value);
					break;
				}
				start++;
			}
		}
		else {
			while (value >= 102400) {
				value >>= 10;
				if (value < 100) {
					out = to_string(value);
					break;
				}
				start++;
			}
		}
		if (out.empty()) {
			out = to_string(value);
			if (not mega and out.size() == 4 and start > 0) {
				out.pop_back();
				out.insert(2, ".");
			}
			else if (out.size() == 3 and start > 0) {
				out.insert(1, ".");
			}
			else if (out.size() >= 2) {
				out.resize(out.size() - 2);
			}
		}
		if (shorten) {
			auto f_pos = out.find('.');
			if (f_pos == 1 and out.size() > 3) {
				out = to_string(round(stod(out) * 10) / 10).substr(0,3);
			}
			else if (f_pos != string::npos) {
				out = to_string((int)round(stod(out)));
			}
			if (out.size() > 3) {
				out = to_string((int)(out[0] - '0')) + ".0";
				start++;
			}
			out.push_back(units[start][0]);
		}
		else out += " " + units[start];

		if (per_second) out += (bit) ? "ps" : "/s";
		return out;
	}

	std::string operator*(const string& str, int64_t n) {
		if (n < 1 or str.empty()) {
			return "";
		}
		else if (n == 1) {
			return str;
		}

		string new_str;
		new_str.reserve(str.size() * n);

		for (; n > 0; n--)
			new_str.append(str);

		return new_str;
	}

#ifndef _WIN32
	string strf_time(const string& strf) {
		auto in_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm bt {};
		std::stringstream ss;
		ss << std::put_time(localtime_r(&in_time_t, &bt), strf.c_str());
		return ss.str();
	}
#endif // !_WIN32

	void atomic_wait(const atomic<bool>& atom, bool old) noexcept {
		while (atom.load(std::memory_order_relaxed) == old ) busy_wait();
	}

	void atomic_wait_for(const atomic<bool>& atom, bool old, const uint64_t wait_ms) noexcept {
		const uint64_t start_time = time_ms();
		while (atom.load(std::memory_order_relaxed) == old and (time_ms() - start_time < wait_ms)) sleep_ms(1);
	}

	atomic_lock::atomic_lock(atomic<bool>& atom, bool wait) : atom(atom) {
		if (wait) while (not this->atom.compare_exchange_strong(this->not_true, true));
		else this->atom.store(true);
	}

	atomic_lock::~atomic_lock() {
		this->atom.store(false);
	}

	string readfile(const std::filesystem::path& path, const string& fallback) {
		if (not fs::exists(path)) return fallback;
		string out;
		try {
			std::ifstream file(path);
			for (string readstr; getline(file, readstr); out += readstr);
		}
		catch (const std::exception& e) {
			Logger::error("readfile() : Exception when reading " + path.string() + " : " + e.what());
			return fallback;
		}
		return (out.empty() ? fallback : out);
	}

	auto celsius_to(const long long& celsius, const string& scale) -> tuple<long long, string> {
		if (scale == "celsius")
			return {celsius, "°C"};
		else if (scale == "fahrenheit")
			return {(long long)round((double)celsius * 1.8 + 32), "°F"};
		else if (scale == "kelvin")
			return {(long long)round((double)celsius + 273.15), "K "};
		else if (scale == "rankine")
			return {(long long)round((double)celsius * 1.8 + 491.67), "°R"};
		return {0, ""};
	}

	string hostname() {
		char host[HOST_NAME_MAX];
		gethostname(host, HOST_NAME_MAX);
		return string{host};
	}

	string username() {
		auto user = getenv("LOGNAME");
		if (user == nullptr or strlen(user) == 0) user = getenv("USER");
		return (user != nullptr ? user : "");
	}

	DebugTimer::DebugTimer(const string name, bool start, bool delayed_report) : name(name), delayed_report(delayed_report) {
		if (start)
			this->start();
	}

	DebugTimer::~DebugTimer() {
		if (running)
			this->stop(true);
		this->force_report();
	}

	void DebugTimer::start() {
		if (running) return;
		running = true;
		start_time = time_micros();
	}

	void DebugTimer::stop(bool report) {
		if (not running) return;
		running = false;
		elapsed_time = time_micros() - start_time;
		if (report) this->report();
	}

	void DebugTimer::reset(bool restart) {
		running = false;
		start_time = 0;
		elapsed_time = 0;
		if (restart) this->start();
	}

	void DebugTimer::stop_rename_reset(const string &new_name, bool report, bool restart) {
		this->stop(report);
		name = new_name;
		this->reset(restart);
	}

	void DebugTimer::report() {
		string report_line;
		if (start_time == 0 and elapsed_time == 0)
			report_line = fmt::format("DebugTimer::report() warning -> Timer [{}] has not been started!", name);
		else if (running)
			report_line = fmt::format(custom_locale, "Timer [{}] (running) currently at {:L} μs", name, time_micros() - start_time);
		else
			report_line = fmt::format(custom_locale, "Timer [{}] took {:L} μs", name, elapsed_time);

		if (delayed_report)
			report_buffer.emplace_back(report_line);
		else
			Logger::log_write(log_level, report_line);
	}

	void DebugTimer::force_report() {
		if (report_buffer.empty()) return;
		for (const auto& line : report_buffer)
			Logger::log_write(log_level, line);
		report_buffer.clear();
	}

	uint64_t DebugTimer::elapsed() {
		if (running)
			return time_micros() - start_time;
		return elapsed_time;
	}

	bool DebugTimer::is_running() {
		return running;
	}

#ifdef __x86_64__
	string cpuid(uint32_t eax) {
		uint32_t ebx = 0, ecx = 0, edx = 0;

		// Execute CPUID to get the cpuid string
		asm volatile (
			"cpuid"
			: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
			: "a" (eax)
		);
	
		// Allocate 13 bytes for 12 characters plus null terminator
		char result[13];
	
		// Copy registers to the string in correct order: EBX, EDX, ECX
		memcpy(result, &ebx, 4);
		memcpy(result + 4, &ecx, 4);
		memcpy(result + 8, &edx, 4);
		result[12] = '\0'; // Null-terminate the string
	
		return string{result};
	}
#endif
}

#ifdef _WIN32

namespace Tools {

	HandleWrapper::HandleWrapper() : wHandle(nullptr) { ; }
	HandleWrapper::HandleWrapper(HANDLE nHandle) : wHandle(nHandle) { valid = (wHandle != INVALID_HANDLE_VALUE); }
	HANDLE HandleWrapper::operator()() { return wHandle; }
	HandleWrapper::~HandleWrapper() { if (wHandle != nullptr) CloseHandle(wHandle); }

	ServiceHandleWrapper::ServiceHandleWrapper() : wHandle(nullptr) { ; }
	ServiceHandleWrapper::ServiceHandleWrapper(SC_HANDLE nHandle) : wHandle(nHandle) { valid = (wHandle != INVALID_HANDLE_VALUE); }
	SC_HANDLE ServiceHandleWrapper::operator()() { return wHandle; }
	ServiceHandleWrapper::~ServiceHandleWrapper() { if (wHandle != nullptr) CloseServiceHandle(wHandle); }

	ServiceConfigWrapper::ServiceConfigWrapper() : conf(nullptr) { ; }
	ServiceConfigWrapper::ServiceConfigWrapper(DWORD bufSize) {
		conf = reinterpret_cast<LPQUERY_SERVICE_CONFIG>(LocalAlloc(LMEM_FIXED, bufSize));
		valid = (conf != nullptr);
	}
	LPQUERY_SERVICE_CONFIG ServiceConfigWrapper::operator()() { return conf; }
	ServiceConfigWrapper::~ServiceConfigWrapper() { if (conf != nullptr) LocalFree(conf); }

	DWORD ServiceCommand(string name, ServiceCommands command) {
		//? Open handle to service manager
		ServiceHandleWrapper SCmanager(OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
		if (not SCmanager.valid) {
			Logger::error("Tools::ServiceCommand(): OpenSCManager() failed with error code: " + to_string(GetLastError()));
			return ERROR_INVALID_FUNCTION;
		}

		//? Open handle to service
		ServiceHandleWrapper SCitem(OpenService(SCmanager(), _bstr_t(name.c_str()), SERVICE_ALL_ACCESS));
		if (not SCitem.valid) {
			Logger::error("Tools::ServiceCommand(): OpenService() failed with error code: " + to_string(GetLastError()));
			return ERROR_INVALID_FUNCTION;
		}

		SERVICE_STATUS_PROCESS itemStat;
		DWORD BytesNeeded;

		//? Get service status
		if (not QueryServiceStatusEx(SCitem(), SC_STATUS_PROCESS_INFO, (LPBYTE)&itemStat, sizeof(SERVICE_STATUS_PROCESS), &BytesNeeded)) {
			Logger::error("Tools::ServiceCommand(): QueryServiceStatusEx() failed with error code: " + to_string(GetLastError()));
			return ERROR_INVALID_FUNCTION;
		}

		DWORD DesiredState = NULL;
		DWORD ControlCommand;

		if (command == SCstart) {
			DesiredState = SERVICE_RUNNING;
		}
		else if (command == SCstop) {
			DesiredState = SERVICE_STOPPED;
			ControlCommand = SERVICE_CONTROL_STOP;
		}
		else if (command == SCcontinue) {
			DesiredState = SERVICE_RUNNING;
			ControlCommand = SERVICE_CONTROL_CONTINUE;
		}
		else if (command == SCpause) {
			DesiredState = SERVICE_PAUSED;
			ControlCommand = SERVICE_CONTROL_PAUSE;
		}
		else if (command == SCchange) {
			ControlCommand = SERVICE_CONTROL_PARAMCHANGE;
		}
		else {
			return ERROR_INVALID_FUNCTION;
		}

		//? Check if service is already in the desired state
		if (DesiredState != NULL and itemStat.dwCurrentState == DesiredState) {
			return ERROR_ALREADY_EXISTS;
		}

		//? Send command to service
		if (command == SCstart) {
			if (not StartService(SCitem(), 0, NULL)) {
				return GetLastError();
			}
		}
		else {
			SERVICE_STATUS scStat;
			if (not ControlService(SCitem(), ControlCommand, &scStat)) {
				return GetLastError();
			}
		}

		return ERROR_SUCCESS;
	}

	DWORD ServiceSetStart(string name, DWORD start_type) {
		//? Open handle to service manager
		ServiceHandleWrapper SCmanager(OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
		if (not SCmanager.valid) {
			Logger::error("Tools::ServiceCommand(): OpenSCManager() failed with error code: " + to_string(GetLastError()));
			return ERROR_INVALID_FUNCTION;
		}

		//? Open handle to service
		ServiceHandleWrapper SCitem(OpenService(SCmanager(), _bstr_t(name.c_str()), SERVICE_ALL_ACCESS));
		if (not SCitem.valid) {
			Logger::error("Tools::ServiceCommand(): OpenService() failed with error code: " + to_string(GetLastError()));
			return ERROR_INVALID_FUNCTION;
		}

		//? Change service start type
		if (not ChangeServiceConfig(SCitem(), SERVICE_NO_CHANGE, start_type, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
			return GetLastError();
		}

		return ERROR_SUCCESS;
	}

}

#endif // _WIN32
