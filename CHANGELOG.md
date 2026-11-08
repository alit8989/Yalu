# Changelog

All notable changes to the **Yalu Programming Language** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.3.0] - 2026-11-08

### Added
- **Official Windows Application Icon**: Converted and embedded official Yalu logo as a multi-resolution PE resource icon (`16x16`, `32x32`, `48x48`, `64x64`, `128x128`, and `256x256`) in `yalu.exe` and `yalu_debug.exe`.
- **Book Data Structure**: Key-value collection (`{ "name" .. "ali", "age" .. 18 }`) with insertion order preservation.
  - Multi-key/value declaration operator `<>` (`id<>id2 .. 1999992<>7`).
  - Direct key access (`book>name`, `book>"name"`).
  - Reverse lookup by value (`book>ali` returns `"name"`).
  - Key and value list accessors (`book>keys`, `book>vals`).
  - Book methods via `>>`: `book>>add("ID" .. 233333)`, `book>>edval("name", "NAME")`, `book>>edkey("ali", "Sara")`.
- **Enhanced List Capabilities**:
  - List concatenation operator `<>` (`[1, 2] <> [3, 4]`).
  - Mutating methods via `>>`: `list>>inc(val)`, `list>>rem(val)`, `list>>rep(old, new)`, and `list>>haveit(val)`.
  - Nested index assignment (`list[2][0] = 999`).
- **Enhanced For Loops**:
  - Range loops with variable evaluation (`fr !!i in 1@max!!`).
  - Collection iteration (`fr !!item in list!!`, `fr !!v in book>vals!!`).
  - String character iteration (`fr !!ch in "string"!!`).
  - Loop separator parameter `j` (`j = ","`, `j = ",.NL"`, `j = ".NL"`).
- **Native Comparison Operators**:
  - `>-` (greater than)
  - `<-` (less than)
  - `N!` (not equal)
  - Existing: `>=`, `<=`, `==`, `!=`
- **Standard Library / Built-ins**:
  - `inp.flo` float input alias with input format validation.
  - `type(...)` type inspection supporting single and multi-argument queries returning uppercase type names (`STRING`, `INTEGER`, `FLOAT`, `LIST`, `BOOK`, `BOOLEAN`, `NULL`).
- **Test Automation**:
  - Comprehensive automated test suite expanded to 26 suites verifying all new features and backward compatibility.

### Changed
- Refactored `inp.str`, `inp.int`, and `inp.flo` to print custom prompts directly without unwanted trailing whitespace.
- Updated Windows console window initialization to set `WM_SETICON` dynamically from embedded resource.

---

## [0.2.0] - 2026-11-06

### Added
- **Module System**:
  - `use [./module]` syntax supporting relative paths and `.yl` extension auto-resolution.
  - Circular import detection and prevention using internal module import stacks.
  - Module symbol export into the importing scope.
  - Single-execution caching.
- **Class and Object System**:
  - `class /ClassName < ... >` syntax.
  - Constructor definition `fn inti !!<yclass, ...>>`.
  - Implicit instance binding `yclass`.
  - Class property assignment `property.ClassName = val`.
  - Property access via `object>property`.
  - Special keyword `place fullclass` for structured object serialization.
  - Custom representation method `fn yalupr !!<>>!!` for user-defined object printing.
  - Circular reference detection to prevent infinite recursion during object formatting.
- **Enhanced Control Flow**:
  - Native `place` keyword for early returns.
  - Multi-branch conditionals with `:ei` (else-if) and `<<""else"">>` markers.
- **String Interpolation**:
  - Inline variable interpolation `/#identifier`.
  - Inline property interpolation `/#object>property`.
- **Static Syntax Validation**:
  - `yalu.exe check <file.yl>` CLI subcommand for parsing and validating files without running them.

---

## [0.1.0] - 2026-10-31

### Added
- Initial public implementation of the Yalu programming language in C11.
- Custom handwritten lexer, recursive descent parser, and tree-walking interpreter.
- Core data types: 64-bit integers, 64-bit IEEE 754 floats, booleans (`Tr`, `Fl`), strings, multiline strings (`""\n...""`), arrays, and `Null`.
- Control flow: `lp !!condition!! :: ... ::` (while loops) with `bk` (break) and `cn` (continue).
- Range-based for loops: `fr !!i in 1@100!! :: ... ::` and `fr !!i in 0@10@2!! :: ... ::`.
- Basic functions: `fn name !!<params>>!! :: ... ::` with `rt` return statement.
- Built-in I/O: `pr` statement and `inp.int`, `inp.str`, `inp.flt` expressions.
- Interactive Read-Eval-Print Loop (REPL).
- Native Windows build script (`build.bat`) and Makefile.

