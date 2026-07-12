<#
.SYNOPSIS
    Build the x16clib library with llvm-mos, then compile and optionally
    run a C program against it.

    This is the llvm-mos half of the library. The cc65 half is
    build_ca65.ps1, over src_ca65/ and include_ca65/. The two toolchains
    share no object code and no calling convention -- only the API.

.EXAMPLE
    .\build_llvm.ps1                              # build lib + examples\hello.c
    .\build_llvm.ps1 -Run                         # ...and run it windowed
    .\build_llvm.ps1 -Source examples\bounce.c -Run
    .\build_llvm.ps1 -Test                        # headless regression suite
    .\build_llvm.ps1 -Test -Windowed              # ...with video, so VSYNC and
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
$src   = Join-Path $root "src_llvm"
$core  = Join-Path $src  "core"
$inc   = Join-Path $root "include_llvm"
$build = Join-Path $root "build_llvm"
$obj   = Join-Path $build "obj"
# The finished archive is a deliverable, not an intermediate: it goes to
# dist_llvm, which is committed, while build_llvm stays gitignored.
$dist  = Join-Path $root "dist_llvm"
$lib   = Join-Path $dist "libx16c.a"

# --- locate llvm-mos -------------------------------------------------
# LLVM_MOS_HOME wins; then the copy inside the repo; then the usual spots.
$mosbin = $null
$candidates = @()
if ($env:LLVM_MOS_HOME) { $candidates += (Join-Path $env:LLVM_MOS_HOME "bin") }
$candidates += (Join-Path $root "llvm-mos\bin"), "C:\llvm-mos\bin"
foreach ($c in $candidates) {
    if (Test-Path (Join-Path $c "mos-cx16-clang.bat")) { $mosbin = $c; break }
}
if (-not $mosbin) {
    Fail "llvm-mos not found. Set LLVM_MOS_HOME, or unpack the SDK to $root\llvm-mos."
}

# The SDK ships per-target driver .bat wrappers, not .exe files.
$clang = Join-Path $mosbin "mos-cx16-clang.bat"
$ar    = Join-Path $mosbin "llvm-ar.exe"

foreach ($tool in @($clang, $ar, $emu, $rom)) {
    if (-not (Test-Path $tool)) { Fail "missing: $tool" }
}
foreach ($dir in @($build, $obj, $dist)) {
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
}

# --- assemble the library --------------------------------------------
# One .s per module -> one .o -> one member of libx16c.a. ld.lld pulls a
# member in only when something references one of its symbols, the same
# selectivity ld65 gives the cc65 build.
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
        Write-Host ("mos-as {0}" -f $s.FullName.Substring($root.Length + 1))
        # -g embeds DWARF line info for the .s source, so debuggers that
        # load the final ELF (e.g. VS64 attached to an emulator) can step
        # into library routines at the assembly-source level. It only
        # grows the archive/ELF; the extracted PRG is unaffected.
        & $clang -c -g -I $core -o $o $s.FullName
        if ($LASTEXITCODE -ne 0) { Fail "assembly failed: $($s.Name)" }
        $assembled++
    }
}

# llvm-ar rewrites the whole archive; these are small objects.
if ($assembled -gt 0 -or -not (Test-Path $lib)) {
    if (Test-Path $lib) { Remove-Item $lib -Force }
    $objs = @(Get-ChildItem -Path $obj -Filter *.o -File | ForEach-Object { $_.FullName })
    & $ar rcs $lib @objs
    if ($LASTEXITCODE -ne 0) { Fail "llvm-ar failed" }
    Write-Host ("ar     libx16c.a ({0} modules)" -f $objs.Count)
}

# --- compile and link -------------------------------------------------
#
# No -Wl,-D,__STACKSIZE__ equivalent is needed: llvm-mos puts the soft
# stack at $9F00 growing down, outside the BSS the linker sizes.
#
# -mreserve-zp=16 IS REQUIRED, and every program linking this library
# needs it. The cx16 target leaves only ninety zero-page bytes ($26-$7F);
# clang's LTO pass claims as many of them as it likes for hot statics,
# knowing nothing of core/x16zp.s, which unconditionally contributes a
# sixteen-byte .zp.bss. Without the flag the two collide and ld.lld says
#     section '.zp.bss' will not fit in region 'zp': overflowed by 16 bytes
# -- a hard link error, not a warning. Reserving exactly sixteen makes
# clang spill that much of its own data to ordinary memory instead.
#
# The trade is worth it: the library touches its P/T block 1183 times, and
# each zero-page access saves a byte and a cycle over the absolute form.
# Measured against a printf-using program, the reservation costs 82 bytes
# of compiler-generated code (3841 vs 3759).
$ZP_RESERVE = '-mreserve-zp=16'

function Build-Prg([string]$srcRel) {
    if (-not (Test-Path (Join-Path $root $srcRel))) { Fail "missing source: $srcRel" }
    $prgName = [IO.Path]::GetFileNameWithoutExtension($srcRel).ToUpper()
    $prg     = Join-Path $build "$prgName.PRG"

    Write-Host "clang  $srcRel -> $prg"
    & $clang -Os $ZP_RESERVE -I $inc -o $prg (Join-Path $root $srcRel) $lib
    if ($LASTEXITCODE -ne 0) { Fail "compile/link failed: $srcRel" }
    Write-Host ("      {0} bytes" -f (Get-Item $prg).Length)
    return $prg
}

# -Test with no explicit -Source runs both halves of the suite. runner.c
# covers the modules; runner2.c covers the C entry points, where llvm-mos
# and cc65 differ and where a wrong register is silent.
if ($Test -and -not $PSBoundParameters.ContainsKey('Source')) {
    $suites = @('test_llvm\runner.c', 'test_llvm\runner2.c')
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

    $fsroot = Join-Path $root "test_llvm\fsroot"
    if (-not (Test-Path $fsroot)) { New-Item -ItemType Directory -Path $fsroot | Out-Null }
    Get-ChildItem $fsroot -File | Remove-Item -Force

    $stdin = Join-Path $env:TEMP "x16clib-llvm-empty.in"
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
