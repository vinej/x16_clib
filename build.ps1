<#
.SYNOPSIS
    Build the x16clib library with cc65, then compile and optionally run
    a C program against it.

.EXAMPLE
    .\build.ps1                                   # build lib + examples\hello.c
    .\build.ps1 -Run                              # ...and run it windowed
    .\build.ps1 -Source examples\bounce.c -Run
    .\build.ps1 -Test                             # headless regression suite
#>
param(
    [string]$Source = "examples\hello.c",
    [switch]$Run,
    [switch]$Test,
    [int]$Scale = 2
)

$ErrorActionPreference = "Stop"

# Always leave a real process exit code behind: `throw` does not set one,
# so a CI step running this script would see success after a failed test.
function Fail([string]$message) {
    Write-Host $message -ForegroundColor Red
    exit 1
}

$root  = $PSScriptRoot
$emu   = Join-Path $root "emulator\x16emu.exe"
$rom   = Join-Path $root "emulator\rom.bin"
$src   = Join-Path $root "src"
$core  = Join-Path $src  "core"
$inc   = Join-Path $root "include"
$build = Join-Path $root "build"
$obj   = Join-Path $build "obj"
$lib   = Join-Path $build "x16c.lib"

# --- locate cc65 -----------------------------------------------------
# CC65_HOME wins; then the usual install spots; then whatever is on PATH.
$cc65bin = $null
$candidates = @()
if ($env:CC65_HOME) { $candidates += (Join-Path $env:CC65_HOME "bin") }
$candidates += "C:\Emulator\cc65\bin", "C:\cc65\bin"
foreach ($c in $candidates) {
    if (Test-Path (Join-Path $c "cl65.exe")) { $cc65bin = $c; break }
}
if (-not $cc65bin) {
    $onPath = Get-Command cl65 -ErrorAction SilentlyContinue
    if ($onPath) { $cc65bin = Split-Path $onPath.Source }
}
if (-not $cc65bin) {
    Fail "cc65 not found. Set CC65_HOME, or install to C:\cc65, or put cl65 on PATH."
}

$ca65 = Join-Path $cc65bin "ca65.exe"
$ar65 = Join-Path $cc65bin "ar65.exe"
$cl65 = Join-Path $cc65bin "cl65.exe"

foreach ($tool in @($ca65, $ar65, $cl65, $emu, $rom)) {
    if (-not (Test-Path $tool)) { Fail "missing: $tool" }
}
foreach ($dir in @($build, $obj)) {
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
}

# -Test defaults to the suite, but an explicit -Source still wins (handy
# for running a mutated runner to prove the harness can actually fail).
if ($Test -and -not $PSBoundParameters.ContainsKey('Source')) {
    $Source = "test\runner.c"
}

# --- assemble the library --------------------------------------------
# One .s per module -> one .o -> one member of x16c.lib. ld65 pulls a
# member in only when something references one of its symbols, which is
# exactly what the ACME original's X16_USE_* gates did by hand (ACME has
# no linker and so could not strip dead code).
$sources = @(Get-ChildItem -Path $src -Filter *.s -Recurse -File)
if ($sources.Count -eq 0) { Fail "no .s files under $src" }

# Any change to a shared .inc invalidates every object.
$incs = @(Get-ChildItem -Path $core -Filter *.inc -File)
$incStamp = ($incs | Measure-Object LastWriteTimeUtc -Maximum).Maximum

$assembled = 0
foreach ($s in $sources) {
    $o = Join-Path $obj ("{0}.o" -f $s.BaseName)
    $stale = $true
    if (Test-Path $o) {
        $ot = (Get-Item $o).LastWriteTimeUtc
        $stale = ($s.LastWriteTimeUtc -gt $ot) -or ($incStamp -gt $ot)
    }
    if ($stale) {
        Write-Host ("ca65  {0}" -f $s.FullName.Substring($root.Length + 1))
        & $ca65 --cpu 65C02 -t cx16 -I $core -o $o $s.FullName
        if ($LASTEXITCODE -ne 0) { Fail "assembly failed: $($s.Name)" }
        $assembled++
    }
}

# ar65 has no "replace all" mode, so rebuild the archive from scratch
# whenever anything changed. It is a handful of small objects.
if ($assembled -gt 0 -or -not (Test-Path $lib)) {
    if (Test-Path $lib) { Remove-Item $lib -Force }
    $objs = @(Get-ChildItem -Path $obj -Filter *.o -File | ForEach-Object { $_.FullName })
    & $ar65 a $lib @objs
    if ($LASTEXITCODE -ne 0) { Fail "ar65 failed" }
    Write-Host ("ar65  x16c.lib ({0} modules)" -f $objs.Count)
}

# --- compile and link the program ------------------------------------
if (-not (Test-Path (Join-Path $root $Source))) { Fail "missing source: $Source" }

$name = [IO.Path]::GetFileNameWithoutExtension($Source).ToUpper()
$out  = Join-Path $build "$name.PRG"

Write-Host "cl65  $Source -> $out"
& $cl65 -t cx16 -O -I $inc -o $out (Join-Path $root $Source) $lib
if ($LASTEXITCODE -ne 0) { Fail "compile/link failed" }

$size = (Get-Item $out).Length
Write-Host "      $size bytes"

# --- test ------------------------------------------------------------
if ($Test) {
    Write-Host "x16emu (headless testbench)"

    # x16emu -testbench only exits at stdin EOF, and it only reads stdin
    # once it has printed its own "RDY" prompt -- which it never does if
    # the guest program leaves BASIC in an odd state. So don't wait on the
    # process: watch its output for our DONE line, then stop it ourselves.
    $stdin  = Join-Path $env:TEMP "x16clib-empty.in"
    $stdout = Join-Path $build "test-output.txt"
    [IO.File]::WriteAllText($stdin, "")
    if (Test-Path $stdout) { Remove-Item $stdout -Force }

    # Point device 8 at a scratch directory so the load/save tests are
    # hermetic and never touch a real SD-card image.
    # The suite writes what it reads, so start from an empty directory:
    # a stale file could make a broken save look like a working one.
    $fsroot = Join-Path $root "test\fsroot"
    if (-not (Test-Path $fsroot)) { New-Item -ItemType Directory -Path $fsroot | Out-Null }
    Get-ChildItem $fsroot -File | Remove-Item -Force

    $emuArgs = @('-rom', $rom, '-fsroot', $fsroot, '-prg', $out,
                 '-run', '-warp', '-echo', '-testbench')
    $proc = Start-Process -FilePath $emu -ArgumentList $emuArgs -NoNewWindow -PassThru `
                          -RedirectStandardInput $stdin -RedirectStandardOutput $stdout

    $deadline = (Get-Date).AddSeconds(60)
    $text = ""
    while ($true) {
        Start-Sleep -Milliseconds 200
        if (Test-Path $stdout) {
            $text = (Get-Content $stdout -Raw -ErrorAction SilentlyContinue) -replace "`r", ""
            if ($text -match '(?m)^DONE ') { break }
        }
        if ($proc.HasExited) { break }
        if ((Get-Date) -gt $deadline) {
            if (-not $proc.HasExited) { $proc.Kill() }
            Fail "emulator timed out after 60s -- no DONE line; the test program never finished"
        }
    }
    if (-not $proc.HasExited) { $proc.Kill() }
    $proc.WaitForExit()

    # Names are [A-Z0-9_]. Don't use \S+: a result line ends without a CR,
    # so whatever the next test prints first (a CLS control byte, say)
    # lands on the same line and would be captured as part of the name.
    $passes = ([regex]::Matches($text, '(?m)^PASS ([A-Z0-9_]+)')).Count
    $fails  = [regex]::Matches($text, '(?m)^FAIL ([A-Z0-9_]+)')
    $skips  = [regex]::Matches($text, '(?m)^SKIP ([A-Z0-9_]+)')
    $done   = [regex]::Match($text, '(?m)^DONE ([0-9A-F]{2})/([0-9A-F]{2})')

    foreach ($f in $fails) { Write-Host ("  FAIL {0}" -f $f.Groups[1].Value) -ForegroundColor Red }
    foreach ($s in $skips) { Write-Host ("  SKIP {0}" -f $s.Groups[1].Value) -ForegroundColor Yellow }

    if (-not $done.Success) {
        Fail "test run produced no DONE line -- the program never finished"
    }

    $reportedPass  = [Convert]::ToInt32($done.Groups[1].Value, 16)
    $reportedTotal = [Convert]::ToInt32($done.Groups[2].Value, 16)

    if ($reportedTotal -eq 0) { Fail "no tests ran" }
    if ($passes -ne $reportedPass) {
        Fail "output is inconsistent: $passes PASS lines but DONE says $reportedPass"
    }
    if ($fails.Count -gt 0 -or $reportedPass -ne $reportedTotal) {
        Fail "$($reportedTotal - $reportedPass) of $reportedTotal tests failed"
    }

    $summary = "      $reportedPass/$reportedTotal tests passed"
    if ($skips.Count -gt 0) {
        # Skips are excluded from the pass/total, so they can never be
        # mistaken for passes. Surface them so they are not forgotten.
        $summary += ", $($skips.Count) skipped (not runnable headless)"
    }
    Write-Host $summary -ForegroundColor Green
    exit 0
}

# --- run -------------------------------------------------------------
if ($Run) {
    Write-Host "x16emu $out"
    & $emu -rom $rom -prg $out -run -scale $Scale
}
