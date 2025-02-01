

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
		pmp_channel = IOReportCopyChannelsInGroup(CFSTR("PMP"), nullptr, 0, 0, 0);

		power_subchannel = nullptr;
		power_subscription = IOReportCreateSubscription(nullptr, pmp_channel, &power_subchannel, 0, nullptr);

		previous_power_sample = nullptr;
		current_power_sample = nullptr;
		sample();
	}

	IOReportSubscription::~IOReportSubscription() {
		if (pmp_channel != nullptr) {
			CFRelease(pmp_channel);
		}
		if (power_subchannel != nullptr) {
			CFRelease(power_subchannel);
		}
		if (previous_power_sample != nullptr) {
			delete previous_power_sample;
		}
		if (current_power_sample != nullptr) {
			delete current_power_sample;
		}
	}

	void IOReportSubscription::sample() {
		if (previous_power_sample != nullptr) {
			delete previous_power_sample;
		}
		previous_power_sample = current_power_sample;
		current_power_sample = new Sample(IOReportCreateSamples(power_subscription, power_subchannel, nullptr));
	}

	long long IOReportSubscription::getANEPower() {
		sample();
		if (previous_power_sample == nullptr || current_power_sample == nullptr) {
			return 0;
		}

		double power = 0;
		double *powerRef = &power;

		CFDictionaryRef delta = IOReportCreateSamplesDelta(previous_power_sample->sample, current_power_sample->sample, nullptr);
		IOReportIterate(delta, ^int (IOReportSampleRef sample) {
			CFStringRef cf_group = IOReportChannelGetGroup(sample);
			CFStringRef cf_name = IOReportChannelGetChannelName(sample);

			std::string group = CFStringRefToString(cf_group);
			std::string name = CFStringRefToString(cf_name);
			if (group == "PMP" and name == "ANE") {
				int format = IOReportChannelGetFormat(sample);
				int value = format == kIOReportFormatSimple ? IOReportSimpleGetIntegerValue(sample, nullptr) : 0;
				*powerRef = value;
			}

			CFRelease(cf_group);
			CFRelease(cf_name);
			return kIOReportIterOk;
		});
		CFRelease(delta);

		return static_cast<long long>(power);
	}
}  // namespace Npu