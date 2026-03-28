
import subprocess
import os
import sys

def test_ino(ino_file, mock_dir='tests/mock'):
    test_cpp = f"tests/test_{os.path.basename(ino_file)}.cpp"
    test_bin = f"tests/test_{os.path.basename(ino_file)}"

    with open(test_cpp, 'w') as out:
        # Include mocks
        out.write(f'#include "Arduino.h"\n')
        if 'Adafruit_SSD1306' in open(ino_file).read():
            out.write(f'#include "Adafruit_SSD1306.h"\n')
        if 'BlueDisplay' in open(ino_file).read():
            out.write(f'#include "BlueDisplay.h"\n')

        # Read INO and strip includes
        content = open(ino_file).read()
        lines = content.split('\n')

        # Forward declarations for functions defined after setup/loop
        import re
        for line in lines:
            # Match function definitions like "void someFunc() {" or "void ICACHE_RAM_ATTR someFunc() {"
            match = re.search(r'^\s*(\w+)\s+(?:ICACHE_RAM_ATTR\s+)?(\w+)\s*\((.*?)\)\s*\{', line)
            if match:
                return_type = match.group(1)
                func_name = match.group(2)
                args = match.group(3)
                if func_name not in ['setup', 'loop']:
                    out.write(f"{return_type} {func_name}({args});\n")

        for line in lines:
            if line.strip().startswith('#include'):
                continue
            # Remove ICACHE_RAM_ATTR as it might cause issues if not handled by mock
            line = line.replace('ICACHE_RAM_ATTR', '')
            out.write(line + '\n')

        # Append test runner
        out.write(open(f'{mock_dir}/test_runner.cpp').read())

    # Compile
    cmd = ['g++', '-I', mock_dir, test_cpp, '-o', test_bin]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Compilation failed for {ino_file}")
        print(res.stderr)
        return False

    # Run
    res = subprocess.run([test_bin], capture_output=True, text=True)
    print(f"Result for {ino_file}:")
    print(res.stdout[:500] + ("..." if len(res.stdout) > 500 else ""))
    if res.stderr:
        print("Errors:")
        print(res.stderr)

    # Clean up artifacts
    os.remove(test_cpp)
    os.remove(test_bin)

    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_ino.py <ino_file>")
    else:
        test_ino(sys.argv[1])
