import re
import uuid
import sys

def hex_byte(c):
    return int(c, 16)

def calc_domain_id(s):
    x = ((hex_byte(s[0]) << 0) | (hex_byte(s[1]) << 4) | (hex_byte(s[2]) << 8) | (hex_byte(s[3]) << 12) |
         (hex_byte(s[4]) << 16) | (hex_byte(s[5]) << 20) | (hex_byte(s[6]) << 24) | (hex_byte(s[7]) << 28) |
         (hex_byte(s[9]) << 32) | (hex_byte(s[10]) << 36) | (hex_byte(s[11]) << 40) | (hex_byte(s[12]) << 44) |
         (hex_byte(s[14]) << 48) | (hex_byte(s[15]) << 52) | (hex_byte(s[16]) << 56) | (hex_byte(s[17]) << 60))

    y = ((hex_byte(s[19]) << 0) | (hex_byte(s[20]) << 4) | (hex_byte(s[21]) << 8) | (hex_byte(s[22]) << 12) |
         (hex_byte(s[24]) << 16) | (hex_byte(s[25]) << 20) | (hex_byte(s[26]) << 24) | (hex_byte(s[27]) << 28) |
         (hex_byte(s[28]) << 32) | (hex_byte(s[29]) << 36) | (hex_byte(s[30]) << 40) | (hex_byte(s[31]) << 44) |
         (hex_byte(s[32]) << 48) | (hex_byte(s[33]) << 52) | (hex_byte(s[34]) << 56) | (hex_byte(s[35]) << 60))

    return x ^ y

def generate_spec(path):
    with open(path, 'r') as file:
        cpp_code = file.read()

    enum_regex = re.compile(r'enum class (\w+Errc)\s*(:\s*[a-z0-9_:]+)?\s*{\s*([^}]+)};', re.MULTILINE)

    matches = enum_regex.findall(cpp_code)

    print(matches)

    output = 'SYSTEM_ERROR2_NAMESPACE_BEGIN'
    for match in matches:
        enum_name = match[0]
        enum_values = [value.strip() for value in match[2].split(',')]

        domain_uuid = uuid.uuid4()
        domain_uuid64 = calc_domain_id(str(domain_uuid))

        payload_uuid = uuid.uuid4()
        payload_uuid64 = calc_domain_id(str(payload_uuid))
        indirect_uuid64 = 0xc44f7bdeb2cc50e9 ^ payload_uuid64

        spec = f"""
template <>
struct quick_status_code_from_enum<{enum_name}>
    : quick_status_code_from_enum_defaults<{enum_name}> {{
  static constexpr auto domain_name = "{enum_name}";
  // {hex(domain_uuid64)}
  static constexpr auto domain_uuid = "{domain_uuid}";
  // {hex(payload_uuid64)} ^ 0xc44f7bdeb2cc50e9 = {hex(indirect_uuid64)}
  static constexpr auto payload_uuid = "{payload_uuid}";
  static constexpr bool all_failures = true;
  static const std::initializer_list<mapping>& value_mappings() {{
    // NOLINTBEGIN
    static const std::initializer_list<mapping> v = {{
"""
        for value in enum_values:
            if value:
                spec += f'      {{{enum_name}::{value}, "{value}", {{}}}},\n'

        spec += """    };
    // NOLINTEND
    return v;
  }
};"""
        output += spec
    output += '\nSYSTEM_ERROR2_NAMESPACE_END'
    print(output)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python quick_status_code.py <path_to_cpp_file>")
        sys.exit(1)

    generate_spec(sys.argv[1])
