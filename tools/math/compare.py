"""Check what a math tool generated against what is in the tree.

    python3 compare.py table GENERATED SOURCE ARRAY...
        Each named C array in SOURCE must appear, value for value and in order, somewhere in
        GENERATED (a table generator's output, comments and layout ignored).

    python3 compare.py golden GENERATED TEST...
        The rows GENERATED (a golden generator's output) prints must be exactly the rows of the
        golden arrays (those whose name holds "golden") in the TEST files, in any order.

Values are compared, not text, so 0x1.08p-4 matches 0x1.0800000000000p-4, a float literal is
rounded to float, and integer suffixes are ignored. Exits 1 on any difference.
"""

import re
import struct
import sys
from collections import Counter

NUMBER = re.compile(r"(?<![\w.])[-+]?(?:0x[0-9a-fA-F.]+(?:p[-+]?\d+)?|\d+\.?\d*(?:e[-+]?\d+)?)[uUlLfF]*(?![\w.])")


def strip_comments(text):
    """The text with its // comments removed."""
    return "\n".join(line.split("//")[0] for line in text.splitlines())


def to_f32(value):
    """value rounded to the nearest float."""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def value(token):
    """A numeric literal as a comparable value: ('i', int) or ('f', float)."""
    is_hex = token.lower().lstrip("-+").startswith("0x")
    body = token.rstrip("uUlL") if is_hex else token.rstrip("uUlLfF")
    if is_hex and "." not in body and "p" not in body.lower():
        return ("i", int(body, 16))

    if is_hex and body.lower().endswith("f") and "p" in body.lower():
        return ("f", to_f32(float.fromhex(body[:-1])))

    if is_hex:
        return ("f", float.fromhex(body))

    if token[-1] in "fF":
        return ("f", to_f32(float(body)))

    if "." in body or "e" in body.lower():
        return ("f", float(body))

    return ("i", int(body))


def numbers(text):
    """Every numeric literal in text, comments ignored, as values."""
    return [value(t) for t in NUMBER.findall(strip_comments(text))]


def array_body(source, name):
    """The text between the braces of the C array called name."""
    match = re.search(r"\b" + re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{", source)
    if not match:
        sys.exit(f"compare.py: no array {name}")
    end = source.index("\n};", match.end())
    return source[match.end():end]


def contains(haystack, needle):
    """Whether needle appears as a contiguous run of haystack."""
    for start in range(len(haystack) - len(needle) + 1):
        if haystack[start:start + len(needle)] == needle:
            return True
    return False


def compare_tables(generated_path, source_path, names):
    """Checks each named array of the source appears in the generated output; the count that don't."""
    generated = numbers(open(generated_path).read())
    source = strip_comments(open(source_path).read())
    bad = 0
    for name in names:
        # Drop the bracketed sizes, which are not table values
        wanted = numbers(re.sub(r"\[[^\]]*\]", "", array_body(source, name)))
        if contains(generated, wanted):
            print(f"{name}: {len(wanted)} values match {source_path}")
        else:
            print(f"{name}: DIFFERS from {source_path}")
            bad += 1
    return bad


def rows(lines):
    """The value tuples of the lines that are braced rows."""
    found = []
    for line in lines:
        text = line.split("//")[0].strip()
        if text.startswith("{") and "}" in text:
            found.append(tuple(numbers(text[1:text.index("}")])))
    return found


def golden_rows(test_path):
    """The rows of every golden array in a test file."""
    found = []
    text = open(test_path).read()
    for match in re.finditer(r"^static const \w+ \w*golden\w*\[\][^=]*= \{\n(.*?)^\};", text, re.M | re.S):
        found += rows(match.group(1).splitlines())
    return found


def compare_golden(generated_path, test_paths):
    """Checks the generated rows are the tests' golden rows; 1 if not."""
    generated = Counter(rows(open(generated_path).read().splitlines()))
    tests = Counter()
    for path in test_paths:
        tests.update(golden_rows(path))
    missing = generated - tests
    extra = tests - generated
    for row in missing:
        print("generated but not in the tests:", row)
    for row in extra:
        print("in the tests but not generated:", row)
    if missing or extra:
        print(f"golden rows DIFFER: {sum(missing.values())} generated rows not in the tests, "
              f"{sum(extra.values())} test rows not generated")
        return 1
    print(f"all {sum(generated.values())} golden rows match {', '.join(test_paths)}")
    return 0


def main():
    if len(sys.argv) < 4 or sys.argv[1] not in ("table", "golden"):
        sys.exit(__doc__)
    if sys.argv[1] == "table":
        bad = compare_tables(sys.argv[2], sys.argv[3], sys.argv[4:])
    else:
        bad = compare_golden(sys.argv[2], sys.argv[3:])
    sys.exit(1 if bad else 0)


main()
