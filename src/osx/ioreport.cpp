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

#include "ioreport.hpp"

#include <vector>

#include "../cosmotop_tools.hpp"

/* Only available on M series Macs */
#ifdef __aarch64__

static std::string CFStringRefToString(CFStringRef str, UInt32 encoding = kCFStringEncodingUTF8) {
	if (str == nullptr) {
		return "";
	}

	auto cstr = CFStringGetCStringPtr(str, encoding);
	if (cstr != nullptr) {
		return std::string(cstr);
	}

	auto length = CFStringGetLength(str);
	auto max_size = CFStringGetMaximumSizeForEncoding(length, encoding);
	std::vector<char> buffer(max_size, 0);
	CFStringGetCString(str, buffer.data(), max_size, encoding);
	return std::string(buffer.data());
}

// this sampling interval seems to give results similar to asitop
// a lower sampling interval doesn't seem to produce better results
const long sampling_interval = 1000; // ms

IOReportSubscription::IOReportSubscription(const std::vector<uint32_t>& gpu_freqs_init): gpu_freqs(gpu_freqs_init) {
	ane_power = 0;
	gpu_power = 0;
	gpu_ram_power = 0;
	gpu_freq_mhz = 0;
	gpu_utilization = 0.0f;

	thread_stop = false;
	thread = new std::thread([this]() {
		CFMutableDictionaryRef energy_model_channel = IOReportCopyChannelsInGroup(CFSTR("Energy Model"), nullptr, 0, 0, 0);
		CFMutableDictionaryRef pmp_channel = IOReportCopyChannelsInGroup(CFSTR("PMP"), nullptr, 0, 0, 0);
		CFMutableDictionaryRef gpu_stats_channel = IOReportCopyChannelsInGroup(CFSTR("GPU Stats"), CFSTR("GPU Performance States"), 0, 0, 0);
		IOReportMergeChannels(energy_model_channel, pmp_channel, nullptr);
		IOReportMergeChannels(energy_model_channel, gpu_stats_channel, nullptr);
		CFMutableDictionaryRef power_subchannel = nullptr;
		struct IOReportSubscriptionRef *power_subscription = IOReportCreateSubscription(nullptr, energy_model_channel, &power_subchannel, 0, nullptr);

		while (!thread_stop) {
			CFDictionaryRef sample_a = IOReportCreateSamples(power_subscription, power_subchannel, nullptr);
			std::this_thread::sleep_for(std::chrono::milliseconds(sampling_interval));
			CFDictionaryRef sample_b = IOReportCreateSamples(power_subscription, power_subchannel, nullptr);
			if (!sample_a or !sample_b) {
				Logger::error("Failed to create IOReport samples");
				ane_power = 0;
				if (!has_ane.has_value()) {
					has_ane = false;
				}
				if (sample_a) CFRelease(sample_a);
				if (sample_b) CFRelease(sample_b);
				break;
			}

			CFDictionaryRef delta = IOReportCreateSamplesDelta(sample_a, sample_b, nullptr);
			CFRelease(sample_a);
			CFRelease(sample_b);

			IOReportIterate(delta, ^int (IOReportSampleRef sample) {
				std::string group = CFStringRefToString(IOReportChannelGetGroup(sample));
				std::string name = CFStringRefToString(IOReportChannelGetChannelName(sample));
				std::string units = CFStringRefToString(IOReportChannelGetUnitLabel(sample));
				int format = IOReportChannelGetFormat(sample);

				// Process ANE channels
				if (
					(group == "PMP" and name == "ANE") or
					(group == "Energy Model" and name.starts_with("ANE"))
				) {
					double value = format == kIOReportFormatSimple ? IOReportSimpleGetIntegerValue(sample, nullptr) : 0;
					if (units.find("mJ") != std::string::npos) {
						value /= 1000; // convert to joules
					} else if (units.find("uJ") != std::string::npos) {
						value /= 1000000; // convert to joules
					} else if (units.find("nJ") != std::string::npos) {
						value /= 1000000000; // convert to joules
					}

					ane_power = value / (sampling_interval / 1000.0); // convert to watts
					if (!has_ane.has_value()) {
						has_ane = true;
					}
				}

				// Process GPU Energy channels
				if (group == "Energy Model") {
					if (name == "GPU Energy") {
						double value = format == kIOReportFormatSimple ? IOReportSimpleGetIntegerValue(sample, nullptr) : 0;
						if (units.find("mJ") != std::string::npos) {
							value /= 1000; // convert to joules
						} else if (units.find("uJ") != std::string::npos) {
							value /= 1000000; // convert to joules
						} else if (units.find("nJ") != std::string::npos) {
							value /= 1000000000; // convert to joules
						}

						gpu_power = value / (sampling_interval / 1000.0); // convert to watts
						if (!has_gpu.has_value()) {
							has_gpu = true;
						}
					} else if (name.find("GPU SRAM") != std::string::npos) {
						double value = format == kIOReportFormatSimple ? IOReportSimpleGetIntegerValue(sample, nullptr) : 0;
						if (units.find("mJ") != std::string::npos) {
							value /= 1000; // convert to joules
						} else if (units.find("uJ") != std::string::npos) {
							value /= 1000000; // convert to joules
						} else if (units.find("nJ") != std::string::npos) {
							value /= 1000000000; // convert to joules
						}

						gpu_ram_power = value / (sampling_interval / 1000.0); // convert to watts
					}
				}

				// Process GPU frequency/utilization (state residency)
				if (group == "GPU Stats" and name == "GPUPH" and format == kIOReportFormatState) {
					if (!gpu_freqs.empty()) {
						int32_t state_count = IOReportStateGetCount(sample);

						// Find offset: skip idle states (typically first state is "OFF")
						int32_t offset = 0;
						for (int32_t i = 0; i < state_count; i++) {
							CFStringRef state_name = IOReportStateGetNameForIndex(sample, i);
							std::string state_str = CFStringRefToString(state_name);
							if (state_str != "OFF" && state_str != "IDLE" && state_str != "DOWN") {
								offset = i;
								break;
							}
						}

						// Calculate total time and active time
						int64_t total_time = 0;
						int64_t active_time = 0;
						for (int32_t i = 0; i < state_count; i++) {
							int64_t residency = IOReportStateGetResidency(sample, i);
							total_time += residency;
							if (i >= offset) {
								active_time += residency;
							}
						}

						// Calculate weighted average frequency
						double weighted_freq = 0.0;
						if (active_time > 0) {
							for (size_t i = 0; i < gpu_freqs.size() && (i + offset) < state_count; i++) {
								int64_t residency = IOReportStateGetResidency(sample, i + offset);
								double percent = (double)residency / active_time;
								weighted_freq += percent * gpu_freqs[i];
							}
						}

						// Calculate normalized utilization
						float usage_ratio = total_time > 0 ? (float)active_time / total_time : 0.0f;
						float max_freq = gpu_freqs.empty() ? 1.0f : gpu_freqs.back();
						float normalized_usage = max_freq > 0 ? (weighted_freq / max_freq) * usage_ratio : 0.0f;

						gpu_freq_mhz = (uint32_t)weighted_freq;
						gpu_utilization = normalized_usage;

						if (!has_gpu.has_value()) {
							has_gpu = true;
						}
					}
				}

				return kIOReportIterOk;
			});
			CFRelease(delta);

			if (!has_ane.has_value() && !has_gpu.has_value()) {
				has_ane = false;
				has_gpu = false;
				break;
			}
		}
	});

	while (!has_ane.has_value() && !has_gpu.has_value()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	// Set defaults if not detected
	if (!has_ane.has_value()) {
		has_ane = false;
	}
	if (!has_gpu.has_value()) {
		has_gpu = false;
	}
}

IOReportSubscription::~IOReportSubscription() {
	thread_stop = true;
	thread->join();
	delete thread;
}

bool IOReportSubscription::hasANE() {
	return has_ane.value();
}

double IOReportSubscription::getANEPower() {
	return ane_power;
}

bool IOReportSubscription::hasGPU() {
	return has_gpu.value();
}

double IOReportSubscription::getGPUPower() {
	return gpu_power;
}

double IOReportSubscription::getGPURAMPower() {
	return gpu_ram_power;
}

uint32_t IOReportSubscription::getGPUFrequency() {
	return gpu_freq_mhz;
}

float IOReportSubscription::getGPUUtilization() {
	return gpu_utilization;
}

#endif // __aarch64__