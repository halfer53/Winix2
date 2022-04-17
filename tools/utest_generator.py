import sys
import re

from soupsieve import match

def get_prototypes(files):
    prototypes = []
    for file in files:
        with open(file) as f:
            for line in f:
                match = re.search(r"\s*void\s+test_(\w+)\s*\(\)", line)
                if match:
                    prototypes.append(f"test_{match.group(1)}")
    return prototypes

def generate(prototypes):
    for proto in prototypes:
        print(f"void {proto}();")
    print()
    print("void run_all_tests(){")
    for proto in prototypes:
        print(f"    {proto}();")
    print("}")

def main():
    if len(sys.argv) < 2:
        exit(1)
    files = sys.argv[1:]
    prototypes = get_prototypes(files)
    generate(prototypes)

if __name__ == '__main__':
    main()