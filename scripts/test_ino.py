
import subprocess
import os
import sys
import re

def test_ino(ino_file, mock_dir='tests/mock'):
    test_cpp = f"tests/test_{os.path.basename(ino_file)}.cpp"
    test_bin = f"tests/test_{os.path.basename(ino_file)}"

    with open(ino_file, 'r') as f:
        ino_content = f.read()

    has_ssd1306 = 'Adafruit_SSD1306 display' in ino_content
    has_bluedisplay = 'BlueDisplay myDisplay' in ino_content

    with open(test_cpp, 'w') as out:
        # Include mocks
        out.write(f'#include "Arduino.h"\n')
        if 'Adafruit_SSD1306' in ino_content:
            out.write(f'#include "Adafruit_SSD1306.h"\n')
        if 'BlueDisplay' in ino_content:
            out.write(f'#include "BlueDisplay.h"\n')

        # Forward declarations for functions defined after setup/loop
        lines = ino_content.split('\n')
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
    if has_ssd1306:
        cmd.append('-DHAS_SSD1306')
    elif has_bluedisplay:
        cmd.append('-DHAS_BLUEDISPLAY')

    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Compilation failed for {ino_file}")
        print(res.stderr)
        return False

    # Run
    res = subprocess.run([test_bin], capture_output=True, text=True)
    print(f"Result for {ino_file}:")
    print(res.stdout[:500] + ("..." if len(res.stdout) > 500 else ""))

    # Process captures
    capture_dir = f"docs/images/{os.path.basename(ino_file)}"
    os.makedirs(capture_dir, exist_ok=True)
    found_captures = False
    for f in os.listdir('.'):
        if f.startswith('output_') and f.endswith('.pbm'):
            png_file = f.replace('.pbm', '.png')
            subprocess.run(['python3', 'scripts/dump_to_png.py', f, f"{capture_dir}/{png_file}"])
            os.remove(f)
            found_captures = True

    if not found_captures:
        print(f"No captures generated for {ino_file}")

    # Clean up artifacts
    os.remove(test_cpp)
    os.remove(test_bin)

    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python test_ino.py <ino_file>")
    else:
        test_ino(sys.argv[1])
