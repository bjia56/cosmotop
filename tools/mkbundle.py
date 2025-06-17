import sys
import os
import argparse
import base64
import gzip

HEADER_SCRIPT = r"""#!/bin/sh
set -e
OUTDIR="$HOME/.cosmotop"
SCRIPT="$0"
mkdir -p "$OUTDIR"
b64() {{
if command -v base64 >/dev/null 2>&1; then
base64 -d
else
python -c 'import sys,base64;sys.stdout.write(base64.decodestring(sys.stdin.read()))'
fi
}}
needs_extract() {{
for f in "$@"; do
TARGET="$OUTDIR/$(basename "$f")"
if [ ! -f "$TARGET" ] || [ "$SCRIPT" -nt "$TARGET" ]; then
return 0
fi
done
return 1
}}
if needs_extract "{files_list}"; then
{extract_section}
fi
exec "$OUTDIR/{first_file_name}" "$@"
"""

EXTRACT_FILE_SNIPPET = r"""
tail -n +{start_line} "$SCRIPT" | head -n {num_lines} | b64 | gzip -d > "$OUTDIR/{filename}"
chmod +x "$OUTDIR/{filename}"
""".strip()

def encode_file_as_base64_gz(filepath):
    with open(filepath, "rb") as f:
        gzipped = gzip.compress(f.read())
        b64 = base64.b64encode(gzipped).decode('utf-8')
    return b64

def main():
    parser = argparse.ArgumentParser(description="Generate a self-extracting shell script from input files.")
    parser.add_argument('input_files', nargs='+', help='Input files to embed')
    parser.add_argument('-o', '--output', required=True, help='Output shell script file')
    args = parser.parse_args()

    input_files = args.input_files
    output_path = args.output

    # Prepare extraction script and encoded content
    extract_section = ""
    file_blobs = []
    header_lines = 0
    # We'll count the header lines after formatting

    file_positions = []

    # Files we are operating on
    files_list = " ".join([f"{os.path.basename(f)}" for f in input_files])
    first_file_name = os.path.basename(input_files[0])

    # Encode files and precompute lines
    b64_blobs = []
    for path in input_files:
        b64 = encode_file_as_base64_gz(path)
        b64_lines = b64.splitlines()
        b64_blobs.append(b64_lines)

    # We need to know header line count to set the correct start line in the extract script
    # So generate the real extract section after header is formatted, but do a fake one here
    extract_section_tmp = ""
    start_line = 1000 # arbitrary fake value
    for i, path in enumerate(input_files):
        filename = os.path.basename(path)
        num_lines = len(b64_blobs[i])
        if extract_section_tmp:
            extract_section_tmp += "\n"
        extract_section_tmp += EXTRACT_FILE_SNIPPET.format(
            filename=filename,
            start_line=start_line,
            num_lines=num_lines
        )
        start_line += num_lines

    # Get expected header length
    header_script_tmp = HEADER_SCRIPT.format(
        files_list=files_list,
        extract_section=extract_section_tmp,
        first_file_name=first_file_name
    )
    header_lines = len(header_script_tmp.splitlines())

    # Now generate the extract section with correct line numbers
    extract_section = ""
    start_line = header_lines + 1
    for i, path in enumerate(input_files):
        filename = os.path.basename(path)
        num_lines = len(b64_blobs[i])
        if extract_section:
            extract_section += "\n"
        extract_section += EXTRACT_FILE_SNIPPET.format(
            filename=filename,
            start_line=start_line,
            num_lines=num_lines
        )
        start_line += num_lines

    # Build the final header
    header_script = HEADER_SCRIPT.format(
        files_list=files_list,
        extract_section=extract_section.rstrip(),
        first_file_name=first_file_name
    )

    # Now write to output file
    with open(output_path, "w") as fout:
        # Write the header
        fout.write(header_script)
        # Write the encoded blobs
        for blob in b64_blobs:
            for line in blob:
                fout.write(line)
                fout.write('\n')

    # Make output file executable
    os.chmod(output_path, 0o755)

if __name__ == "__main__":
    main()
