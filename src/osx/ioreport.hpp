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

#include <CoreFoundation/CoreFoundation.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <optional>

enum {
	kIOReportIterOk,
	kIOReportIterFailed,
	kIOReportIterSkipped
};

enum {
	kIOReportInvalidFormat = 0,
	kIOReportFormatSimple = 1,
	kIOReportFormatState = 2,
	kIOReportFormatHistogram = 3,
	kIOReportFormatSimpleArray = 4
};

// hide this since we use gcc on Tiger, leading to confusion with the clang block closure syntax
#ifndef __POWERPC__
typedef CFDictionaryRef IOReportSampleRef;
typedef int (^ioreportiterateblock)(IOReportSampleRef sample);

extern "C" {
	extern CFMutableDictionaryRef IOReportCopyAllChannels(uint64_t, uint64_t);
	extern CFMutableDictionaryRef IOReportCopyChannelsInGroup(CFStringRef, CFStringRef, uint64_t, uint64_t, uint64_t);
	extern void IOReportMergeChannels(CFMutableDictionaryRef, CFMutableDictionaryRef, CFTypeRef);
	extern struct IOReportSubscriptionRef *IOReportCreateSubscription(void *a, CFMutableDictionaryRef desiredChannels, CFMutableDictionaryRef *subbedChannels, uint64_t channel_id, CFTypeRef b);
	extern CFDictionaryRef IOReportCreateSamples(struct IOReportSubscriptionRef *iorsub, CFMutableDictionaryRef subbedChannels, CFTypeRef a);
	extern CFDictionaryRef IOReportCreateSamplesDelta(CFDictionaryRef prev, CFDictionaryRef current, CFTypeRef a);
	extern void IOReportIterate(CFDictionaryRef samples, ioreportiterateblock);
	extern CFStringRef IOReportChannelGetGroup(IOReportSampleRef sample);
	extern CFStringRef IOReportChannelGetSubGroup(IOReportSampleRef sample);
	extern CFStringRef IOReportChannelGetChannelName(IOReportSampleRef sample);
	extern int IOReportChannelGetFormat(IOReportSampleRef sample);
	extern uint64_t IOReportSimpleGetIntegerValue(IOReportSampleRef sample, void *a);
	extern uint64_t IOReportStateGetCount(IOReportSampleRef sample);
	extern uint64_t IOReportStateGetResidency(IOReportSampleRef sample, uint32_t index);
	extern CFStringRef IOReportStateGetNameForIndex(CFDictionaryRef sample, uint32_t index);
	extern uint64_t IOReportArrayGetValueAtIndex(CFDictionaryRef sample, uint32_t index);
	extern CFStringRef IOReportChannelGetUnitLabel(CFDictionaryRef sample);
}
#endif // __POWERPC__

namespace Npu {
	// todo: neoasitop appears to use different max wattages
	// based on the cpu generation/model
	class PowerEstimate {
	public:
		PowerEstimate(std::string cpuModel);
		~PowerEstimate();

		double getANEMaxPower();

	private:
		std::string cpuModel;
	};

	class IOReportSubscription {
	public:
		IOReportSubscription();
		~IOReportSubscription();

		bool hasANE();
		double getANEPower();

	private:
		// metrics gathering thread
		std::atomic<bool> thread_stop;
		std::thread *thread;

		std::optional<bool> has_ane;
		std::atomic<double> ane_power; // watts
	};
}