import sys
import base64

def usage():
    print("Usage: python make_polyglot.py <template> <main_exe> <interpreter> <output_script>")
    sys.exit(1)

def b64_of_file(path):
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("ascii")

def pad_replace(template, placeholder, value, width):
    # value must be int
    value_str = str(value)
    # Pad with trailing spaces
    if len(value_str) < width:
        value_str = value_str + (" " * (width - len(value_str)))
    elif len(value_str) > width:
        raise ValueError(f"Value {value} is too long for placeholder {placeholder} (max {width} characters)")
    # Replace all occurrences
    return template.replace(placeholder, value_str)

def main():
    if len(sys.argv) != 5:
        usage()
    template_path = sys.argv[1]
    main_exe_path = sys.argv[2]
    interp_path = sys.argv[3]
    output_path = sys.argv[4]

    # Read template
    with open(template_path, "r", encoding="utf-8") as f:
        template = f.read()

    # Base64 encode binaries
    main_b64 = b64_of_file(main_exe_path)
    interp_b64 = b64_of_file(interp_path)

    # Prepare output: template + blobs
    sep1 = "\n__BLOB_INTERP__\n"
    sep2 = "\n__BLOB_MAIN__\n"
    script_body = template + sep1 + interp_b64 + sep2 + main_b64

    # Calculate offsets (byte offsets)
    offset_interp = len(template.encode("utf-8")) + len(sep1.encode("utf-8"))
    offset_main = offset_interp + len(interp_b64.encode("utf-8")) + len(sep2.encode("utf-8"))
    len_interp = len(interp_b64.encode("utf-8"))

    # Replace placeholders with zero-padded numbers
    replacements = [
        ("{OFFSET_INTERP}", offset_interp),
        ("{LEN_INTERP}", len_interp),
        ("{OFFSET_MAIN}", offset_main),
    ]
    for placeholder, value in replacements:
        script_body = pad_replace(script_body, placeholder, value, len(placeholder))

    # Write output
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(script_body)

    print(f"Installer script written to: {output_path}")
    print(f"Interpreter blob offset: {offset_interp} (length {len_interp})")
    print(f"Main binary offset: {offset_main}")

if __name__ == "__main__":
    main()