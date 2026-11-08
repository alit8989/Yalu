# Yalu Language Syntax Reference (v0.3.0)

**Yalu** is a distinctive, dynamically typed programming language implemented in native C11. This document serves as the comprehensive syntax guide for writing Yalu programs.

---

## 1. Comments and Section Markers

Multi-line comments and organizational section markers are enclosed in `<<""` and `"">>`.

```yl
<<"" This is a Yalu comment. "">>
<<""
    Comments can span multiple lines
    cleanly without special line markers.
"">>

<<"" Section Markers "">>
<<"" Variables & Setup "">>
x = 10
```

> [!NOTE]
> The conditional fallback marker `<<""else"">>` uses the same delimiter convention but is distinguished syntactically by the parser.

---

## 2. Variables and Primitive Types

Yalu features dynamic typing. Variables do not require type declarations and can be reassigned freely.

### 2.1 Supported Types
- **Integer**: 64-bit signed integer (`int64_t`).
- **Float**: 64-bit IEEE 754 double precision (`double`).
- **Boolean**: First-class literals `Tr` (true) and `Fl` (false).
- **String**: UTF-8 dynamic character sequence.
- **Multiline String**: Spanning across lines enclosed by `""` and `""`.
- **List**: Dynamic indexed array (`[elem1, elem2, ...]`).
- **Book**: Key-value dictionary (`{ key .. value }`).
- **Class / Instance**: Real object-oriented instances.
- **Null**: Singleton `Null`.

### 2.2 Examples
```yl
integer = 42
floating = 3.14159
is_active = Tr
is_finished = Fl
empty_val = Null

message = "Hello, Yalu!"

multiline = ""
This is a multiline string in Yalu.
Line breaks and formatting are preserved natively.
""
```

---

## 3. String Interpolation

Variables and object properties can be interpolated directly into double-quoted strings using `/#identifier` and `/#object>property`:

```yl
user = "Alireza"
score = 98

pr "Player /#user scored /#score points!"

point = /Point("Origin", 0, 0)
pr "Coordinate: /#point>name at (/#point>x, /#point>y)"
```

---

## 4. Input and Output

### 4.1 Printing (`pr`)
`pr` prints expressions separated by commas, automatically appending a trailing newline:

```yl
pr "Hello World"
pr 10, 20, 30
pr "Status:", Tr, 45.67
```

### 4.2 Reading Input (`inp.str`, `inp.int`, `inp.flo`)
Yalu provides typed input expressions with optional prompt messages and automatic format validation:

```yl
name = inp.str "Enter your name: "
age = inp.int "Enter your age: "
ratio = inp.flo "Enter ratio: "

<<"" Without prompts "">>
raw_text = inp.str
number = inp.int
```

If user input does not match the requested type, Yalu reports a formatted `[Yalu Input Error]` and terminates execution safely.

---

## 5. Operators

### 5.1 Arithmetic
```yl
add = 10 + 5       <<"" 15 "">>
sub = 10 - 5       <<"" 5 "">>
mul = 10 * 5       <<"" 50 "">>
div = 10 / 5       <<"" 2 "">>
mod = 10 % 3       <<"" 1 "">>
```

Compound assignments are also supported:
```yl
x = 10
x += 5
x -= 2
x *= 3
x /= 2
```

### 5.2 Comparisons
Yalu supports standard comparisons and native operator tokens:
- `>-` or `>` : Greater than
- `<-` or `<` : Less than
- `>=` : Greater than or equal
- `<=` : Less than or equal
- `==` : Equal
- `N!` or `!=` : Not equal

```yl
pr 10 >- 5         <<"" Tr "">>
pr 3 <- 8          <<"" Tr "">>
pr 5 >= 5          <<"" Tr "">>
pr 4 <= 10         <<"" Tr "">>
pr 5 N! 6          <<"" Tr "">>
pr 7 == 7          <<"" Tr "">>
```

### 5.3 Logical Operators
```yl
pr Tr && Fl        <<"" Fl "">>
pr Tr and Tr       <<"" Tr "">>
pr Tr || Fl        <<"" Tr "">>
pr Fl or Fl        <<"" Fl "">>
pr !Tr             <<"" Fl "">>
pr not Fl          <<"" Tr "">>
```

---

## 6. List Data Structure

Lists are ordered, heterogeneous, zero-indexed collections.

### 6.1 Declaration & Indexing
```yl
items = [1, "two", 3.14, [10, 20]]

pr items[0]        <<"" 1 "">>
pr items[1]        <<"" two "">>
pr items[3][0]     <<"" 10 "">>
pr items[-1]       <<"" [10, 20] (negative indexing supported) "">>

<<"" Nested index assignment "">>
items[3][0] = 999
pr items[3][0]     <<"" 999 "">>
```

### 6.2 Concatenation Operator `<>`
The `<>` operator concatenates two lists into a combined list:
```yl
combined = ["Ali", 3] <> [44, "44"]
pr combined        <<"" ["Ali", 3]<>[44, "44"] "">>
pr combined[2]     <<"" 44 "">>
```

### 6.3 List Methods (`>>`)
Mutating methods are invoked on lists using the `>>` operator:

```yl
numbers = [1, 45, "Ali"]

<<"" inc: append element to the end "">>
numbers>>inc(100)

<<"" haveit: check membership, returns Tr or Fl "">>
pr numbers>>haveit("Ali")     <<"" Tr "">>
pr numbers>>haveit(999)       <<"" Fl "">>

<<"" rep: replace occurrences of old value with new value "">>
numbers>>rep("Ali", "Sara")

<<"" rem: remove matching element "">>
numbers>>rem(45)
```

---

## 7. Book Data Structure (Key-Value Dictionary)

A **Book** is an associative dictionary storing key-value pairs while preserving insertion order.

### 7.1 Declaration Syntax
Keys and values are separated by `..`. Multiple keys and values can be paired simultaneously using `<>`:

```yl
book1 = {
    name .. ali,
    age .. 18,
    id<>id2 .. 1999992<>7,
}

book2 = {
    "name" .. "ali"
    "country" .. "IR"
    "list1" .. ["Ali", 3] <> [44, "44"]
}
```

### 7.2 Property Access & Reverse Lookup
- Access value by key: `book>name` or `book>"name"`
- Reverse lookup by value: `book>ali` returns `"name"`

```yl
pr book1>name      <<"" ali "">>
pr book1>age       <<"" 18 "">>
pr book1>id        <<"" 1999992 "">>
pr book1>id2       <<"" 7 "">>

pr book2>ali       <<"" name (reverse lookup: returns key for value 'ali') "">>
```

### 7.3 Listing Keys and Values
`book>vals` and `book>keys` output all values and keys in the exact standard layout:

```yl
pr book2>vals
<<""
Outputs:
book>vals:
ali
IR
["Ali", 3]<>[44, "44"]
"">>

pr book2>keys
<<""
Outputs:
book>keys:
name
country
list1
"">>
```

### 7.4 Nested Indexing on Books
```yl
pr book2>val[2][0] <<"" Ali "">>
pr book2>key[0][0] <<"" n "">>
```

### 7.5 Book Methods (`>>`)
```yl
book = { "name" .. "ali", "age" .. 20 }

<<"" add: insert new key-value pair "">>
book>>add("ID" .. 233333)
book>>add("city", "Tehran")

<<"" edval: update existing key's value "">>
book>>edval("name", "NAME")

<<"" edkey: rename key without losing its value "">>
book>>edkey("NAME", "fullName")
```

---

## 8. Type Inspection (`type`)

The built-in `type(...)` function inspects one or multiple expressions, returning uppercase type names:

```yl
pr type("hello")                             <<"" STRING "">>
pr type(42)                                  <<"" INTEGER "">>
pr type(3.14)                                <<"" FLOAT "">>
pr type([1, 2, 3])                           <<"" LIST "">>
pr type({ "x" .. 10 })                       <<"" BOOK "">>
pr type(Tr)                                  <<"" BOOLEAN "">>
pr type(Null)                                <<"" NULL "">>

<<"" Multi-argument inspection "">>
pr type("hello", 42, 3.14, Tr)              <<"" STRING INTEGER FLOAT BOOLEAN "">>
```

---

## 9. Control Flow

### 9.1 Conditionals (`if`, `:ei`, `<<""else"">>`)
```yl
x = 25

if !!x > 50!! ::
    pr "Large"
:ei !!x > 20!! ::
    pr "Medium"
<<""else"">>
    pr "Small"
::
```

### 9.2 While Loops (`lp`)
```yl
i = 0
lp !!i < 5!! ::
    pr "i = /#i"
    i += 1
    if !!i == 3!! ::
        pr "Halting loop at 3"
        bk  <<"" Break loop "">>
    ::
::
```

### 9.3 For Loops (`fr`)

#### Numeric Ranges
```yl
<<"" Inclusive range start@end "">>
fr !!i in 1@5!! ::
    pr i
::

<<"" Range with custom step start@end@step "">>
fr !!k in 0@10@2!! ::
    pr k
::

<<"" Range using variables "">>
limit = 4
fr !!i in 1@limit!! ::
    pr i
::
```

#### Iterating Collections & Strings
```yl
fruits = ["apple", "banana", "cherry"]
fr !!f in fruits!! ::
    pr f
::

fr !!val in book>vals!! ::
    pr val
::

fr !!ch in "Yalu"!! ::
    pr ch
::
```

#### Loop Separator Parameter (`j`)
Control output separation when printing within loops:

```yl
<<"" Comma-separated on one line: h,e,l,l,o "">>
fr !!ch in "hello", j = ","!! ::
    pr ch
::

<<"" Comma followed by newline "">>
fr !!ch in "xyz", j = ",.NL"!! ::
    pr ch
::

<<"" Newline after each element (default) "">>
fr !!ch in "abc", j = ".NL"!! ::
    pr ch
::
```

---

## 10. Functions & Return Mechanism (`place`)

### 10.1 Declaration and Calling
Functions are declared using `fn` with parameter list enclosed in `!!< ... >>!!`:

```yl
fn multiply !!<a, b>!! ::
    place a * b
::

result = multiply(6, 7)
pr result  <<"" 42 "">>
```

`place` immediately returns the evaluated value to the caller. Backward compatibility with `rt` is maintained.

### 10.2 Parameters with Type Annotations and Defaults
```yl
fn greet !!<name:user:(str), greeting:"Hello":(str)>>!! ::
    pr "/#greeting, /#name!"
::

greet("Alireza")
greet("Bob", "Good morning")
```

---

## 11. Object-Oriented Programming

Yalu features a native class and object system:

```yl
class /Point <
    fn inti !!<yclass, name:user:(str), age:18:(int), id:int>> ::
        name.Point = name
        age.Point = age
        id.Point = id
    ::

    fn show !!<yclass>>!! ::
        pr "Point /#yclass>name: age /#yclass>age, id /#yclass>id"
    ::

    fn yalupr !!<>>!! ::
        place fullclass
    ::
>

<<"" Instantiation "">>
p = /Point("alpha", 25, 1001)

<<"" Property access & mutation "">>
pr p>name          <<"" alpha "">>
p>age = 26
pr p>age           <<"" 26 "">>

<<"" Calling instance methods "">>
p>show()
p>>show()

<<"" Formatted printing using yalupr and fullclass "">>
pr p
<<""
Outputs:
Point(
    name: "alpha",
    age: 26,
    id: 1001
)
"">>
```

---

## 12. Modules and Imports (`use`)

Separate code into distinct modules using `use [path]`:

```yl
<<"" main.yl "">>
use [./helpers/math]
use [./models/user]

result = calculate_tax(100)
pr result
```

- Paths are relative to the importing file.
- The `.yl` extension is resolved automatically if omitted.
- Circular dependencies are detected and prevented.
- Modules execute once and cache their exported environment.
