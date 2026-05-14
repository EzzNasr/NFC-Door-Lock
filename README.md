ESP32 PN532 NFC Master Tester

I built this project to solve a massive headache: getting the PN532 NFC module to reliably talk to an ESP32 without crashing, hanging, or timing out. If you have ever tried to hook these two up and got hit with the infamous "i2cRead returned Error 263" message, this code is exactly what you need.

What it does (The Big Picture)
At its core, this is a diagnostic tool and a rock-solid starting point for NFC projects. When you connect a card reader to a microcontroller brain, they have to speak the same language. Often, because of cheap clone parts or confusing wiring diagrams, they just ignore each other.

This code acts as a bridge. It lets you easily flip between different communication methods until the devices finally connect. Instead of guessing if your board is dead or if your code is wrong, this setup actively tells you what to fix so you can get back to actually scanning RFID tags and cards.

How it works (The Technical Details)
Under the hood, this is a dual-mode PlatformIO project written in C++. It specifically targets the hardware quirks of the ESP32 and those common red PN532 breakout boards.

The main reason I2C fails on these boards (giving you Error 263) is a hardware flaw: they often lack the proper pull-up resistors on the data lines. The ESP32 sends a signal, the voltage drops too low, and it times out waiting for an acknowledgment.

This project bypasses that hardware flaw entirely by giving you a one-line software toggle between standard I2C and High-Speed UART (HSU). If I2C refuses to work, you just comment out a single line of code. The script seamlessly reconfigures the system to use the ESP32 Hardware Serial (RX2/TX2). HSU forces a direct, robust data stream that completely ignores the I2C resistor issues. The code also implements non-blocking retries, ensuring your main loop never hangs indefinitely while polling for a card.

Hardware Configuration Rules
The absolute most critical part of this setup is matching the physical DIP switches on the reader to your code. If both switches are ON, the board enters a reserved state and disables communication entirely.

If you are using I2C Mode: Switch 1 must be ON, and Switch 2 must be OFF. Connect SDA to GPIO 21 and SCL to GPIO 22.

If you are using HSU / UART Mode: Both switches must be OFF. Cross your serial lines by connecting the PN532 TX to ESP32 RX (GPIO 16) and PN532 RX to ESP32 TX (GPIO 17).

Just select your mode at the top of the file, flash the board, and open the serial monitor. The code will output exactly how your physical switches and wires should be configured based on the mode you chose, making troubleshooting essentially foolproof.
