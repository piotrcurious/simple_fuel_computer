
# Arduino Fuel Computer Tests

This directory contains a mock Arduino environment to test the `.ino` fuel computer files.

## Files

- `mock/`: Contains mock headers for Arduino, Adafruit_GFX, Adafruit_SSD1306, BlueDisplay, etc.
- `mock/test_runner.cpp`: A main function that simulates time and injector pulses, then calls `setup()` and `loop()`.

## Running Tests

You can use the provided `scripts/test_ino.py` script to test any `.ino` file. It will create a temporary C++ file in the `tests/` directory that includes the mock environment and the `.ino` file content, compiles it, and runs it.

```bash
python3 scripts/test_ino.py your_file.ino
```

The script automatically handles forward declarations and mocking of specific Arduino-isms to allow the `.ino` files to compile as standard C++.
