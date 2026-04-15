[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](http://creativecommons.org/licenses/by-nc-sa/4.0/)
<!--ANYONE EDITING THE README, LISTEN UP! THE ABOVE BADGE OR ANY LICENSE TERMS MUST NOT BE REMOVED!-->
<small>If anyone is found to break these license terms, legal action may be taken</small>
---

# Iris_Lite Camera System

**STEAM IC 2026 Computer Science Project**

Project Programming by Abhiram Vadali & Shreyash Thakur
Lab Report by Abhiram V, Shreyash T, Subeg G, and Atharv R.

---

## About

Iris_Lite is a lightweight camera system built on the Raspberry Pi 4B. It features a custom compression pipeline designed to efficiently capture and store video clips with minimal resource usage.

---

## Features

- Camera capture on Raspberry Pi 4B
- Custom compression algorithm (C++)
- Clip storage and management
- Simple shell-based launcher (`IRIS_Lite.sh`)

---

## Project Structure

```
Iris-Lite/
├── Compression/       # Core compression algorithm (C++)
├── clips/             # Stored video/image clips
├── IRIS_Lite.sh       # Main run script
└── README.md
```

---

## Languages

| Language | Usage |
|----------|-------|
| C++      | 65.4% |
| Python   | 30.3% |
| CMake    | 2.2%  |
| Shell    | 2.1%  |

---

## Getting Started

1. Clone the repository:
   ```bash
   git clone https://github.com/AbhiramV010/Iris-Lite.git
   cd Iris-Lite
   ```

2. Run the launcher script:
   ```bash
   bash IRIS_Lite.sh
   ```

---

## License

This project is licensed under [CC BY-NC-SA 4.0](http://creativecommons.org/licenses/by-nc-sa/4.0/).

---

## Acknowledgements

Thanks to Macmeet B for lending us a Raspberry Pi 4B, it truly allowed the project to function.
Thanks to our sophomore science teacher (Mr. Hadley) for supporting us.
