<#
.SYNOPSIS
    Build the x16clib library with vbcc, then compile and optionally run a
    C program against it.

    This is the vbcc half of the library. The cc65 half is build_ca65.ps1,
    the llvm-mos half build_llvm.ps1. The toolchains share no object code
    and no calling convention -- only the API.

    vbcc's 6502 backend ships a native Commander X16 target (+x16). We use
    that toolchain (vbcc6502 + vasm6502_oldstyle + vlink) but port THIS
    library's modules, so the same x16_* API is available under vbcc as
    under the other four toolchains.

.EXAMPLE
    .\build_vbcc.ps1                              # build lib + examples\hello.c
    .\build_vbcc.ps1 -Run                         # ...and run it windowed
    .\build_vbcc.ps1 -Source examples\bounce.c -Run
    .\build_vbcc.ps1 -Test                        # headless regression suite
    .\build_vbcc.ps1 -Test -Windowed              # ...with video, so VSYNC and
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
$src   = Join-Path $root "src_vbcc"
$core  = Join-Path $src  "core"
$inc   = Join-Path $root "include_vbcc"
$build = Join-Path $root "build_vbcc"
$obj   = Join-Path $build "obj"
# The finished archive is a deliverable, not an intermediate: it goes to
# dist_vbcc, which is committed, while build_vbcc stays gitignored.
$dist  = Join-Path $root "dist_vbcc"
$lib   = Join-Path $dist "libx16c.a"

# --- locate vbcc -----------------------------------------------------
# VBCC (the env var vc.exe itself reads) wins; then the copy inside the
# repo. vc.exe shells out to vbcc6502/vasm/vlink by bare name, so their
# bin directory must be on PATH.
$vbccHome = $null
$candidates = @()
if ($env:VBCC) { $candidates += $env:VBCC }
$candidates += (Join-Path $root "vbcc6502\vbcc6502_win\vbcc"), "C:\vbcc"
foreach ($c in $candidates) {
    if (Test-Path (Join-Path $c "bin\vc.exe")) { $vbccHome = $c; break }
}
if (-not $vbccHome) {
    Fail "vbcc not found. Set VBCC, or unpack the 6502 distribution to $root\vbcc6502."
}
$env:VBCC = $vbccHome
$env:PATH = (Join-Path $vbccHome "bin") + ";" + $env:PATH

$vc = Join-Path $vbccHome "bin\vc.exe"
$as = Join-Path $vbccHome "bin\vasm6502_oldstyle.exe"

# ar makes the standard !<arch> archive vlink links selectively from.
$ar = (Get-Command ar -ErrorAction SilentlyContinue).Source
if (-not $ar) { Fail "GNU ar not found on PATH (ships with Git for Windows)." }

foreach ($tool in @($vc, $as, $emu, $rom)) {
    if (-not (Test-Path $tool)) { Fail "missing: $tool" }
}
foreach ($dir in @($build, $obj, $dist)) {
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
}

# --- assemble the library --------------------------------------------
# One .s per module -> one .o -> one member of libx16c.a. vlink pulls a
# member in only when something references one of its symbols, the same
# selectivity ld65 gives the cc65 build. Files beginning with '_' are
# private fragments, never assembled on their own.
#
# These are the exact assembler flags the +x16 config passes, plus -I for
# the shared core/ includes. -opt-branch is REQUIRED by the vbcc backend.
$asFlags = @('-quiet','-nosym','-vobj3','-c02','-nowarn=62','-opt-branch',
             '-Fvobj','-ldots','-I', $core)

$sources = @(Get-ChildItem -Path $src -Filter *.s -Recurse -File |
             Where-Object { $_.Name -notlike '_*' })
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
        Write-Host ("vasm  {0}" -f $s.FullName.Substring($root.Length + 1))
        & $as @asFlags $s.FullName -o $o
        if ($LASTEXITCODE -ne 0) { Fail "assembly failed: $($s.Name)" }
        $assembled++
    }
}

# ar rewrites the whole archive; these are small objects.
if ($assembled -gt 0 -or -not (Test-Path $lib)) {
    if (Test-Path $lib) { Remove-Item $lib -Force }
    $objs = @(Get-ChildItem -Path $obj -Filter *.o -File | ForEach-Object { $_.FullName })
    & $ar rcs $lib @objs
    if ($LASTEXITCODE -ne 0) { Fail "ar failed" }
    Write-Host ("ar     libx16c.a ({0} modules)" -f $objs.Count)
}

# --- compile and link -------------------------------------------------
#
# vc drives compile (vbcc6502) + assemble (vasm) + link (vlink), links the
# +x16 startup and libvc automatically, and produces a CBM .PRG loading at
# $0801. -O=1 is the +x16 default. The library needs no special linker
# flag: its P/T zero-page block is a plain zpage-section reservation the
# linker places clear of vbcc's own r0..r31/sp/btmp registers.
function Build-Prg([string]$srcRel) {
    if (-not (Test-Path (Join-Path $root $srcRel))) { Fail "missing source: $srcRel" }
    $prgName = [IO.Path]::GetFileNameWithoutExtension($srcRel).ToUpper()
    $prg     = Join-Path $build "$prgName.PRG"

    Write-Host "vc     $srcRel -> $prg"
    & $vc +x16 "-I$inc" (Join-Path $root $srcRel) $lib -o $prg
    if ($LASTEXITCODE -ne 0) { Fail "compile/link failed: $srcRel" }
    Write-Host ("      {0} bytes" -f (Get-Item $prg).Length)
    return $prg
}

# -Test with no explicit -Source runs the whole suite. Only runner.c
# exists so far: it covers the modules and the C entry points, where the
# ABI is silent if wrong. The list grows when a runner does -- a name
# here with no file behind it fails the build with "missing source",
# which is what runner2.c and runner3.c did to a bare -Test.
if ($Test -and -not $PSBoundParameters.ContainsKey('Source')) {
    $suites = @('test_vbcc\runner.c', 'test_vbcc\runner2.c')
} else {
    $suites = @($Source)
}

$prgs = @()
foreach ($sfile in $suites) { $prgs += (Build-Prg $sfile) }
$out = $prgs[0]

# --- test ------------------------------------------------------------
if ($Test) {
    if ($Windowed) {
        Write-Host "x16emu (windowed: video, VSYNC and raster interrupts live)"
    } else {
        Write-Host "x16emu (headless testbench)"
    }

    $fsroot = Join-Path $root "test_vbcc\fsroot"
    if (-not (Test-Path $fsroot)) { New-Item -ItemType Directory -Path $fsroot | Out-Null }
    Get-ChildItem $fsroot -File | Where-Object { $_.Name -ne '.gitkeep' } | Remove-Item -Force

    $stdin = Join-Path $env:TEMP "x16clib-vbcc-empty.in"
    [IO.File]::WriteAllText($stdin, "")

    $totalPass = 0; $totalRun = 0; $totalSkip = 0; $anyFail = $false

    foreach ($prg in $prgs) {
        $stdout = Join-Path $build ("{0}-output.txt" -f [IO.Path]::GetFileNameWithoutExtension($prg))
        if (Test-Path $stdout) { Remove-Item $stdout -Force }

        if ($Windowed) {
            $emuArgs = @('-rom', $rom, '-fsroot', $fsroot, '-prg', $prg,
                         '-run', '-echo', '-scale', $Scale)
        } else {
            $emuArgs = @('-rom', $rom, '-fsroot', $fsroot, '-prg', $prg,
                         '-run', '-warp', '-echo', '-testbench')
        }

        # x16emu -testbench only exits at stdin EOF, and it only reads stdin
        # once it has printed its own "RDY" prompt. Watch its output for our
        # DONE line instead of waiting on the process.
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
        # so whatever the next test prints first lands on the same line.
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
