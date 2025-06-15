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

namespace Npu {
	// this sampling interval seems to give results similar to asitop
	// a lower sampling interval doesn't seem to produce better results
	const long sampling_interval = 1000; // ms

	IOReportSubscription::IOReportSubscription() {
		ane_power = 0;

		thread_stop = false;
		thread = new std::thread([this]() {
			CFMutableDictionaryRef energy_model_channel = IOReportCopyChannelsInGroup(CFSTR("Energy Model"), nullptr, 0, 0, 0);
			CFMutableDictionaryRef pmp_channel = IOReportCopyChannelsInGroup(CFSTR("PMP"), nullptr, 0, 0, 0);
			IOReportMergeChannels(energy_model_channel, pmp_channel, nullptr);
			CFMutableDictionaryRef power_subchannel = nullptr;
			struct IOReportSubscriptionRef *power_subscription = IOReportCreateSubscription(nullptr, energy_model_channel, &power_subchannel, 0, nullptr);

			while (!thread_stop) {
				CFDictionaryRef sample_a = IOReportCreateSamples(power_subscription, power_subchannel, nullptr);
				std::this_thread::sleep_for(std::chrono::milliseconds(sampling_interval));
				CFDictionaryRef sample_b = IOReportCreateSamples(power_subscription, power_subchannel, nullptr);

				CFDictionaryRef delta = IOReportCreateSamplesDelta(sample_a, sample_b, nullptr);
				CFRelease(sample_a);
				CFRelease(sample_b);

				IOReportIterate(delta, ^int (IOReportSampleRef sample) {
					std::string group = CFStringRefToString(IOReportChannelGetGroup(sample));
					std::string name = CFStringRefToString(IOReportChannelGetChannelName(sample));
					std::string units = CFStringRefToString(IOReportChannelGetUnitLabel(sample));
					if (
						(group == "PMP" and name == "ANE") or
						(group == "Energy Model" and name.starts_with("ANE"))
					) {
						int format = IOReportChannelGetFormat(sample);
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
					return kIOReportIterOk;
				});
				CFRelease(delta);

				if (!has_ane.has_value()) {
					has_ane = false;
					break;
				}
			}
		});

		while (!has_ane.has_value()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
}  // namespace Npu

#endif // __aarch64__