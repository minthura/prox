<#
.SYNOPSIS
    Downloads all u8g2 csrc files from GitHub into
    components/espu8g2/csrc relative to this script's location.

.DESCRIPTION
    Run once after cloning / setting up the project.
    Requires an internet connection and PowerShell 5.1+.

.EXAMPLE
    .\fetch_u8g2_csrc.ps1
#>

$ErrorActionPreference = "Stop"

$REPO_API = "https://api.github.com/repos/olikraus/u8g2/contents/csrc"
$RAW_BASE = "https://raw.githubusercontent.com/olikraus/u8g2/master/csrc"

# Destination: csrc/ sibling of this script
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$DestDir   = Join-Path $ScriptDir "csrc"

if (-not (Test-Path $DestDir)) {
    New-Item -ItemType Directory -Path $DestDir | Out-Null
    Write-Host "Created directory: $DestDir"
}

Write-Host "Fetching file list from GitHub API..."
$items = Invoke-RestMethod -Uri $REPO_API -UseBasicParsing

$total   = $items.Count
$current = 0

foreach ($item in $items) {
    $current++
    $dest = Join-Path $DestDir $item.name

    if (Test-Path $dest) {
        Write-Host "[$current/$total] Skip (exists): $($item.name)"
        continue
    }

    Write-Host "[$current/$total] Downloading: $($item.name)"
    $url = "$RAW_BASE/$($item.name)"
    Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
}

Write-Host ""
Write-Host "Done. $total files in $DestDir"
Write-Host "You can now run: idf.py build"
