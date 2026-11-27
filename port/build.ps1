# Builds the Windows game: fetch pinned cocos2d-x v2 + deps, apply patches/, build app/ (Win32 Release).
# usage: build.ps1 [-Rebuild] [-Install] [-Test]
#   -Rebuild  full MSBuild rebuild     -Install  copy out\bin to %LOCALAPPDATA%\NinjaHeroesPC\bin
#   -Test     build and run tests\*.cpp (pure C++ unit tests, no engine)
param([switch]$Rebuild, [switch]$Install, [switch]$Test)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # PowerShell 5.1's progress bar slows downloads to a crawl

$lock = Get-Content "$PSScriptRoot\engine.lock" -Raw | ConvertFrom-StringData
$work = "$PSScriptRoot\.engine"
$eng = "$work\cocos2d-x"
$out = "$PSScriptRoot\out"

function Invoke-Git([string]$dir) {
    $ErrorActionPreference = 'Continue'   # git writes warnings to stderr; judge by exit code
    & git -C $dir @args
    if ($LASTEXITCODE) { throw "git $args failed in $dir" }
}

# Runs git where failure is an answer, not an error (PowerShell 5.1 turns native stderr into errors under Stop).
function Test-Git([string]$dir) {
    $ErrorActionPreference = 'Continue'
    $output = & git -C $dir @args 2>$null
    return @{ ok = ($LASTEXITCODE -eq 0); output = $output }
}

function Get-PinnedRepo([string]$dir, [string]$repo, [string]$commit) {
    if (-not (Test-Path "$dir\.git")) {
        New-Item -ItemType Directory -Force $dir | Out-Null
        Invoke-Git $dir init -q
        Invoke-Git $dir config core.autocrlf true
        Invoke-Git $dir remote add origin $repo
    }
    $head = (Test-Git $dir rev-parse --verify -q HEAD).output
    if ($head -ne $commit) {
        Write-Host "fetching $repo @ $commit"
        Invoke-Git $dir fetch -q --depth 1 origin $commit
        Invoke-Git $dir checkout -q --force FETCH_HEAD
        return $true
    }
    return $false
}

function Expand-Deps {
    $zip = "$work\deps.zip"
    if (-not (Test-Path $zip) -or (Get-FileHash $zip -Algorithm SHA256).Hash -ne $lock.deps_sha256) {
        Write-Host "downloading $($lock.deps_url)"
        Invoke-WebRequest $lock.deps_url -OutFile $zip -UseBasicParsing
    }
    if ((Get-FileHash $zip -Algorithm SHA256).Hash -ne $lock.deps_sha256) { throw "deps.zip checksum mismatch" }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zip)
    try {
        foreach ($e in $archive.Entries) {
            $rel = $e.FullName.Substring($e.FullName.IndexOf('/') + 1)
            if (-not $rel -or $rel.EndsWith('/')) { continue }
            if ($rel -match '(^|/)\.\.(/|$)' -or $rel -match ':') { throw "unsafe path in deps.zip: $rel" }
            $target = Join-Path $eng $rel
            New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, $target, $true)
        }
    } finally { $archive.Dispose() }
}

$stamp = "$work\applied.txt"   # names + hashes of the patch set currently applied to the engine

# When the patch set changes (added, edited, removed), reset the engine's tracked files and
# apply every patch again: an edited patch can't be applied on top of its old version.
function Invoke-Patches {
    $patches = Get-ChildItem "$PSScriptRoot\patches\*.patch" | Sort-Object Name
    $want = ($patches | ForEach-Object { "$($_.Name) $((Get-FileHash $_.FullName -Algorithm SHA256).Hash)" }) -join "`n"
    if ((Test-Path $stamp) -and (Get-Content $stamp -Raw).Trim() -eq $want) { return }
    Invoke-Git $eng checkout --force -- .
    foreach ($p in $patches) {
        Write-Host "applying $($p.Name)"
        Invoke-Git $eng apply --ignore-whitespace $p.FullName
    }
    Set-Content $stamp $want -Encoding ascii
}

function Invoke-InDevShell([string]$command) {
    $ErrorActionPreference = 'Continue'   # judge by exit code; vcvars chatters on stderr
    $installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
    $vs = & "$installer\vswhere.exe" -products * -latest -property installationPath
    & cmd /c "set `"PATH=$installer;%PATH%`" && call `"$vs\VC\Auxiliary\Build\vcvars32.bat`" >nul && $command"
    if ($LASTEXITCODE) { throw "failed: $command" }
}

if ($Test) {
    New-Item -ItemType Directory -Force "$out\tests" | Out-Null
    # engine-free sources the tests may link against
    $pure = @("$PSScriptRoot\app\Classes\DesktopInputMath.cpp") | Where-Object { Test-Path $_ } | ForEach-Object { "`"$_`"" }
    foreach ($t in Get-ChildItem "$PSScriptRoot\tests\*.cpp") {
        $exe = "$out\tests\$($t.BaseName).exe"
        Invoke-InDevShell "cl /nologo /EHsc /std:c++17 /I`"$PSScriptRoot\app\Classes`" `"$($t.FullName)`" $($pure -join ' ') /Fo`"$out\tests\\`" /Fe`"$exe`""
        & $exe
        if ($LASTEXITCODE) { throw "$($t.BaseName) failed" }
    }
    return
}

$fresh = Get-PinnedRepo $eng $lock.engine_repo $lock.engine_commit
if ($fresh -and (Test-Path $stamp)) { Remove-Item $stamp }   # a new checkout has no patches applied
$null = Get-PinnedRepo "$eng\$($lock.bindings_path)" $lock.bindings_repo $lock.bindings_commit
if ($fresh -or -not (Test-Path "$eng\scripting\javascript\spidermonkey-win32\lib\mozjs.dll")) { Expand-Deps }
Invoke-Patches

$proj = "$eng\projects\NinjaHeroes"
& robocopy "$PSScriptRoot\app" $proj /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "copying app failed" }

$msb = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -products * -latest -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
$env:CL = "/DHAVE_STRUCT_TIMESPEC"   # old pthreads-win32 redefines timespec
$env:LINK = "legacy_stdio_definitions.lib ws2_32.lib"   # old prebuilt libs need pre-2015 stdio symbols
$target = if ($Rebuild) { "Rebuild" } else { "Build" }
New-Item -ItemType Directory -Force $out | Out-Null
& $msb "$proj\proj.win32\NinjaHeroes.sln" /m /t:$target /nologo /v:m /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 *> "$out\build.log"
if ($LASTEXITCODE) {
    Select-String -Path "$out\build.log" -Pattern ": (fatal )?error " | Select-Object -First 20 | ForEach-Object { $_.Line }
    throw "build failed (see port\out\build.log)"
}

$bin = "$out\bin"
if (Test-Path $bin) { Remove-Item $bin -Recurse -Force }
New-Item -ItemType Directory -Force $bin | Out-Null
$rel = "$proj\proj.win32\Release.win32"
Copy-Item "$rel\NinjaHeroes.exe", "$rel\*.dll", "$rel\*.pdb" $bin
Copy-Item "$eng\scripting\javascript\bindings\js\*.js" $bin   # engine JS glue the game require()s (jsb.js, ...)
Copy-Item "$PSScriptRoot\app\Resources\shim.js" $bin -ErrorAction SilentlyContinue
Write-Host "built $bin"

if ($Install) {
    $dest = "$env:LOCALAPPDATA\NinjaHeroesPC\bin"
    New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item "$bin\*" $dest -Force
    Write-Host "installed to $dest"
}
