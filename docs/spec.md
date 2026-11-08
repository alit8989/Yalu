# Yalu Language Formal Specification (v0.3.0)

This document provides the authoritative formal specification for the **Yalu Programming Language** (version 0.3.0).

---

## 1. Notation and Conventions

The grammar is expressed in standard Extended Backus-Naur Form (EBNF):
- `[ ... ]` denotes optional constructs.
- `{ ... }` denotes zero or more repetitions.
- `( ... | ... )` denotes alternatives.
- `,` denotes concatenation.
- `"..."` denotes terminal literal tokens.

---

## 2. Lexical Structure

### 2.1 Character Set & Encoding
Source code is encoded in UTF-8. The runtime automatically detects and ignores any UTF-8 Byte Order Mark (BOM) at the beginning of the file.

### 2.2 Whitespace and Line Terminators
Whitespace consists of spaces (`0x20`), tabs (`0x09`), carriage returns (`0x0D`), and line feeds (`0x0A`). Whitespace serves only to separate tokens and is otherwise ignored, except within string literals.

### 2.3 Comments and Section Markers
Multi-line delimited comments and section markers begin with `<<""` and end with `"">>`.
```ebnf
comment = "<<\"\"" , { any_character_except_else } , "\"\">>" ;
```
Comments can span multiple lines and can contain arbitrary text.

### 2.4 Identifiers
Identifiers designate variables, functions, and properties:
```ebnf
alpha        = "a" | ... | "z" | "A" | ... | "Z" | "_" ;
digit        = "0" | ... | "9" ;
identifier   = alpha , { alpha | digit } ;
```

### 2.5 Literals
```ebnf
integer_lit  = [ "+" | "-" ] , digit , { digit } ;
float_lit    = [ "+" | "-" ] , digit , { digit } , "." , digit , { digit } ,
               [ ( "e" | "E" ) , [ "+" | "-" ] , digit , { digit } ] ;
boolean_lit  = "Tr" | "Fl" ;
null_lit     = "Null" ;
string_lit   = "\"" , { string_char | escape_seq } , "\"" ;
multiline_str= "\"\"" , newline , { any_character } , "\"\"" ;
escape_seq   = "\\n" | "\\t" | "\\r" | "\\\"" | "\\\\" ;
```

### 2.6 Reserved Keywords
```text
class    fn       if       :ei      place    rt       bk       cn
lp       fr       in       use      pr       type     fullclass
inp.str  inp.int  inp.flo  inp.flt  Tr       Fl       Null
and      or       not
```

### 2.7 Operators and Delimiters
```text
::        !!        !!<       >!!       >>!!      >>        >         @
<         <=        <-        >         >=        >-        ==        !=
N!        =         +=        -=        *=        /=        +         -
*         /         %         &&        ||        !         ..        <>
(         )         [         ]         {         }         ,         .
```

---

## 3. Syntactic Grammar

```ebnf
program             = { statement } ;

statement           = use_stmt
                    | class_decl
                    | var_assignment
                    | prop_assignment
                    | class_prop_assign
                    | index_assignment
                    | method_call_stmt
                    | print_stmt
                    | fn_declaration
                    | while_stmt
                    | for_stmt
                    | if_stmt
                    | return_stmt
                    | break_stmt
                    | continue_stmt
                    | block_stmt
                    | expr_stmt ;

use_stmt            = "use" , "[" , module_path , "]" ;
module_path         = [ "./" | "../" ] , identifier , { "/" , identifier } ;

class_decl          = "class" , "/" , identifier , "<" , { fn_declaration } , ">" ;

var_assignment      = identifier , assign_op , expression ;
prop_assignment     = expression , ">" , ( identifier | string_lit ) , assign_op , expression ;
class_prop_assign   = identifier , "." , identifier , assign_op , expression ;
index_assignment    = expression , "[" , expression , "]" , assign_op , expression ;
assign_op           = "=" | "+=" | "-=" | "*=" | "/=" ;

method_call_stmt    = expression , ">>" , identifier , "(" , [ arg_list ] , ")" ;

print_stmt          = "pr" , [ expression , { "," , expression } ] ;

fn_declaration      = "fn" , identifier , "!!<" , [ param_list ] , fn_close_delims , "::" , { statement } , "::" ;
fn_close_delims     = ">!!" | ">>!!" | ">>" | ">" ;
param_list          = param , { "," , param } ;
param               = identifier [ ":" ( type_name | expression ) [ ":(" type_name ")" ] ] ;
type_name           = "str" | "int" | "flt" | "float" | "bool" | "any" ;

while_stmt          = "lp" , "!!" , expression , "!!" , "::" , { statement } , "::" ;

for_stmt            = "fr" , "!!" , identifier , "in" , expression , [ "," , "j" , "=" , expression ] , "!!" , "::" , { statement } , "::" ;

if_stmt             = "if" , "!!" , expression , "!!" , "::" , { statement } ,
                      { ":ei" , "!!" , expression , "!!" , "::" , { statement } } ,
                      [ else_clause ] , "::" ;
else_clause         = "<<\"\"else\"\">>" , { statement } ;

return_stmt         = ( "place" | "rt" ) , [ expression ] ;
break_stmt          = "bk" ;
continue_stmt       = "cn" ;
block_stmt          = "::" , { statement } , "::" ;
expr_stmt           = expression ;
```

---

## 4. Expressions and Operator Precedence

Expressions are evaluated according to the following precedence hierarchy (from highest to lowest):

| Precedence | Level Name | Operators | Associativity |
| :--- | :--- | :--- | :--- |
| 1 (Highest) | Primary | Literals, Variables, Parenthesized, Inputs | — |
| 2 | Postfix | Chained index `[i]`, Property `>prop`, Method call `>>method(...)` | Left-to-right |
| 3 | Unary | `-` (negate), `!` (logical NOT), `not` | Right-to-left |
| 4 | Multiplicative | `*`, `/`, `%` | Left-to-right |
| 5 | Additive & List Concat | `+`, `-`, `<>` (list combination) | Left-to-right |
| 6 | Range | `@` | Left-to-right |
| 7 | Relational | `<`, `<=`, `>`, `>=`, `<-`, `>-` | Left-to-right |
| 8 | Equality | `==`, `!=`, `N!` | Left-to-right |
| 9 | Logical AND | `&&`, `and` | Left-to-right |
| 10 | Logical OR | `\|\|`, `or` | Left-to-right |
| 11 (Lowest) | Mapping | `..` (key-value pair generator) | Left-to-right |

---

## 5. Type System

Yalu is dynamically typed. Every runtime entity has an intrinsic type tag:

1. **`NULL`**: Represents the singleton `Null` value.
2. **`BOOLEAN`**: `Tr` (true) or `Fl` (false).
3. **`INTEGER`**: 64-bit signed two's-complement integer (`int64_t`).
4. **`FLOAT`**: IEEE 754 double-precision floating-point number (`double`).
5. **`STRING`**: Dynamically sized, immutable character sequence encoded in UTF-8.
6. **`LIST` (Array)**: Dynamically sized, indexed heterogeneous collection (`[elem1, elem2, ...]`).
7. **`BOOK`**: Key-value associative mapping preserving insertion order (`{ "key" .. val }`).
8. **`FUNCTION`**: User-defined or native function reference.
9. **`CLASS`**: Class schema and method dictionary.
10. **`INSTANCE`**: Instantiated object with its own property hash table and class binding.

The built-in function `type(...)` returns the uppercase type name string for one or multiple arguments.

---

## 6. Data Structures

### 6.1 Lists
- **Declaration**: `list = [1, "two", 3.0, [4, 5]]`
- **Indexing**: Zero-based. Negative indices wrap from the tail (`-1` is the last element).
- **Methods via `>>`**:
  - `list>>inc(item)`: Appends an item to the list in place.
  - `list>>rem(item)`: Removes the first occurrence of matching item in place.
  - `list>>rep(old, new)`: Replaces occurrences of `old` with `new` in place.
  - `list>>haveit(item)`: Returns boolean `Tr` if item is found, `Fl` otherwise.
- **Concatenation `<>`**: Concatenates two lists into a combined list: `[1, 2] <> [3, 4]`.

### 6.2 Books
- **Declaration**: Key-value mapping separated by `..`:
  ```yl
  book = {
      name .. "ali",
      age .. 18,
      id<>id2 .. 100<>200,
  }
  ```
- **Operator `<>` in Books**: Declares multiple keys mapped to multiple values simultaneously.
- **Property Access**: `book>name` and `book>"name"` access the value of `name`.
- **Reverse Lookup**: `book>value` returns the corresponding key string if the value matches.
- **Collection Accessors**:
  - `book>vals`: Returns an iterable collection of all values.
  - `book>keys`: Returns an iterable collection of all keys.
- **Methods via `>>`**:
  - `book>>add("key" .. value)` or `book>>add("key", value)`: Inserts a new key-value pair.
  - `book>>edval("key", new_value)`: Updates the value of an existing key.
  - `book>>edkey(target, new_key)`: Renames key matching `target` (by key or by value) to `new_key`.

---

## 7. Control Flow

### 7.1 Conditionals (`if`, `:ei`, `<<""else"">>`)
```yl
if !!condition1!! ::
    # Branch 1
:ei !!condition2!! ::
    # Branch 2
<<""else"">>
    # Fallback branch
::
```

### 7.2 While Loop (`lp`)
Executes body repeatedly while the boolean condition evaluates to truthy:
```yl
lp !!condition!! ::
    # Loop body
    if !!done!! :: bk ::   # break
    if !!skip!! :: cn ::   # continue
::
```

### 7.3 For Loop (`fr`)
Supports range loops, list iteration, book iteration, and string iteration:
```yl
# Numeric Range
fr !!i in start@end!! :: ... ::
fr !!i in start@end@step!! :: ... ::

# Collections
fr !!item in list!! :: ... ::
fr !!val in book>vals!! :: ... ::
fr !!key in book>keys!! :: ... ::
fr !!char in "string"!! :: ... ::

# Loop Separator Parameter j
fr !!ch in "hello", j = ","!! ::
    pr ch
::
```
When `j = ","` is specified, `pr` outputs elements separated by commas without intermediate newlines, followed by a newline upon loop termination. When `j = ",.NL"` is specified, each element is followed by a comma and newline.

---

## 8. Functions and Scoping

### 8.1 Function Declaration
Functions are declared using `fn` with parameter list enclosed in `!!< ... >>!!`:
```yl
fn calculate !!<a:int, b:int>>!! ::
    place a * b + 10
::
```

### 8.2 Return Semantics (`place` / `rt`)
`place` immediately halts function execution and returns the evaluated expression to the caller. If no expression is provided, `Null` is returned.

### 8.3 Scoping Rules
- Variables defined at file level reside in the global scope.
- Block statements (`:: ... ::`) introduce a new lexical child scope.
- Variable lookups resolve innermost scope outward.
- Variables reassigned via `=` modify the existing binding in the nearest declaring scope.

---

## 9. Object-Oriented System

- Classes are declared with `class /Name < ... >`.
- Instantiation is performed via `/Name(args)`.
- Constructors are defined with `fn inti !!<yclass, ...>>`.
- Inside the constructor, properties are assigned with `prop.Class = value`.
- After instantiation, properties are read via `inst>prop` and written via `inst>prop = val`.
- `place fullclass` returns the structural representation of the current instance.
- Method `fn yalupr !!<>>!!` defines the custom string representation for printing.
- Circular references among instances are detected and safely suppressed to prevent infinite loops.

---

## 10. Module System (`use`)

Modules are imported using `use [path]`:
- Relative paths (`use [./sub/module]`) resolve relative to the calling source file's directory.
- The `.yl` extension is resolved automatically if omitted.
- Circular imports are tracked and rejected with a fatal error.
- Imported modules execute once and cache their exported environment.
- Exported symbols from the module are merged into the caller's environment.

---

## 11. Error Handling and Diagnostics

The interpreter reports errors with full source diagnostics:
- **`[Yalu Syntax Error]`**: Lexing or grammar validation failures.
- **`[Yalu Type Error]`**: Invalid operand types, incompatible method calls, or type constraint failures.
- **`[Yalu Runtime Error]`**: Undefined variables, division by zero, out-of-bounds indexing, or recursion limits.
- **`[Yalu Module Error]`**: File not found or circular import cycles.
- **`[Yalu Input Error]`**: Invalid format during typed input conversion (`inp.int`, `inp.flo`).

Execution terminates with non-zero exit status upon encountering any unhandled error.
