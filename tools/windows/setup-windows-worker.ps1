# setup-windows-worker.ps1
#
# Turn a Windows 10/11 machine into an altairsim BUILD WORKER: another computer can ssh in,
# build the repo with MSVC, and run the tests. Safe to re-run -- every step checks first and
# skips what is already done.
#
# Normal use: double-click RUN-ME-setup-windows-worker.bat (NOT this .ps1 -- double-clicking a
# .ps1 only opens it in Notepad). Or run this file directly:
#     powershell -ExecutionPolicy Bypass -File setup-windows-worker.ps1 [-Build]
# Either way, if it is not already running as administrator it asks for that (UAC prompt) and
# re-runs itself elevated in a new window.
#
# It needs nothing but a working internet connection and Windows 10 1803+ (curl.exe, tar.exe).
# The repo is the checkout this script is in (tools\windows\ under the repo root) and is left
# exactly as it is. Only a script copied out of the repo (say, onto a USB stick) needs
# -RepoDir <folder>, and then it clones the repo into that folder.
#
# Modelled on the Windows worker of DISTRIBUTION.md 4 (Windows 10, static SDL3
# 3.4.12), inventoried 2026-09-21. The compiler is Visual Studio 2026 (18.x) -- the same MSVC
# as CI's windows-2025-vs2026 runner, so a release is built by the compiler every PR is
# checked with.
#
# What it sets up, in order:
#   1. OpenSSH Server, running and automatic, with an inbound firewall rule on TCP 22
#   2. authorized ssh keys, in the file sshd really reads for this account
#   3. Visual Studio 2026 Build Tools (MSVC + the bundled CMake and Ninja), unless a Visual
#      Studio 2026 with the C++ tools (Community, say) is already there
#   4. CMake and Ninja on the USER path (the installer does not put them there), replacing
#      any other Visual Studio's CMake and Ninja there
#   5. Git for Windows (git + Git Bash, which tools/build-package.sh needs)
#   6. the repo: the checkout this script is in, used as it is (cloned only if -RepoDir is given)
#   7. a STATIC SDL3, built by tools\build-sdl3-static.bat into %USERPROFILE%\opt\sdl3-static
#   8. no sleeping on AC power, so the machine is still there when the ssh arrives
#   9. a check that altairsim configures with video enabled -- and, with -Build, a full build
#      and test run
#
# What it does NOT do: it holds no GitHub credentials and makes no delivery key. A worker only
# ever needs to READ the public repo. Making the scp delivery key (DISTRIBUTION.md 4.5) is a
# separate, deliberate step, because its public half has to be added to the coordinator by hand.

[CmdletBinding()]
param(
    # The repo is the checkout this script is in (tools\windows\ under the repo root). Give
    # -RepoDir only if the script was copied out of the repo (e.g. onto a USB stick): it is then
    # cloned from -RepoUrl into that folder.
    [string]   $RepoDir,
    [string]   $RepoUrl = 'https://github.com/deltecent/altairsim.git',
    # Public keys allowed to ssh in (one line each, e.g. the contents of id_ed25519.pub).
    # If none are given the script asks for them; press Enter at the prompt to skip.
    [string[]] $AuthorizedKey = @(),
    [switch]   $Build,             # also run the full Release build and ctest (slow on a small VM)
    [switch]   $SkipSleepSettings, # leave the power plan alone
    [switch]   $NoPause            # do not wait for Enter at the end
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'   # Add-WindowsCapability / downloads crawl with the bar
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# --- elevate: not admin? ask for it (UAC prompt) and re-run this script in that window ------
$curId = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not ([Security.Principal.WindowsPrincipal]$curId).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host 'This needs administrator rights. Approve the prompt to continue...'
    $fwd = @()
    foreach ($kv in $PSBoundParameters.GetEnumerator()) {
        if     ($kv.Value -is [switch]) { if ($kv.Value) { $fwd += "-$($kv.Key)" } }
        elseif ($kv.Value -is [array])  { $fwd += "-$($kv.Key)"; $fwd += (($kv.Value | ForEach-Object { '"' + $_ + '"' }) -join ',') }
        else                            { $fwd += "-$($kv.Key)"; $fwd += ('"' + $kv.Value + '"') }
    }
    $argList = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" ' + ($fwd -join ' ')
    try { Start-Process -FilePath (Get-Process -Id $PID).Path -Verb RunAs -ArgumentList $argList }
    catch { Write-Host 'Elevation was declined or failed; nothing was changed.' -ForegroundColor Red; exit 1 }
    exit 0
}

# --- log: next to the script (handy on a USB stick), but never inside a git checkout, and in
# --- %TEMP% if that place is read-only ---------------------------------------------------------
$logName = 'setup-windows-worker.log'
$inRepo = $false
for ($d = $PSScriptRoot; $d; $d = Split-Path $d) { if (Test-Path (Join-Path $d '.git')) { $inRepo = $true; break } }
$log = if ($inRepo) { Join-Path $env:TEMP $logName } else { Join-Path $PSScriptRoot $logName }
try { Start-Transcript -Path $log -Force | Out-Null }
catch {
    $log = Join-Path $env:TEMP $logName
    try { Start-Transcript -Path $log -Force | Out-Null } catch { $log = '(no log)' }
}

$script:failed = $false
function Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
# Every step says which it was, so the output (and the summary at the end) tells you what this
# run actually CHANGED on the machine versus what was already in place:
#   Did  = this run installed / added / changed something
#   Had  = already there; left alone
#   Ok   = a check that passed (changes nothing)
#   Todo = something you still have to do
$script:did  = New-Object System.Collections.Generic.List[string]
$script:had  = New-Object System.Collections.Generic.List[string]
$script:todo = New-Object System.Collections.Generic.List[string]
function Did($msg)  { $script:did.Add($msg);  Write-Host "    [changed]  $msg" -ForegroundColor Yellow }
function Had($msg)  { $script:had.Add($msg);  Write-Host "    [already]  $msg" -ForegroundColor DarkGray }
function Ok($msg)   { Write-Host "    [check]    $msg" -ForegroundColor Green }
function Todo($msg) { $script:todo.Add($msg); Write-Host "    [todo]     $msg" -ForegroundColor Magenta }
function Note($msg) { Write-Host "    $msg" }
function Fetch($url, $out) {
    curl.exe -fsSL -o $out $url
    if ($LASTEXITCODE -ne 0) { throw "download failed: $url" }
}
function AddToProcessPath($dir) {
    if (($env:Path -split ';') -notcontains $dir) { $env:Path = "$env:Path;$dir" }
}

try {
    # --- 0. preconditions ------------------------------------------------------------------
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $isAdmin = ([Security.Principal.WindowsPrincipal]$id).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    Write-Host "Computer : $env:COMPUTERNAME"
    Write-Host "Account  : $($id.Name)   elevated: $isAdmin"
    Write-Host "Windows  : $((Get-CimInstance Win32_OperatingSystem).Caption) build $([Environment]::OSVersion.Version.Build)"
    if (-not $isAdmin) { throw 'Not elevated. Double-click RUN-ME-setup-windows-worker.bat, or use an Administrator PowerShell.' }
    if (-not (Get-Command curl.exe -ErrorAction SilentlyContinue)) { throw 'curl.exe is missing (needs Windows 10 1803 or later).' }
    $userName = $env:USERNAME

    # Which repo? The one this script lives in: tools\windows\ under the repo root. Only a script
    # copied somewhere else (a USB stick) needs -RepoDir, and then it clones there. Settled NOW,
    # before anything is installed, so a bad answer costs seconds and not a multi-GB install.
    $isRepo = { param($d) (Test-Path (Join-Path $d 'tools\build-sdl3-static.bat')) -or (Test-Path (Join-Path $d '.git')) }
    if (-not $RepoDir) {
        $RepoDir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
        if (-not (& $isRepo $RepoDir)) {
            throw "This script is not inside an altairsim checkout (it expects to be in tools\windows\ under the repo root, and $RepoDir is not one). Run it from the repo, or pass -RepoDir <folder> to clone the repo into an empty or new folder."
        }
    }
    $repoIsCheckout = [bool](& $isRepo $RepoDir)
    if (-not $repoIsCheckout -and (Test-Path $RepoDir) -and (Get-ChildItem $RepoDir -Force | Select-Object -First 1)) {
        throw "$RepoDir already exists, is not empty, and is not an altairsim checkout, so the repo cannot be cloned there. Nothing in it was touched. Choose an empty or new folder for -RepoDir."
    }

    # --- 1. OpenSSH Server -----------------------------------------------------------------
    Step 'OpenSSH Server'
    $sshdWas = [bool](Get-Service sshd -ErrorAction SilentlyContinue)
    if (-not $sshdWas) {
        try {
            $cap = Get-WindowsCapability -Online -Name 'OpenSSH.Server*' | Select-Object -First 1
            if ($cap.State -ne 'Installed') { Add-WindowsCapability -Online -Name $cap.Name | Out-Null }
        } catch { Note "Windows capability route failed ($($_.Exception.Message)); trying Win32-OpenSSH." }
    }
    if (-not (Get-Service sshd -ErrorAction SilentlyContinue)) {
        $rel = curl.exe -fsSL https://api.github.com/repos/PowerShell/Win32-OpenSSH/releases/latest | ConvertFrom-Json
        $asset = $rel.assets | Where-Object { $_.name -match '^OpenSSH-Win64-.*\.msi$' } | Select-Object -First 1
        if (-not $asset) { throw 'Could not find a Win32-OpenSSH MSI to install.' }
        $msi = Join-Path $env:TEMP $asset.name
        Fetch $asset.browser_download_url $msi
        $p = Start-Process msiexec.exe -Wait -PassThru -ArgumentList '/i', "`"$msi`"", '/qn', '/norestart'
        if ($p.ExitCode -notin 0, 3010) { throw "OpenSSH MSI failed, exit $($p.ExitCode)" }
    }
    if (-not (Get-Service sshd -ErrorAction SilentlyContinue)) { throw 'sshd is still not installed.' }
    $sshdChanged = $false
    if (-not $sshdWas) { Did 'OpenSSH Server installed'; $sshdChanged = $true }
    if ((Get-Service sshd).StartType -ne 'Automatic') { Set-Service sshd -StartupType Automatic; Did 'sshd set to start automatically'; $sshdChanged = $true }
    if ((Get-Service sshd).Status -ne 'Running') { Start-Service sshd; Did 'sshd started'; $sshdChanged = $true }   # first start writes sshd_config + host keys
    if (-not $sshdChanged) { Had 'OpenSSH Server installed, running, starts automatically' }

    $rule = Get-NetFirewallRule -DisplayName '*ssh*' -ErrorAction SilentlyContinue |
        Where-Object { $_.Direction -eq 'Inbound' -and $_.Action -eq 'Allow' -and $_.Enabled -eq 'True' } |
        Where-Object { ($_ | Get-NetFirewallPortFilter).LocalPort -contains '22' } | Select-Object -First 1
    if (-not $rule) {
        Get-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -ErrorAction SilentlyContinue | Remove-NetFirewallRule
        New-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -DisplayName 'OpenSSH SSH Server (sshd)' `
            -Enabled True -Direction Inbound -Protocol TCP -Action Allow -LocalPort 22 -Profile Any | Out-Null
        Did 'added inbound firewall rule for TCP 22 (all profiles)'
    } else { Had "firewall already allows inbound TCP 22 ($($rule.DisplayName))" }

    # --- 2. authorized keys ----------------------------------------------------------------
    Step 'authorized ssh keys'
    # Members of Administrators are read from administrators_authorized_keys and their own
    # ~\.ssh\authorized_keys is IGNORED. That file must be ACL'd to exactly Administrators +
    # SYSTEM or sshd silently ignores it too.
    $inAdmins = [bool]((net localgroup Administrators) -match "(^|\\)$([regex]::Escape($userName))\s*$")
    if ($inAdmins) { $keyFile = Join-Path $env:ProgramData 'ssh\administrators_authorized_keys' }
    else           { $keyFile = Join-Path $env:USERPROFILE '.ssh\authorized_keys' }
    Note "'$userName' is $(if ($inAdmins) {'an Administrator'} else {'a standard user'}); key file: $keyFile"
    $dir = Split-Path $keyFile
    if (-not (Test-Path $dir))     { New-Item -ItemType Directory -Path $dir | Out-Null }
    if (-not (Test-Path $keyFile)) { New-Item -ItemType File -Path $keyFile | Out-Null; Did "created the (empty) key file $keyFile" }
    $have = @(Get-Content $keyFile -ErrorAction SilentlyContinue | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
    $keyRe = '^(ssh-(rsa|ed25519|dss)|ecdsa-sha2-\S+|sk-\S+) \S+'
    $keys = @($AuthorizedKey | Where-Object { $_ -and $_.Trim() })
    # Only ask when nobody can log in yet. A machine that already trusts a key is not asked again.
    if ($keys.Count -eq 0 -and $have.Count -eq 0 -and -not $NoPause -and [Environment]::UserInteractive) {
        Write-Host ''
        Write-Host '    Paste the PUBLIC key of the computer that will ssh into this one'
        Write-Host '    (the one line in its id_ed25519.pub). One key per line; press Enter on an'
        Write-Host '    empty line to finish, or right away to skip this step.'
        while ($true) {
            $line = (Read-Host '    public key').Trim()
            if (-not $line) { break }
            if ($line -notmatch $keyRe) { Write-Host '    that does not look like a public key (should start with ssh-ed25519, ssh-rsa, ...); try again' -ForegroundColor Yellow; continue }
            $keys += $line
        }
    }
    if ($keys.Count -eq 0 -and $have.Count -eq 0) { Todo "no ssh key authorized, so nobody can log in yet. Re-run and paste one at the prompt, or add a public key line to $keyFile" }
    elseif ($keys.Count -eq 0) { Had "$($have.Count) ssh key(s) already authorized" }
    foreach ($k in $keys) {
        $k = $k.Trim()
        if ($k -notmatch $keyRe) { throw "not a public key: $k" }
        if ($have -contains $k) { Had "ssh key already authorized: $($k.Split(' ')[-1])"; continue }
        $raw = [IO.File]::ReadAllText($keyFile)
        $nl  = ''
        if ($raw.Length -gt 0 -and -not $raw.EndsWith("`n")) { $nl = "`r`n" }
        # ASCII on purpose: '>' in Windows PowerShell 5.1 writes UTF-16, which sshd cannot read.
        [IO.File]::AppendAllText($keyFile, $nl + $k + "`r`n", [Text.Encoding]::ASCII)
        Did "authorized ssh key: $($k.Split(' ')[-1])"
    }
    # The ACL is only touched if it is wrong: inheritance off, and no one but the intended owners.
    $acl  = Get-Acl $keyFile
    $want = if ($inAdmins) { @('BUILTIN\Administrators', 'NT AUTHORITY\SYSTEM') } else { @("$env:USERDOMAIN\$userName", 'NT AUTHORITY\SYSTEM') }
    $got  = @($acl.Access | ForEach-Object { $_.IdentityReference.Value })
    $aclOk = $acl.AreAccessRulesProtected -and -not (Compare-Object ($want | Sort-Object -Unique) ($got | Sort-Object -Unique))
    if (-not $aclOk) {
        if ($inAdmins) { icacls.exe $keyFile /inheritance:r /grant 'Administrators:F' /grant 'SYSTEM:F' | Out-Null }
        else           { icacls.exe $keyFile /inheritance:r /grant "${userName}:F" /grant 'SYSTEM:F' | Out-Null }
        Did 'fixed the permissions on the key file'
    } else { Had 'key file permissions are correct' }

    # --- 3. Visual Studio 2026 Build Tools -------------------------------------------------
    Step 'Visual Studio 2026 (MSVC + CMake + Ninja)'
    $vsw = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    function Find-VS {
        if (-not (Test-Path $vsw)) { return $null }
        # 18.x only -- Visual Studio 2026, the MSVC that CI builds with. Any edition will do
        # (Build Tools, or a Community that is already there), but an older VS on the same
        # machine must not be picked up in its place, and is left installed and untouched.
        $p = & $vsw -products * -version '[18.0,19.0)' -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($p) { return "$p".Trim() } else { return $null }
    }
    $vs = Find-VS
    $cmakeRel = 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    $vsInstalledNow = $false
    if (-not $vs -or -not (Test-Path (Join-Path $vs "$cmakeRel\CMake\bin\cmake.exe"))) {
        $vsInstalledNow = $true
        # A running Visual Studio Installer (or a VS that has it open) locks its folder and the
        # bootstrapper dies with the unhelpful exit code 5002. Say so instead of failing there.
        $viDir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
        $lockers = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($viDir, 'OrdinalIgnoreCase') })
        if ($lockers) {
            $names = ($lockers | ForEach-Object { "$($_.ProcessName) (pid $($_.Id), since $($_.StartTime))" }) -join '; '
            $ides  = @(Get-Process devenv -ErrorAction SilentlyContinue | ForEach-Object { "devenv (pid $($_.Id))" }) -join '; '
            throw "The Visual Studio Installer is running and would block the Build Tools install: $names. Close it$(if ($ides) { " and any open Visual Studio ($ides)" }) -- save your work first -- then run this again."
        }
        # An earlier uninstall leaves debris behind (a few Roslyn DLLs, empty folders), and the
        # installer refuses to install into ANY existing folder ("cannot be installed to a
        # nonempty directory", exit 1). If Visual Studio has NO instance registered at that path
        # (-all also lists half-installed ones), whatever is there is orphaned debris: stop
        # anything still running from it, then clear it. A registered instance is never touched.
        # The install path is given explicitly (--installPath below), so this is exactly where
        # it goes and the check here looks in the right place.
        $btDir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\18\BuildTools"
        $registered = @(if (Test-Path $vsw) { & $vsw -all -products * -property installationPath }) | ForEach-Object { "$_".Trim().TrimEnd('\') }
        if ((Test-Path $btDir) -and ($registered -notcontains $btDir)) {
            Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($btDir, 'OrdinalIgnoreCase') } | Stop-Process -Force
            Remove-Item $btDir -Recurse -Force
            Did "removed leftover files from an earlier uninstall: $btDir (no Visual Studio instance is registered there; the installer refuses a non-empty target)"
        }
        Note 'installing (multi-GB download, several silent minutes -- this is the slow step)'
        $setup = Join-Path $env:TEMP 'vs_BuildTools.exe'
        # vs/18/stable is Visual Studio 2026's channel. (vs/18/release does not exist; vs/17/release
        # is 2022's.)
        Fetch 'https://aka.ms/vs/18/stable/vs_BuildTools.exe' $setup
        $p = Start-Process $setup -Wait -PassThru -ArgumentList '--quiet', '--wait', '--norestart',
            '--installPath', "`"$btDir`"",
            '--add', 'Microsoft.VisualStudio.Workload.VCTools',
            '--add', 'Microsoft.VisualStudio.Component.VC.CMake.Project',
            '--includeRecommended'
        if ($p.ExitCode -eq 3010) { Note 'Build Tools installed; Windows asks for a restart at some point (exit 3010).' }
        elseif ($p.ExitCode -ne 0) { throw "Build Tools installer failed, exit $($p.ExitCode)" }
        $vs = Find-VS
    }
    if (-not $vs) { throw 'MSVC (VC.Tools.x86.x64) still not found after the install.' }
    $msvc = (Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') | Sort-Object Name | Select-Object -Last 1).Name
    if ($vsInstalledNow) { Did "installed Visual Studio 2026 Build Tools: $vs  (MSVC $msvc)" }
    else                 { Had "Visual Studio 2026: $vs  (MSVC $msvc)" }

    # --- 4. CMake + Ninja on the user PATH -------------------------------------------------
    Step 'CMake and Ninja on PATH'
    $mk = Join-Path $vs $cmakeRel
    $want = @("$mk\CMake\bin", "$mk\Ninja")
    # Another Visual Studio's bundled CMake/Ninja on the PATH (2022's, from an earlier run of this
    # script) would be found FIRST, and 2022's CMake cannot even make the VS 2026 generator. So
    # those entries come off -- only entries inside a Visual Studio's own CMake folder; nothing
    # else on the PATH is touched, and the other Visual Studio stays installed.
    $isOtherVsCMake = { param($e) $e -match '\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\' -and $want -notcontains $e }
    $u = [Environment]::GetEnvironmentVariable('Path', 'User'); if (-not $u) { $u = '' }
    $u0 = $u
    $gone = @($u -split ';' | Where-Object { $_ -and (& $isOtherVsCMake $_) })
    $u = (@($u -split ';' | Where-Object { $_ -and -not (& $isOtherVsCMake $_) }) -join ';')
    foreach ($d in $want) {
        if (($u -split ';') -notcontains $d) { $u = ($u.TrimEnd(';') + ';' + $d) }
    }
    # This run's own PATH too: drop the other VS's entries and put ours FIRST.
    $env:Path = ((@($want) + @($env:Path -split ';' | Where-Object { $_ -and -not (& $isOtherVsCMake $_) -and $want -notcontains $_ })) -join ';')
    # Written only when something changed, so a finished machine is left exactly as it was.
    if ($u -ne $u0) {
        [Environment]::SetEnvironmentVariable('Path', $u.TrimStart(';'), 'User')
        foreach ($g in $gone) { Did "took another Visual Studio's CMake/Ninja off the user PATH: $g" }
        Did 'CMake and Ninja of Visual Studio 2026 are on the user PATH (new shells and ssh sessions see them)'
    }
    else { Had 'CMake and Ninja of Visual Studio 2026 already on the user PATH' }
    Ok ((cmake --version | Select-Object -First 1) + ' / ninja ' + (ninja --version))

    # --- 5. Git for Windows ----------------------------------------------------------------
    Step 'Git for Windows'
    $gitCmd = 'C:\Program Files\Git\cmd'
    $gitInstalledNow = $false
    if (-not (Test-Path "$gitCmd\git.exe") -and -not (Get-Command git -ErrorAction SilentlyContinue)) {
        $gitInstalledNow = $true
        $rel = curl.exe -fsSL https://api.github.com/repos/git-for-windows/git/releases/latest | ConvertFrom-Json
        $asset = $rel.assets | Where-Object { $_.name -match '64-bit\.exe$' -and $_.name -notmatch 'Portable|Mini|arm' } | Select-Object -First 1
        if (-not $asset) { throw 'Could not find a Git for Windows installer.' }
        $gexe = Join-Path $env:TEMP 'git-installer.exe'
        Fetch $asset.browser_download_url $gexe
        $p = Start-Process $gexe -Wait -PassThru -ArgumentList '/VERYSILENT', '/NORESTART', '/SP-', '/SUPPRESSMSGBOXES', '/NOCANCEL'
        if ($p.ExitCode -ne 0) { throw "Git installer failed, exit $($p.ExitCode)" }
    }
    if (Test-Path "$gitCmd\git.exe") { AddToProcessPath $gitCmd }
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'git is still not available.' }
    if ($gitInstalledNow) { Did "installed $(git --version)" } else { Had "$(git --version)" }
    $bash = 'C:\Program Files\Git\bin\bash.exe'
    if (Test-Path $bash) { Note "Git Bash is at $bash  (not put on PATH on purpose: System32\bash.exe is WSL and would win or lose unpredictably)" }
    else { Todo 'Git Bash not found; tools/build-package.sh needs it.' }

    # --- 6. the repo -----------------------------------------------------------------------
    Step "repo: $RepoDir"
    if ($repoIsCheckout) {
        # The repo is the checkout this script was run from. It is used exactly as it is: no fetch,
        # no checkout, and above all its origin is NOT rewritten (a developer's checkout usually
        # pushes over ssh). Whoever drives a build does the fetch and checkout.
        Had "using the repo at $RepoDir, left exactly as it is"
        if (Test-Path (Join-Path $RepoDir '.git')) {
            # Only if git refuses the folder (owned by another account, "dubious ownership"): allow it.
            git -C $RepoDir rev-parse --git-dir 2>&1 | Out-Null
            if ($LASTEXITCODE -ne 0) {
                git config --global --add safe.directory ($RepoDir -replace '\\', '/')
                Did "told git the repo at $RepoDir is safe (it is owned by another account)"
            }
            Note "at $(git -C $RepoDir describe --tags --always)   origin = $(git -C $RepoDir remote get-url origin)"
        } else {
            Note 'no .git here (a source download, not a clone): fine for building; a release build needs a real clone (git fetch --tags, git checkout).'
        }
    } else {
        # -RepoDir named a folder that is not a checkout yet: clone there. A worker reads over
        # ANONYMOUS https and holds no credential (DISTRIBUTION.md 4.1).
        git clone $RepoUrl $RepoDir
        if ($LASTEXITCODE -ne 0) { throw 'git clone failed' }
        Did "cloned $RepoUrl into $RepoDir"
    }

    # --- 7. static SDL3 --------------------------------------------------------------------
    Step 'static SDL3'
    $sdl = Join-Path $env:USERPROFILE 'opt\sdl3-static'
    $sdlBat  = "$RepoDir\tools\build-sdl3-static.bat"
    $sdlWant = ((Select-String -Path $sdlBat -Pattern '^set SDL3_VERSION=(\S+)').Matches | Select-Object -First 1).Groups[1].Value
    $sdlHave = if (Test-Path "$sdl\.altairsim-sdl3-version") { (Get-Content "$sdl\.altairsim-sdl3-version" -TotalCount 1).Trim() } else { '' }
    if ($sdlWant -and $sdlHave -eq $sdlWant -and (Test-Path "$sdl\lib\SDL3-static.lib")) {
        Had "static SDL3 $sdlHave already in $sdl"
    } else {
        Note "building static SDL3 $sdlWant (a few minutes)"
        & cmd.exe /c "`"$sdlBat`" `"$sdl`""
        if ($LASTEXITCODE -ne 0) { throw 'build-sdl3-static.bat failed' }
        if (-not (Test-Path "$sdl\lib\SDL3-static.lib")) { throw "no SDL3-static.lib in $sdl\lib" }
        Did "built static SDL3 $(Get-Content "$sdl\.altairsim-sdl3-version" -TotalCount 1) into $sdl"
    }

    # --- 8. power --------------------------------------------------------------------------
    Step 'power plan'
    if ($SkipSleepSettings) { Note 'skipped (-SkipSleepSettings)' }
    else {
        function AcTimeout($alias) {
            $m = [regex]::Match((powercfg /q SCHEME_CURRENT SUB_SLEEP $alias | Out-String), 'Current AC Power Setting Index:\s*0x([0-9a-fA-F]+)')
            if ($m.Success) { return [Convert]::ToInt32($m.Groups[1].Value, 16) } else { return -1 }
        }
        $fixed = $false
        if ((AcTimeout 'STANDBYIDLE')   -ne 0) { powercfg /change standby-timeout-ac 0;   $fixed = $true }
        if ((AcTimeout 'HIBERNATEIDLE') -ne 0) { powercfg /change hibernate-timeout-ac 0; $fixed = $true }
        if ($fixed) { Did 'set: never sleep or hibernate on AC power (a sleeping worker cannot be reached over ssh)' }
        else        { Had 'already never sleeps or hibernates on AC power' }
    }

    # --- 9. prove it -----------------------------------------------------------------------
    Step 'configure altairsim: is SDL3 found?'
    # Without -Build the check runs in a throwaway directory that is deleted afterwards, so the
    # repo's own build\ is never touched and a finished machine is left exactly as it was.
    # With -Build it is the repo's build\ -- the whole point of that switch.
    if ($Build) { $bdir = Join-Path $RepoDir 'build' } else { $bdir = Join-Path $env:TEMP "altairsim-verify-$PID" }
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'   # PS 5.1 turns a native command's stderr into an error under 'Stop'
    $gen = 'Visual Studio 18 2026'
    $genOf = {
        $cache = Join-Path $bdir 'CMakeCache.txt'
        if (-not (Test-Path $cache)) { return '' }
        $m = Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.*)$' | Select-Object -First 1
        if ($m) { return $m.Matches[0].Groups[1].Value.Trim() } else { return '' }
    }
    # A build\ made with another generator (Visual Studio 2022's, from before this machine moved
    # to 2026) cannot be reconfigured with this one -- CMake refuses. It holds only build output,
    # so it is cleared and made again.
    $oldGen = & $genOf
    if ($oldGen -and $oldGen -ne $gen) {
        Remove-Item $bdir -Recurse -Force
        Did "cleared $bdir, which was made with '$oldGen', so it is made again with '$gen'"
    }
    try {
        $out = cmake -S $RepoDir -B $bdir -G $gen -DCMAKE_PREFIX_PATH="$sdl" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded 2>&1 | Out-String
        # -G names the compiler outright: Visual Studio 2026's, the MSVC that CI checks every PR
        # with. A CMake too old to know that generator (2022's) fails here rather than quietly
        # building with something else.
        if ($LASTEXITCODE -ne 0) { Write-Host $out; throw 'cmake configure failed' }
        Ok "generator: $gen"
        if ($out -match 'SDL3 found -- video boards enabled \(windowed\)') { Ok 'SDL3 found -- video boards enabled (windowed)' }
        else { Write-Host $out; throw 'STOP: configure did not report "SDL3 found -- video boards enabled". A headless binary is what v0.2.0 shipped.' }

        if ($Build) {
            Step 'build (single-threaded) and test'
            cmake --build $bdir --config Release
            if ($LASTEXITCODE -ne 0) { throw 'build failed' }
            $t = ctest --test-dir $bdir -C Release -LE slow 2>&1 | Out-String
            Write-Host $t
            if ($t -notmatch '100% tests passed') { throw 'ctest did not report "100% tests passed"' }
            Ok '100% tests passed'
            & (Join-Path $bdir 'Release\altairsim.exe') -n -x 'SHOW VERSION' | Select-String 'video|version' | ForEach-Object { Note $_.Line }
        } else { Note 'skipped the full build; re-run with -Build to build and run the tests here.' }
    } finally {
        $ErrorActionPreference = $prevEap
        if (-not $Build -and (Test-Path $bdir)) { Remove-Item $bdir -Recurse -Force -ErrorAction SilentlyContinue }
    }

    # --- done ------------------------------------------------------------------------------
    Step 'SUMMARY'
    if ($script:did.Count -eq 0) { Write-Host '  Changed on this machine: nothing -- it was already set up.' -ForegroundColor Green }
    else {
        Write-Host '  Changed on this machine:' -ForegroundColor Yellow
        $script:did | ForEach-Object { Write-Host "    + $_" }
    }
    if ($script:had.Count) {
        Write-Host '  Already in place (left alone):' -ForegroundColor DarkGray
        $script:had | ForEach-Object { Write-Host "    = $_" }
    }
    if ($script:todo.Count) {
        Write-Host '  Still to do:' -ForegroundColor Magenta
        $script:todo | ForEach-Object { Write-Host "    ! $_" }
    }
    Step 'READY'
    $ips = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object { $_.IPAddress -notmatch '^(127\.|169\.254\.)' } | Select-Object -ExpandProperty IPAddress
    Write-Host "  From the other computer:   ssh $userName@$($ips | Select-Object -First 1)"
    Write-Host "  All addresses: $($ips -join ', ')   (DHCP addresses can change -- use a reservation or the hostname $env:COMPUTERNAME)"
    Write-Host "  Repo: $RepoDir     SDL3 prefix: $sdl"
    Write-Host "  Build: cmake -B build -G `"Visual Studio 18 2026`" -DCMAKE_PREFIX_PATH=`"$sdl`" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ; cmake --build build --config Release"
    Write-Host "  Log: $log"
}
catch {
    $script:failed = $true
    Write-Host "`nFAILED: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Fix the cause and run this again; it picks up where it left off. Log: $log"
}
finally {
    try { Stop-Transcript | Out-Null } catch { }
}
if (-not $NoPause -and [Environment]::UserInteractive) { Read-Host 'Press Enter to close' }
if ($script:failed) { exit 1 }
