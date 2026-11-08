$exe = ".\yalu.exe"
if (-not (Test-Path $exe)) {
    if (Test-Path "..\yalu.exe") {
        $exe = "..\yalu.exe"
    } elseif (Test-Path "$PSScriptRoot\..\yalu.exe") {
        $exe = "$PSScriptRoot\..\yalu.exe"
    } else {
        Write-Host "[ERROR] yalu.exe not found!" -ForegroundColor Red
        exit 1
    }
}

$total = 0
$passed = 0
$failed = 0

function Run-Pos($name, $file) {
    $global:total++
    Write-Host -NoNewline "Test: $name ... "
    $proc = Start-Process -FilePath $exe -ArgumentList $file -NoNewWindow -PassThru -Wait
    if ($proc.ExitCode -eq 0) {
        $global:passed++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $global:failed++
        Write-Host "FAIL ($($proc.ExitCode))" -ForegroundColor Red
    }
}

function Run-Neg($name, $file) {
    $global:total++
    Write-Host -NoNewline "Error Test: $name ... "
    $proc = Start-Process -FilePath $exe -ArgumentList $file -NoNewWindow -PassThru -Wait
    if ($proc.ExitCode -ne 0) {
        $global:passed++
        Write-Host "PASS (expected failure caught)" -ForegroundColor Green
    } else {
        $global:failed++
        Write-Host "FAIL (expected error, but returned 0)" -ForegroundColor Red
    }
}

function Run-Chk($name, $file, $expectSuccess) {
    $global:total++
    Write-Host -NoNewline "Check Test: $name ... "
    $proc = Start-Process -FilePath $exe -ArgumentList "check", $file -NoNewWindow -PassThru -Wait
    if (($expectSuccess -and $proc.ExitCode -eq 0) -or (-not $expectSuccess -and $proc.ExitCode -ne 0)) {
        $global:passed++
        Write-Host "PASS" -ForegroundColor Green
    } else {
        $global:failed++
        Write-Host "FAIL ($($proc.ExitCode))" -ForegroundColor Red
    }
}

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "         YALU AUTOMATED TEST SUITE            " -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan

Run-Pos "Hello World" "tests/test_hello.yl"
Run-Pos "Variable Declarations" "tests/test_variables.yl"
Run-Pos "Integer Arithmetic" "tests/test_integers.yl"
Run-Pos "String Operations" "tests/test_strings.yl"
Run-Pos "Boolean Logic" "tests/test_booleans.yl"
Run-Pos "String Interpolation" "tests/test_interpolation.yl"
Run-Pos "While Loops" "tests/test_while.yl"
Run-Pos "For Loops" "tests/test_for.yl"
Run-Pos "Functions & Recursion" "tests/test_functions.yl"
Run-Pos "Nested Blocks & Scope" "tests/test_nested_blocks.yl"
Run-Pos "Arrays & Collections" "tests/test_arrays.yl"

# v0.2 Extensions
Run-Pos "v0.2 Place Return Statement" "tests/test_place.yl"
Run-Pos "v0.2 Conditionals (:ei & else)" "tests/test_conditionals_v2.yl"
Run-Pos "v0.2 Class System & Methods" "tests/test_classes.yl"
Run-Pos "v0.2 Modules (use)" "tests/test_modules.yl"
Run-Pos "v0.2 Recursion Protection" "tests/test_recursion_protection.yl"
Run-Pos "v0.2 Full Integration" "examples/v02_integration.yl"

# v0.3 Extensions (Lists, Books, Methods, Iteration, Operators)
Run-Pos "v0.3 Lists, Books, Methods & Loops" "tests/test_v03_features.yl"

Run-Chk "Check Valid Syntax" "examples/hello.yl" $true
Run-Chk "Check Valid v0.2 Integration" "examples/v02_integration.yl" $true
Run-Chk "Check Invalid Syntax" "tests/test_invalid_syntax.yl" $false

Run-Neg "Undefined Variable" "tests/test_undefined_var.yl"
Run-Neg "Division By Zero" "tests/test_div_zero.yl"
Run-Neg "Invalid Operand Types" "tests/test_invalid_types.yl"
Run-Neg "v0.2 Circular Import" "tests/test_circular_import.yl"
Run-Neg "v0.2 Constructor Type Error" "tests/test_type_error.yl"

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "Results: $passed / $total passed, $failed failed." -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host "==============================================" -ForegroundColor Cyan

if ($failed -gt 0) { exit 1 } else { exit 0 }

