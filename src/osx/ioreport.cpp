

#include "ioreport.hpp"

#include <vector>

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

#include <fstream>

namespace Npu {
	IOReportSubscription::IOReportSubscription() {
		ane_power = 0;

		thread_stop = false;
		thread = new std::thread([this]() {
			CFMutableDictionaryRef pmp_channel = IOReportCopyChannelsInGroup(CFSTR("PMP"), nullptr, 0, 0, 0);
			CFMutableDictionaryRef power_subchannel = nullptr;
			struct IOReportSubscriptionRef *power_subscription = IOReportCreateSubscription(nullptr, pmp_channel, &power_subchannel, 0, nullptr);

			while (!thread_stop) {
				CFDictionaryRef sample_a = IOReportCreateSamples(power_subscription, power_subchannel, nullptr);
				std::this_thread::sleep_for(std::chrono::seconds(1));
				CFDictionaryRef sample_b = IOReportCreateSamples(power_subscription, power_subchannel, nullptr);

				CFDictionaryRef delta = IOReportCreateSamplesDelta(sample_a, sample_b, nullptr);
				CFRelease(sample_a);
				CFRelease(sample_b);

				IOReportIterate(delta, ^int (IOReportSampleRef sample) {
					CFStringRef cf_group = IOReportChannelGetGroup(sample);
					CFStringRef cf_name = IOReportChannelGetChannelName(sample);

					std::string group = CFStringRefToString(cf_group);
					std::string name = CFStringRefToString(cf_name);
					if (group == "PMP" and name == "ANE") {
						int format = IOReportChannelGetFormat(sample);
						int value = format == kIOReportFormatSimple ? IOReportSimpleGetIntegerValue(sample, nullptr) : 0;
						ane_power = value;
					}

					return kIOReportIterOk;
				});
				CFRelease(delta);
			}
		});
	}

	IOReportSubscription::~IOReportSubscription() {
		thread_stop = true;
		thread->join();
		delete thread;
	}

	long long IOReportSubscription::getANEPower() {
		return ane_power;
	}
}  // namespace Npu