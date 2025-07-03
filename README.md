# 🎮 LR35902 Emulator

Ein **GameBoy CPU Emulator** geschrieben in C - ein Lernprojekt für hardwarenahe Programmierung.

## 📖 Über das Projekt

Dieses Projekt ist ein kleines **Lernprojekt**, um tiefer in die Welt der hardwarenahen Programmierung und der Programmiersprache C einzutauchen. Das Ziel ist es, den **LR35902 Prozessor** (die CPU des originalen Nintendo Game Boy) zu emulieren und dabei ein besseres Verständnis für:

- 🔧 Hardwarenahe Programmierung
- ⚡ CPU-Architektur und Befehlssätze
- 🎯 Präzise Timing-Emulation
- 📚 C-Programmierung auf niedrigem Level

## 🎯 Projektziel

Die emulierte GameBoy CPU soll später als Grundlage für einen **vollwertigen GameBoy Emulator** dienen. Momentan liegt der Fokus darauf, die Kernfunktionalitäten der LR35902 CPU korrekt zu implementieren.

## 🏗️ Technische Details

- **Sprache**: C
- **Build-System**: CMake
- **Ziel-CPU**: Sharp LR35902 (8-bit CPU basierend auf Z80)

## 🚀 Build & Run

```bash
mkdir build
cd build
cmake ..
make
```

## 📚 Lernziele

- Verständnis von CPU-Zyklen und Instruction Sets
- Implementierung von Registern und Memory Management
- Debugging komplexer Hardware-Emulation
- Saubere C-Code-Architektur

---

*Dies ist ein Lernprojekt - Feedback und Verbesserungsvorschläge sind willkommen!* 🙂
