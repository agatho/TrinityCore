# install.ps1 — Trinity Housing master SQL installer (Windows / PowerShell)
#
# Runs the three MASTER_housing_*.sql files against the matching databases in
# the correct order. Exits non-zero on the first failure.
#
# Usage:
#     & '.\sql\housing\master\install.ps1' -User <user> -Password <password>
#
# Optional parameters:
#     -MysqlPath     full path to mysql.exe (default: looks in PATH, then the
#                    standard "C:\Program Files\MySQL\MySQL Server 9.4\bin\mysql.exe")
#     -CharactersDb  name of the characters database (default: characters)
#     -HotfixesDb    name of the hotfixes database   (default: hotfixes)
#     -WorldDb       name of the world database      (default: world)
#     -Host          MySQL host (default: localhost)
#     -Port          MySQL port (default: 3306)

param(
    [Parameter(Mandatory=$true)] [string]$User,
    [Parameter(Mandatory=$true)] [string]$Password,
    [string]$MysqlPath = '',
    [string]$CharactersDb = 'characters',
    [string]$HotfixesDb   = 'hotfixes',
    [string]$WorldDb      = 'world',
    [string]$DbHost       = 'localhost',
    [int]   $Port         = 3306
)

$ErrorActionPreference = 'Stop'

# Resolve mysql.exe
if (-not $MysqlPath) {
    $candidate = Get-Command mysql.exe -ErrorAction SilentlyContinue
    if ($candidate) {
        $MysqlPath = $candidate.Source
    } else {
        $fallback = 'C:\Program Files\MySQL\MySQL Server 9.4\bin\mysql.exe'
        if (Test-Path $fallback) {
            $MysqlPath = $fallback
        } else {
            Write-Error "mysql.exe not found. Pass -MysqlPath or add it to PATH."
            exit 1
        }
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$charsFile = Join-Path $scriptDir 'MASTER_housing_characters.sql'
$hfFile    = Join-Path $scriptDir 'MASTER_housing_hotfixes.sql'
$worldFile = Join-Path $scriptDir 'MASTER_housing_world.sql'

foreach ($f in @($charsFile, $hfFile, $worldFile)) {
    if (-not (Test-Path $f)) {
        Write-Error "Master SQL file missing: $f. Regenerate with build.sh."
        exit 1
    }
}

function Invoke-MysqlFile {
    param([string]$Database, [string]$SqlFile)

    Write-Host "==> Installing $SqlFile into database '$Database'" -ForegroundColor Cyan
    $args = @(
        "--host=$DbHost",
        "--port=$Port",
        "--user=$User",
        "--password=$Password",
        "--database=$Database",
        "--default-character-set=utf8mb4"
    )

    $proc = Start-Process -FilePath $MysqlPath -ArgumentList $args `
        -RedirectStandardInput $SqlFile `
        -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        Write-Error "mysql.exe exited with code $($proc.ExitCode) on $SqlFile"
        exit $proc.ExitCode
    }
}

Invoke-MysqlFile -Database $CharactersDb -SqlFile $charsFile
Invoke-MysqlFile -Database $HotfixesDb   -SqlFile $hfFile
Invoke-MysqlFile -Database $WorldDb      -SqlFile $worldFile

Write-Host ""
Write-Host "Housing master bundle installed successfully." -ForegroundColor Green
