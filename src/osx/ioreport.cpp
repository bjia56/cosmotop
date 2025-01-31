

#include "ioreport.hpp"

#include <fstream>
#include <thread>
#include <vector>

std::string CFStringRefToString(CFStringRef str, UInt32 encoding = kCFStringEncodingASCII) {
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

namespace Cpu {
	IOReportSubscription::IOReportSubscription() {
		energy_model_channel = IOReportCopyChannelsInGroup(CFSTR("Energy Model"), nullptr, 0, 0, 0);
		pmp_channel = IOReportCopyChannelsInGroup(CFSTR("PMP"), nullptr, 0, 0, 0);

		IOReportMergeChannels(energy_model_channel, pmp_channel, nullptr);

		power_subchannel = nullptr;
		power_subscription = IOReportCreateSubscription(nullptr, energy_model_channel, &power_subchannel, 0, nullptr);

		previous_power_sample = nullptr;
		current_power_sample = nullptr;
		sample();

		std::this_thread::sleep_for(std::chrono::seconds(2));
	}

	IOReportSubscription::~IOReportSubscription() {
		if (energy_model_channel != nullptr) {
			CFRelease(energy_model_channel);
		}
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

	void sample() {
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

		auto delta = IOReportCreateSamplesDelta(previous_power_sample->sample, current_power_sample->sample, nullptr);
		IOReportIterate(delta, ^int (IOReportSampleRef sample) {
			static std::ofstream debugOut("ane.txt");
			debugOut << "--- Sample ---" << std::endl;
			//static std::vector<std::string> names = {"CPU Stats", "GPU Stats", "AMC Stats", "CLPC Stats", "PMP", "Energy Model"};

			auto group = IOReportChannelGetGroup(sample);
			auto subgroup = IOReportChannelGetSubGroup(sample);
			auto name = IOReportChannelGetChannelName(sample);
			auto format = IOReportChannelGetFormat(sample);
			if (group == nullptr || subgroup == nullptr || name == nullptr) {
				return kIOReportIterFailed;
			}

			debugOut << "Group: " << CFStringRefToString(group) << std::endl;
			debugOut << "Subgroup: " << CFStringRefToString(subgroup) << std::endl;
			debugOut << "Name: " << CFStringRefToString(name) << std::endl;
			debugOut << "Format: " << format << std::endl;

			if (format == kIOReportFormatSimple) {
				debugOut << "Value: " << IOReportSimpleGetIntegerValue(sample, nullptr) << std::endl;
			} else if (format == kIOReportFormatSimpleArray) {
				int count = IOReportStateGetCount(sample);
				for (int i = 0; i < count; i++) {
					debugOut << "Value " << i << ": " << IOReportArrayGetValueAtIndex(sample, i) << std::endl;
				}
			} else if (format == kIOReportFormatState) {
				int count = IOReportStateGetCount(sample);
				for (int i = 0; i < count; i++) {
					auto state = IOReportStateGetNameForIndex(sample, i);
					debugOut << "State " << CFStringRefToString(state) << ": " << IOReportStateGetResidency(sample, i) << std::endl;
				}
			}

			return kIOReportIterOk;
		});
		return 0;
	}
}  // namespace Cpu