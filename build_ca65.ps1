<#
.SYNOPSIS
    Build the x16clib library with cc65, then compile and optionally run
    a C program against it.

    This is the cc65/ca65 half of the library. The llvm-mos half is
    build_llvm.ps1, over src_llvm/ and include_llvm/. The two toolchains
    share no object code and no calling convention -- only the API.

.EXAMPLE
    .\build_ca65.ps1                              # build lib + examples\hello.c
    .\build_ca65.ps1 -Run                         # ...and run it windowed
    .\build_ca65.ps1 -Source examples\bounce.c -Run
    .\build_ca65.ps1 -Test                        # headless regression suite
    .\build_ca65.ps1 -Test -Windowed              # ...with video, so VSYNC and
                                                  # raster interrupts really fire
#>
param(
    [string]$Source = "examples\hello.c",
    [switch]$Run,
    [switch]$Test,
    [switch]$Windowed,
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
$src   = Join-Path $root "src_ca65"
$core  = Join-Path $src  "core"
$inc   = Join-Path $root "include_ca65"
$build = Join-Path $root "build_ca65"
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

# --- compile and link -------------------------------------------------
#
# The test runner is built TWICE. All 28 library modules plus 150-odd test
# functions no longer fit in the X16's 38.6 KB of program RAM, so
# test/runner2.c re-includes runner.c with SUITE=2 and takes the other
# half. Each PRG links only the modules its half of the tests reaches.
#
# __STACKSIZE__ is a LINKER symbol (cx16.cfg sizes BSS as
# __HIMEM__ - __ONCE_RUN__ - __STACKSIZE__), so it goes through -Wl. A
# plain -D would define it for the C preprocessor and change nothing. The
# runner does not recurse, so 1K of C stack is ample.
function Build-Prg([string]$srcRel) {
    if (-not (Test-Path (Join-Path $root $srcRel))) { Fail "missing source: $srcRel" }
    $prgName = [IO.Path]::GetFileNameWithoutExtension($srcRel).ToUpper()
    $prg     = Join-Path $build "$prgName.PRG"

    Write-Host "cl65  $srcRel -> $prg"
    $linkArgs = @('-t', 'cx16', '-O', '-I', $inc, '-o', $prg)
    if ($srcRel -like 'test_ca65\*') { $linkArgs += @('-Wl', '-D,__STACKSIZE__=0x0400') }
    $linkArgs += (Join-Path $root $srcRel)
    $linkArgs += $lib

    & $cl65 @linkArgs
    if ($LASTEXITCODE -ne 0) { Fail "compile/link failed: $srcRel" }
    Write-Host ("      {0} bytes" -f (Get-Item $prg).Length)
    return $prg
}

# -Test with no explicit -Source runs both halves of the suite.
$suites = @()
if ($Test -and -not $PSBoundParameters.ContainsKey('Source')) {
    $suites = @('test_ca65\runner.c', 'test_ca65\runner2.c')
} else {
    $suites = @($Source)
}

$prgs = @()
foreach ($sfile in $suites) { $prgs += (Build-Prg $sfile) }
$out = $prgs[0]

# --- test ------------------------------------------------------------
if ($Test) {
    # Headless -testbench runs no video, so VERA raises neither VSYNC nor a
    # raster interrupt: the tests that need one skip themselves and say so.
    # -Windowed opens a real display, at real speed, and those tests run.
    if ($Windowed) {
        Write-Host "x16emu (windowed: video, VSYNC and raster interrupts live)"
    } else {
        Write-Host "x16emu (headless testbench)"
    }

    # Point device 8 at a scratch directory so the load/save tests are
    # hermetic and never touch a real SD-card image. Cleared ONCE, before
    # the first suite: a stale file could make a broken save look like a
    # working one, but each suite cleans up after itself.
    $fsroot = Join-Path $root "test_ca65\fsroot"
    if (-not (Test-Path $fsroot)) { New-Item -ItemType Directory -Path $fsroot | Out-Null }
    Get-ChildItem $fsroot -File | Remove-Item -Force

    $stdin = Join-Path $env:TEMP "x16clib-empty.in"
    [IO.File]::WriteAllText($stdin, "")

    $totalPass = 0; $totalRun = 0; $totalSkip = 0; $anyFail = $false

    foreach ($prg in $prgs) {
        $stdout = Join-Path $build ("{0}-output.txt" -f [IO.Path]::GetFileNameWithoutExtension($prg))
        if (Test-Path $stdout) { Remove-Item $stdout -Force }

        # -echo works either way; it is what puts CHROUT on stdout. Windowed
        # drops -testbench (which forces headless) and -warp (real 60 Hz, so
        # a raster interrupt lands where the scanline counter says it should).
        if ($Windowed) {
            $emuArgs = @('-rom', $rom, '-fsroot', $fsroot, '-prg', $prg,
                         '-run', '-echo', '-scale', $Scale)
        } else {
            $emuArgs = @('-rom', $rom, '-fsroot', $fsroot, '-prg', $prg,
                         '-run', '-warp', '-echo', '-testbench')
        }

        # x16emu -testbench only exits at stdin EOF, and it only reads stdin
        # once it has printed its own "RDY" prompt -- which it never does if
        # the guest program leaves BASIC in an odd state. So don't wait on
        # the process: watch its output for our DONE line, then stop it.
        $proc = Start-Process -FilePath $emu -ArgumentList $emuArgs -NoNewWindow -PassThru `
                              -RedirectStandardInput $stdin -RedirectStandardOutput $stdout

        $deadline = (Get-Date).AddSeconds(120)
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
                Fail "$([IO.Path]::GetFileName($prg)) timed out after 120s -- no DONE line"
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
            Fail "$([IO.Path]::GetFileName($prg)) produced no DONE line -- it never finished"
        }

        $reportedPass  = [Convert]::ToInt32($done.Groups[1].Value, 16)
        $reportedTotal = [Convert]::ToInt32($done.Groups[2].Value, 16)

        if ($reportedTotal -eq 0) { Fail "no tests ran in $([IO.Path]::GetFileName($prg))" }
        if ($passes -ne $reportedPass) {
            Fail "output is inconsistent: $passes PASS lines but DONE says $reportedPass"
        }
        if ($fails.Count -gt 0 -or $reportedPass -ne $reportedTotal) { $anyFail = $true }

        $totalPass += $reportedPass
        $totalRun  += $reportedTotal
        $totalSkip += $skips.Count
    }

    if ($anyFail) { Fail "$($totalRun - $totalPass) of $totalRun tests failed" }

    $summary = "      $totalPass/$totalRun tests passed"
    if ($prgs.Count -gt 1) { $summary += " across $($prgs.Count) suites" }
    if ($totalSkip -gt 0) {
        # Skips are excluded from the pass/total, so they can never be
        # mistaken for passes. Surface them so they are not forgotten.
        $summary += ", $totalSkip skipped (not runnable headless)"
    }
    Write-Host $summary -ForegroundColor Green
    exit 0
}

# --- run -------------------------------------------------------------
if ($Run) {
    Write-Host "x16emu $out"
    & $emu -rom $rom -prg $out -run -scale $Scale
}
