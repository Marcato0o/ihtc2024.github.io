param(
    [string]$SolverPath = "",
    [string]$InputDir = "",
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")

if ([string]::IsNullOrWhiteSpace($InputDir)) {
    $InputDir = Join-Path $repoRoot "assets/files/test"
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputDir = Join-Path $scriptDir ("test_solutions_" + $timestamp)
}

if ([string]::IsNullOrWhiteSpace($SolverPath)) {
    $candidates = @(
        (Join-Path $scriptDir "IHTC_Test.exe"),
        (Join-Path $scriptDir "IHTC_Test")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $SolverPath = $candidate
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($SolverPath) -or -not (Test-Path $SolverPath)) {
    throw "Solver executable not found. Build it first or pass -SolverPath explicitly."
}

if (-not (Test-Path $InputDir)) {
    throw "Input directory not found: $InputDir"
}

$tests = Get-ChildItem -Path $InputDir -File -Filter "test*.json" | Sort-Object Name
if ($tests.Count -eq 0) {
    throw "No test instances found in: $InputDir"
}

New-Item -Path $OutputDir -ItemType Directory -Force | Out-Null

$solverResolved = (Resolve-Path $SolverPath).Path
$inputResolved = (Resolve-Path $InputDir).Path
$outputResolved = (Resolve-Path $OutputDir).Path

Write-Host "Solver:      $solverResolved"
Write-Host "Input dir:   $inputResolved"
Write-Host "Output dir:  $outputResolved"
Write-Host "Test count:  $($tests.Count)"

$oldLocation = Get-Location
$passed = 0
$failed = 0
$failedTests = @()

try {
    Set-Location $scriptDir

    foreach ($test in $tests) {
        $instanceName = [System.IO.Path]::GetFileNameWithoutExtension($test.Name)
        $tempSolution = Join-Path $scriptDir "solution.json"

        if (Test-Path $tempSolution) {
            Remove-Item $tempSolution -Force
        }

        Write-Host "[RUN] $($test.Name)"
        & $solverResolved $test.FullName

        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Solver failed on $($test.Name) with exit code $LASTEXITCODE"
            $failed++
            $failedTests += $test.Name
            continue
        }

        if (-not (Test-Path $tempSolution)) {
            Write-Warning "Missing solution.json after running $($test.Name)"
            $failed++
            $failedTests += $test.Name
            continue
        }

        $targetSolution = Join-Path $outputResolved ("sol_" + $instanceName + ".json")
        Move-Item -Path $tempSolution -Destination $targetSolution -Force
        $passed++
    }
}
finally {
    Set-Location $oldLocation
}

Write-Host ""
Write-Host "Done. Passed: $passed, Failed: $failed"
Write-Host "Solutions written to: $outputResolved"

if ($failed -gt 0) {
    Write-Host "Failed tests: $($failedTests -join ', ')"
    exit 1
}

exit 0
