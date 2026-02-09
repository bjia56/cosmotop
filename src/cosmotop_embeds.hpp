/* Copyright 2026 Brett Jia (dev.bjia56@gmail.com)

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

#pragma once

#include <string>
#include <vector>

namespace cosmotop {
namespace embeds {

// Check if embedded themes are available
bool has_embedded_themes();

// Check if embedded licenses are available
bool has_embedded_licenses();

// Get embedded theme content, returns empty string if not found
std::string get_theme(const std::string& name);

// Get embedded license content, returns empty string if not found
std::string get_license(const std::string& name);

// Get list of embedded theme names
std::vector<std::string> get_theme_names();

// Get list of embedded license names
std::vector<std::string> get_license_names();

} // namespace embeds
} // namespace cosmotop
