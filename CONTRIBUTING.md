# Contributing to Yalu

Thank you for your interest in contributing to the **Yalu Programming Language**! Yalu is an independent, dynamic programming language built from scratch in C11.

---

## 1. Code of Conduct

All contributors and participants are expected to adhere to the [Code of Conduct](CODE_OF_CONDUCT.md).

---

## 2. Project Authorship & Attribution

- **Original Creator & Lead Developer**: Alireza T. (Opuntake)
- **AI-Assisted Development**: Gemini 3.8 Flash
- All contributions submitted to this repository remain under the project's [MIT License](LICENSE).

---

## 3. Development Environment

### Prerequisites

To build and test Yalu on Windows x64, you will need:

1. **C Compiler**: A C11-compliant compiler:
   - **Clang** (recommended via LLVM-MinGW or official LLVM Windows release)
   - **GCC** (via MinGW-w64)
   - **MSVC** (Visual Studio 2019 or later)
2. **Windows Resource Compiler**:
   - `llvm-rc` (included with LLVM) or `windres` (included with MinGW)
3. **PowerShell 5.1+ or PowerShell 7+** (for running automated test suites)
4. *(Optional)* **CMake 3.15+** or **GNU Make**

---

## 4. Building Yalu from Source

### Using `build.bat` (Recommended on Windows)

Open a Command Prompt or PowerShell terminal in the repository root:

```cmd
build.bat
```

This script will:
1. Detect your available C compiler (`clang` or `gcc`).
2. Compile the Windows resource file (`src/yalu.rc`) containing the official multi-resolution icon (`yalu.ico`).
3. Compile the Release executable (`yalu.exe`).
4. Compile the Debug executable (`yalu_debug.exe`).
5. Run the complete automated test suite (`tests/run_all.ps1`).

### Using Make

```bash
make release
make debug
make test
```

### Using CMake

```bash
cmake -B build -S .
cmake --build build --config Release
```

---

## 5. Running the Test Suite

Yalu features an automated test runner located in `tests/run_all.ps1`. Before submitting any changes, ensure all tests pass:

```powershell
powershell -ExecutionPolicy Bypass -File tests/run_all.ps1
```

If you add a new language feature or fix a bug, please add an accompanying test file in `tests/` and register it in `tests/run_all.ps1`.

---

## 6. Coding Standards & Conventions

1. **Standard C11**: The core runtime and interpreter must be strictly written in ANSI C11 without non-standard vendor extensions.
2. **Zero External Dependencies**: The interpreter must depend only on the standard C library and standard Win32 console APIs for icon handling. Do not introduce third-party libraries.
3. **Memory Management**:
   - Always pair allocations (`yalu_malloc`, `yalu_strdup`) with clean deallocations (`yalu_free`).
   - Use the reference-counting functions (`value_retain`, `value_release`) when managing composite `Value` instances.
4. **Syntax Integrity**: Never alter existing Yalu keywords, delimiters (`<<""`, `"">>`, `::`, `!!`, `fr`, `lp`, `pr`, etc.) or break backward compatibility with existing `.yl` programs.
5. **Clear Error Reporting**: All errors should be emitted through the centralized diagnostic system (`yalu_report_error` in `src/errors.h`), including exact source position (filename, line, column).

---

## 7. Submitting Changes

1. **Fork the Repository**: Create a fork of the project on GitHub.
2. **Create a Feature Branch**:
   ```bash
   git checkout -b feature/my-feature-name
   ```
3. **Write Tests**: Verify that existing tests pass and your new tests confirm the intended behavior.
4. **Commit with Clear Messages**: Write descriptive commit messages explaining *what* was changed and *why*.
5. **Open a Pull Request**: Submit your pull request against the `main` branch. Provide a concise summary of the changes and link any related issues.

