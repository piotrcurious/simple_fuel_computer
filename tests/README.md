
# Arduino Fuel Computer Tests

This directory contains a mock Arduino environment to test the `.ino` fuel computer files.

## Files

- `mock/`: Contains mock headers for Arduino, Adafruit_GFX, Adafruit_SSD1306, BlueDisplay, etc.
- `mock/test_runner.cpp`: A main function that simulates time and injector pulses, then calls `setup()` and `loop()`.

## Running Tests

You can use the provided `test_ino.py` script to test any `.ino` file. It will create a temporary C++ file that includes the mock environment and the `.ino` file content, compiles it, and runs it.

```bash
python3 /home/jules/self_created_tools/test_ino.py your_file.ino
```

(Note: The `test_ino.py` is currently in the agent's home directory for convenience during development, but its logic can be replicated or moved to a permanent location in the repository.)
