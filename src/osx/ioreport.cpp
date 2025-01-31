

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
	}

	IOReportSubscription::~IOReportSubscription() {
	}

	long long IOReportSubscription::getANEEnergy() {
		static std::ofstream debugOut("ane0.txt");
		CFMutableDictionaryRef subchn;
		auto chn = IOReportCopyAllChannels(0, 0);
		auto sub = IOReportCreateSubscription(nullptr, chn, &subchn, 0, nullptr);

		auto samples_a = IOReportCreateSamples(sub, subchn, nullptr);
		std::this_thread::sleep_for(std::chrono::seconds(5));
		auto samples_b = IOReportCreateSamples(sub, subchn, nullptr);

		auto delta = IOReportCreateSamplesDelta(samples_a, samples_b, nullptr);
		CFRelease(samples_a);
		CFRelease(samples_b);

		debugOut << "Sample collected!" << std::endl;

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