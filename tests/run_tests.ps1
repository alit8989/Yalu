# Yalu Automated Test Suite Runner

$exe = ".\yalu.exe"
if (-not (Test-Path $exe)) {
    if (Test-Path "..\yalu.exe") {
        $exe = "..\yalu.exe"
    } elseif (Test-Path "$PSScriptRoot\..\yalu.exe") {
        $exe = "$PSScriptRoot\..\yalu.exe"
    } else {
        Write-Host "[ERROR] yalu.exe not found! Please build it first." -ForegroundColor Red
        exit 1
    }
}

$passed = 0
$failed = 0
$total = 0

function Run-Positive-Test($name, $file, $input = "") {
    $global:total++
    Write-Host -NoNewline "Running test: $name ... "
    
    if ($input -ne "") {
        $out = $input | & $exe $file 2>&1
    } else {
        $out = & $exe $file 2>&1
    }
    
    if ($LASTEXITCODE -eq 0) {
        $global:passed++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $global:failed++
        Write-Host "FAIL (exit code $LASTEXITCODE)" -ForegroundColor Red
        Write-Host $out
    }
}

function Run-Negative-Test($name, $file, $expectedErrPattern) {
    $global:total++
    Write-Host -NoNewline "Running error test: $name ... "
    
    $out = & $exe $file 2>&1 | Out-String
    
    if ($LASTEXITCODE -ne 0 -and $out -match $expectedErrPattern) {
        $global:passed++
        Write-Host "PASS (expected error caught)" -ForegroundColor Green
    } else {
        $global:failed++
        Write-Host "FAIL (expected error pattern '$expectedErrPattern' not matched or exit code 0)" -ForegroundColor Red
        Write-Host $out
    }
}

function Run-Check-Test($name, $file, $expectSuccess) {
    $global:total++
    Write-Host -NoNewline "Running check test: $name ... "
    
    $out = & $exe check $file 2>&1 | Out-String
    
    if ($expectSuccess -and $LASTEXITCODE -eq 0) {
        $global:passed++
        Write-Host "PASS" -ForegroundColor Green
    } elseif (-not $expectSuccess -and $LASTEXITCODE -ne 0) {
        $global:passed++
        Write-Host "PASS (syntax error properly caught)" -ForegroundColor Green
    } else {
        $global:failed++
        Write-Host "FAIL (exit code $LASTEXITCODE)" -ForegroundColor Red
        Write-Host $out
    }
}

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "       YALU PROGRAMMING LANGUAGE TEST SUITE   " -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

# 1. Feature Tests
Run-Positive-Test "Hello World and Primitives" "tests\test_hello.yl"
Run-Positive-Test "Variable Declarations & Types" "tests\test_variables.yl"
Run-Positive-Test "Integer Arithmetic & Precedence" "tests\test_integers.yl"
Run-Positive-Test "String Operations & Multiline" "tests\test_strings.yl"
Run-Positive-Test "Boolean Logic (Tr/Fl, and/or/not)" "tests\test_booleans.yl"
Run-Positive-Test "String Interpolation (/#var)" "tests\test_interpolation.yl"
Run-Positive-Test "While Loop with Break/Continue" "tests\test_while.yl"
Run-Positive-Test "For Loop with Numeric Ranges" "tests\test_for.yl"
Run-Positive-Test "Functions, Defaults, Recursion" "tests\test_functions.yl"
Run-Positive-Test "Nested Blocks and Scoping" "tests\test_nested_blocks.yl"
Run-Positive-Test "Standard Input (inp.int, inp.str)" "tests\test_input.yl" "123`nHelloYalu"
Run-Positive-Test "Array Operations & Indexing" "tests\test_arrays.yl"

# 2. CLI Validation Checks
Run-Check-Test "Syntax Check on Valid File" "examples\hello.yl" $true
Run-Check-Test "Syntax Check on Invalid File" "tests\test_invalid_syntax.yl" $false

# 3. Error Handling Tests
Run-Negative-Test "Undefined Variable Handling" "tests\test_undefined_var.yl" "Undefined variable"
Run-Negative-Test "Division by Zero Handling" "tests\test_div_zero.yl" "Division by zero"
Run-Negative-Test "Invalid Operand Types Handling" "tests\test_invalid_types.yl" "Invalid operands"

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "Test Results: $passed / $total passed, $failed failed." -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host "=============================================" -ForegroundColor Cyan

if ($failed -gt 0) {
    exit 1
} else {
    exit 0
}

