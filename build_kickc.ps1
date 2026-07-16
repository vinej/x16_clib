<#
.SYNOPSIS
    Compile a C program against the x16clib KickC port, and optionally
    run it or the regression suite.

    This is the KickC third of the library; build_ca65.ps1 and
    build_llvm.ps1 are the other two. KickC has no linker and no archive
    format, so unlike those scripts there is no library-build step here:
    src_kickc\ IS the library, compiled into every program by KickC's
    own library mechanism (-L), and unused routines are stripped by its
    whole-program pass.

.EXAMPLE
    .\build_kickc.ps1                             # compile the test runner
    .\build_kickc.ps1 -Test                       # headless regression suite
    .\build_kickc.ps1 -Test -Windowed             # ...with real video
    .\build_kickc.ps1 -Source mygame.c -Run
#>
param(
    # numbers.c is the one example that stays cc65/llvm-only: it tours
    # printf, and KickC has no <stdio.h>.
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

$root   = $PSScriptRoot
$emu    = Join-Path $root "emulator\x16emu.exe"
$rom    = Join-Path $root "emulator\rom.bin"
$inc    = Join-Path $root "include_kickc"
$libsrc = Join-Path $root "src_kickc"
$build  = Join-Path $root "build_kickc"

# --- locate KickC -----------------------------------------------------
# The repo-local .\kickc copy wins, then KICKC_HOME, then C:\kickc.
# KickC is a Java program (Java 8 or later) with its own include, lib,
# fragment and target trees next to the jar; kickc.bat is just a wrapper
# that passes those paths, and invoking the jar directly here keeps the
# wrapper's echo noise out of the build output.
$kickcHome = $null
$candidates = @((Join-Path $root "kickc"))
if ($env:KICKC_HOME) { $candidates += $env:KICKC_HOME }
$candidates += "C:\kickc"
foreach ($c in $candidates) {
    if (Test-Path (Join-Path $c "jar")) { $kickcHome = $c; break }
}
if (-not $kickcHome) {
    Fail "KickC not found. Unzip a KickC release to .\kickc, or set KICKC_HOME."
}
$jar = @(Get-ChildItem (Join-Path $kickcHome "jar") -Filter "kickc-*.jar")[0]
if (-not $jar) { Fail "no kickc-*.jar under $kickcHome\jar" }

$java = Get-Command java -ErrorAction SilentlyContinue
if (-not $java) { Fail "java not found on PATH. KickC needs a Java 8+ runtime." }

if (-not (Test-Path $build)) { New-Item -ItemType Directory -Path $build | Out-Null }

# --- compile ----------------------------------------------------------
# KickC compiles the whole program every time -- there are no objects to
# reuse -- so the only staleness check worth doing is whether anything at
# all changed since the PRG was written.
function Build-Prg([string]$srcRel) {
    $srcAbs = Join-Path $root $srcRel
    if (-not (Test-Path $srcAbs)) { Fail "missing source: $srcRel" }
    $name = [IO.Path]::GetFileNameWithoutExtension($srcRel)
    $prg  = Join-Path $build "$name.prg"

    $newest = (Get-Item $srcAbs).LastWriteTimeUtc
    $deps = @(Get-ChildItem $libsrc -Recurse -File) + @(Get-ChildItem $inc -Recurse -File)
    if ($srcRel -like 'test_kickc\*') { $deps += @(Get-ChildItem (Join-Path $root "test_kickc") -Filter *.h -File) }
    foreach ($d in $deps) {
        if ($d.LastWriteTimeUtc -gt $newest) { $newest = $d.LastWriteTimeUtc }
    }
    if ((Test-Path $prg) -and ((Get-Item $prg).LastWriteTimeUtc -gt $newest)) {
        Write-Host "kickc $srcRel -> $prg (up to date)"
        return $prg
    }

    Write-Host "kickc $srcRel -> $prg"
    # Both -I/-L pairs matter: KickC's own stdlib first, then this
    # library's headers and sources. -a assembles to a PRG with the
    # bundled KickAssembler; without it the "output" is assembler text.
    & java -jar $jar.FullName `
        -I (Join-Path $kickcHome "include") -L (Join-Path $kickcHome "lib") `
        -F (Join-Path $kickcHome "fragment") -P (Join-Path $kickcHome "target") `
        -p cx16 -a -I $inc -L $libsrc -odir $build $srcAbs 2>&1 |
        Where-Object { $_ -notmatch '^//' -and $_ -notmatch '^Compiling|^Writing asm|^Assembling' } |
        ForEach-Object { Write-Host "      $_" }
    if ($LASTEXITCODE -ne 0) { Fail "kickc failed: $srcRel" }
    if (-not (Test-Path $prg)) { Fail "kickc reported success but $prg is missing" }
    Write-Host ("      {0} bytes" -f (Get-Item $prg).Length)
    return $prg
}

# -Test with no explicit -Source runs both halves of the suite. Two
# PRGs, like the cc65 suite -- there for program RAM, here for zero
# page: one program holding every test plus the whole library carries
# more never-coalesced asm-referenced variables than the $22-$7F user
# window holds, and past its edge KickC allocates into the KERNAL's
# zero page silently.
$suites = @()
if ($Test -and -not $PSBoundParameters.ContainsKey('Source')) {
    $suites = @('test_kickc\runner.c', 'test_kickc\runner2.c', 'test_kickc\runner3.c',
                'test_kickc\runner4.c')
} else {
    $suites = @($Source)
}

foreach ($tool in @($emu, $rom)) {
    if (($Test -or $Run) -and -not (Test-Path $tool)) { Fail "missing: $tool" }
}

$prgs = @()
foreach ($sfile in $suites) { $prgs += (Build-Prg $sfile) }
$out = $prgs[0]

# --- test ------------------------------------------------------------
if ($Test) {
    # Headless -testbench runs no video, so VERA raises neither VSYNC nor
    # a raster interrupt: tests that need one skip themselves and say so.
    # -Windowed opens a real display, at real speed, and those tests run.
    if ($Windowed) {
        Write-Host "x16emu (windowed: video, VSYNC and raster interrupts live)"
    } else {
        Write-Host "x16emu (headless testbench)"
    }

    # Point device 8 at a scratch directory so any load/save tests are
    # hermetic and never touch a real SD-card image.
    $fsroot = Join-Path $root "test_kickc\fsroot"
    if (-not (Test-Path $fsroot)) { New-Item -ItemType Directory -Path $fsroot | Out-Null }
    Get-ChildItem $fsroot -File | Where-Object { $_.Name -ne '.gitkeep' } | Remove-Item -Force

    $stdin = Join-Path $env:TEMP "x16clib-empty.in"
    [IO.File]::WriteAllText($stdin, "")

    $totalPass = 0; $totalRun = 0; $totalSkip = 0; $anyFail = $false

    foreach ($prg in $prgs) {
        $stdout = Join-Path $build ("{0}-output.txt" -f [IO.Path]::GetFileNameWithoutExtension($prg))
        if (Test-Path $stdout) { Remove-Item $stdout -Force }

        # -echo works either way; it is what puts CHROUT on stdout.
        if ($Windowed) {
            $emuArgs = @('-rom', $rom, '-fsroot', $fsroot, '-prg', $prg,
                         '-run', '-echo', '-scale', $Scale)
        } else {
            $emuArgs = @('-rom', $rom, '-fsroot', $fsroot, '-prg', $prg,
                         '-run', '-warp', '-echo', '-testbench')
        }

        # x16emu -testbench only exits at stdin EOF, and it only reads
        # stdin once it has printed its own "RDY" prompt -- which it never
        # does if the guest program leaves BASIC in an odd state. So don't
        # wait on the process: watch its output for our DONE line.
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

        # Names are [A-Z0-9_]. Don't use \S+: a result line ends without a
        # CR, so whatever the next test prints first lands on the same
        # line and would be captured as part of the name.
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
    if ($totalSkip -gt 0) {
        $summary += ", $totalSkip skipped (not runnable headless)"
    }
    Write-Host $summary -ForegroundColor Green
    exit 0
}

# --- run --------------------------------------------------------------
if ($Run) {
    Write-Host "x16emu $out"
    & $emu -rom $rom -prg $out -run -scale $Scale
}
