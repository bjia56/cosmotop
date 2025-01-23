/* Copyright 2024 Brett Jia (dev.bjia56@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

#include "intel_gpu_top.h"
#include "intel_chipset.h"

#define VENDOR_ID "0x8086"
#define SYSFS_PATH "/sys/class/drm"
#define VENDOR_FILE "vendor"
#define DEVICE_FILE "device"

// Caller must free the returned pointer
char* find_intel_gpu_dir() {
    DIR *dir;
    struct dirent *entry;
    char path[256];
    char vendor_path[256];
    char vendor_id[16];

    if ((dir = opendir(SYSFS_PATH)) == NULL) {
        perror("opendir");
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Construct the path to the vendor file
        snprintf(vendor_path, sizeof(vendor_path), "%s/%s/device/%s", SYSFS_PATH, entry->d_name, VENDOR_FILE);

        // Check if the vendor file exists
        if (access(vendor_path, F_OK) != -1) {
            FILE *file = fopen(vendor_path, "r");
            if (file) {
                if (fgets(vendor_id, sizeof(vendor_id), file)) {
                    // Trim the newline character
                    vendor_id[strcspn(vendor_id, "\n")] = 0;

                    if (strcmp(vendor_id, VENDOR_ID) == 0) {
                        // Return the parent directory (i.e., /sys/class/drm/card*)
                        snprintf(path, sizeof(path), "%s/%s", SYSFS_PATH, entry->d_name);
                        fclose(file);
                        closedir(dir);
                        return strdup(path);
                    }
                }
                fclose(file);
            }
        }
    }

    closedir(dir);
    return NULL;  // Intel GPU not found
}

// Caller must free the returned pointer
char* get_intel_device_id(const char* gpu_dir) {
    char device_path[256];
    char device_id[16];

    // Construct the path to the device file
    snprintf(device_path, sizeof(device_path), "%s/device/%s", gpu_dir, DEVICE_FILE);

    FILE *file = fopen(device_path, "r");
    if (file) {
        if (fgets(device_id, sizeof(device_id), file)) {
            fclose(file);
            // Trim the newline character
            device_id[strcspn(device_id, "\n")] = 0;
            // Return a copy of the device ID
            return strdup(device_id);
        }
        fclose(file);
    } else {
        perror("fopen");
    }

    return NULL;
}

// Caller must free the returned pointer
char *get_intel_device_name(const char *device_id) {
    uint16_t devid = strtol(device_id, NULL, 16);
    char dev_name[256];
    char full_name[256];
    const struct intel_device_info *info = intel_get_device_info(devid);
    if (info) {
        if (info->codename == NULL) {
            strcpy(dev_name, "(unknown)");
        } else {
            strcpy(dev_name, info->codename);
            dev_name[0] = toupper(dev_name[0]);
        }
        snprintf(full_name, sizeof(full_name), "Intel %s (Gen%u)", dev_name, info->graphics_ver);
        return strdup(full_name);
    }
    return NULL;
}
