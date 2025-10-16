# 🎮 LR35902 Emulator
A **GameBoy CPU Emulator** written in C.

## 📖 About the Project
This project is a small **learning project** to dive deeper into the world of low-level programming and the C programming language. The goal is to emulate the **LR35902 processor** (the CPU of the original Nintendo Game Boy) while gaining a better understanding of:
- 🔧 Low-level programming
- ⚡ CPU architecture and instruction sets
- 🎯 Precise timing emulation
- 📚 C programming

## 🎯 Project Goal
The emulated GameBoy CPU will later serve as the foundation for a **full-featured GameBoy emulator**. Currently, the focus is on correctly implementing the core functionalities of the LR35902 CPU.

## 🏗️ Technical Details
- **Language**: C
- **Build System**: CMake
- **Target CPU**: Sharp LR35902 (8-bit CPU based on Z80)
- **Documentation of the instruction set can be found [here](https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html)**

## 🚀 Build & Run
```bash
mkdir build
cd build
cmake ..
make
```

## 📚 Learning Goals
- Understanding CPU cycles and instruction sets
- Implementation of registers and memory management
- Debugging complex hardware emulation
- Clean C code architecture

---
*This is a learning project - feedback and suggestions for improvement are welcome!* 🙂


