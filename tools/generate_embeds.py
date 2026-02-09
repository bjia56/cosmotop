#!/usr/bin/env python3
"""
Generate cosmotop_embeds.cpp with embedded themes and licenses.

Usage:
    python3 tools/generate_embeds.py [options]

Options:
    --themes-dir DIR        Directory containing .theme files (can be specified multiple times)
    --licenses-file NAME=PATH  License file to embed (can be specified multiple times)
    --licenses-dir DIR      Directory containing license files (can be specified multiple times)
    --output FILE           Output file path (default: src/cosmotop_embeds.cpp)

Examples:
    # Development build (empty embeds)
    python3 tools/generate_embeds.py

    # Release build with themes and licenses
    python3 tools/generate_embeds.py \\
        --themes-dir themes \\
        --themes-dir third_party/catppuccin/themes \\
        --licenses-file cosmotop=LICENSE \\
        --licenses-file fmt=third_party/fmt/LICENSE \\
        --output src/cosmotop_embeds.cpp
"""

import sys
import os
import argparse
from pathlib import Path
from datetime import datetime


def read_file_content(filepath: Path) -> str:
    """Read file content as string."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return f.read()
    except Exception as e:
        print(f"Warning: Could not read {filepath}: {e}", file=sys.stderr)
        return ""


def collect_themes(theme_dirs: list[Path]) -> list[tuple[str, str]]:
    """Collect themes from specified directories."""
    themes = []
    seen_names = set()
    
    for themes_dir in theme_dirs:
        if not themes_dir.exists():
            print(f"Warning: Theme directory not found: {themes_dir}", file=sys.stderr)
            continue
            
        for theme_file in sorted(themes_dir.glob("*.theme")):
            if theme_file.name in seen_names:
                continue
            content = read_file_content(theme_file)
            if content:
                themes.append((theme_file.name, content))
                seen_names.add(theme_file.name)
    
    return themes


def collect_licenses(license_files: list[tuple[str, Path]], license_dirs: list[Path]) -> list[tuple[str, str]]:
    """Collect licenses from specified files and directories."""
    licenses = []
    seen_names = set()
    
    # Process explicit license files first
    for name, filepath in license_files:
        if name in seen_names:
            continue
        content = read_file_content(filepath)
        if content:
            licenses.append((name, content))
            seen_names.add(name)
    
    # Process license directories
    for licenses_dir in license_dirs:
        if not licenses_dir.exists():
            print(f"Warning: License directory not found: {licenses_dir}", file=sys.stderr)
            continue
        
        for license_file in sorted(licenses_dir.iterdir()):
            if license_file.is_file():
                name = license_file.name
                if name in seen_names:
                    continue
                content = read_file_content(license_file)
                if content:
                    licenses.append((name, content))
                    seen_names.add(name)
    
    return licenses


def generate_themes_content(themes: list[tuple[str, str]]) -> str:
    """Generate C++ code for themes."""
    if not themes:
        return "\t// No themes embedded"
    
    lines = ["\t// Themes"]
    for name, content in themes:
        # Use raw string literal R"(content)" to avoid escaping issues
        lines.append(f'\t{{"{name}", R"({content})"}},')
    
    return "\n".join(lines)


def generate_licenses_content(licenses: list[tuple[str, str]]) -> str:
    """Generate C++ code for licenses."""
    if not licenses:
        return "\t// No licenses embedded"
    
    lines = ["\t// Licenses"]
    for name, content in licenses:
        lines.append(f'\t{{"{name}", R"({content})"}},')
    
    return "\n".join(lines)


def generate_embeds_file(output_file: Path, themes: list[tuple[str, str]], licenses: list[tuple[str, str]]):
    """Generate the complete embeds file."""
    
    themes_content = generate_themes_content(themes)
    licenses_content = generate_licenses_content(licenses)
    
    has_themes = len(themes) > 0
    has_licenses = len(licenses) > 0
    
    if not has_themes and not has_licenses:
        print("Warning: No themes or licenses found. Generating empty embeds file.", file=sys.stderr)

    file_content = f'''/* Copyright 2025 Brett Jia (dev.bjia56@gmail.com)

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

This file is auto-generated by tools/generate_embeds.py
Generated at: {datetime.now().isoformat()}

For development builds, it contains empty vectors.
For release builds, it contains embedded themes and licenses.
*/

#include <string>
#include <unordered_map>
#include <vector>

namespace cosmotop {{
namespace embeds {{

// Embedded theme files: filename -> content
const std::unordered_map<std::string, std::string> themes = {{
{themes_content}
}};

// Embedded license files: name -> content  
const std::unordered_map<std::string, std::string> licenses = {{
{licenses_content}
}};

// Check if embedded themes are available
bool has_embedded_themes() {{
	return !themes.empty();
}}

// Check if embedded licenses are available
bool has_embedded_licenses() {{
	return !licenses.empty();
}}

// Get embedded theme content, returns empty string if not found
std::string get_theme(const std::string& name) {{
	auto it = themes.find(name);
	if (it != themes.end()) {{
		return it->second;
	}}
	return "";
}}

// Get embedded license content, returns empty string if not found
std::string get_license(const std::string& name) {{
	auto it = licenses.find(name);
	if (it != licenses.end()) {{
		return it->second;
	}}
	return "";
}}

// Get list of embedded theme names
std::vector<std::string> get_theme_names() {{
	std::vector<std::string> names;
	for (const auto& [name, _] : themes) {{
		names.push_back(name);
	}}
	return names;
}}

// Get list of embedded license names
std::vector<std::string> get_license_names() {{
	std::vector<std::string> names;
	for (const auto& [name, _] : licenses) {{
		names.push_back(name);
	}}
	return names;
}}

}} // namespace embeds
}} // namespace cosmotop
'''
    
    # Write the file
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(file_content)
    
    print(f"Generated {output_file}")
    if has_themes:
        print(f"  - Embedded {len(themes)} themes")
    if has_licenses:
        print(f"  - Embedded {len(licenses)} licenses")


def parse_license_file_arg(arg: str) -> tuple[str, Path]:
    """Parse a license file argument in format NAME=PATH."""
    if '=' not in arg:
        raise argparse.ArgumentTypeError(f"License file argument must be in format NAME=PATH: {arg}")
    name, path = arg.split('=', 1)
    return (name, Path(path))


def main():
    # Get repository root (parent of tools directory)
    script_dir = Path(__file__).parent.resolve()
    repo_root = script_dir.parent
    
    parser = argparse.ArgumentParser(
        description='Generate cosmotop_embeds.cpp with embedded themes and licenses'
    )
    parser.add_argument(
        '--themes-dir',
        type=Path,
        action='append',
        default=[],
        help='Directory containing .theme files (can be specified multiple times)'
    )
    parser.add_argument(
        '--licenses-file',
        type=parse_license_file_arg,
        action='append',
        default=[],
        help='License file to embed in format NAME=PATH (can be specified multiple times)'
    )
    parser.add_argument(
        '--licenses-dir',
        type=Path,
        action='append',
        default=[],
        help='Directory containing license files (can be specified multiple times)'
    )
    parser.add_argument(
        '--output',
        type=Path,
        default=repo_root / "src" / "cosmotop_embeds.cpp",
        help='Output file path (default: src/cosmotop_embeds.cpp)'
    )
    
    args = parser.parse_args()
    
    # Collect themes and licenses
    themes = collect_themes(args.themes_dir)
    licenses = collect_licenses(args.licenses_file, args.licenses_dir)
    
    # Generate the embeds file
    generate_embeds_file(args.output, themes, licenses)


if __name__ == "__main__":
    main()
