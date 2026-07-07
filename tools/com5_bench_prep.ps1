param(
    [string]$PortHint = "COM5",
    [switch]$StopKnownMonitors
)

$knownProcessPatterns = @(
    "idf-monitor",
    "python",
    "Code",
    "cursor",
    "putty",
    "ttermpro",
    "openocd"
)

Write-Host "=== Serial Ports ==="
try {
    Get-PnpDevice -Class Ports -ErrorAction Stop | Select-Object FriendlyName, Status
} catch {
    Write-Warning "Get-PnpDevice falhou: $($_.Exception.Message)"
    Write-Host "Fallback: use 'mode' para inspecao basica de portas."
    cmd.exe /c mode
}

Write-Host ""
Write-Host "=== Candidate Processes ==="
$processes = Get-Process | Where-Object {
    $name = $_.ProcessName
    foreach ($pattern in $knownProcessPatterns) {
        if ($name -like "*$pattern*") {
            return $true
        }
    }
    return $false
} | Select-Object ProcessName, Id, Path

if ($processes) {
    $processes
} else {
    Write-Host "No obvious serial-monitor processes found."
}

Write-Host ""
Write-Host "=== Bench Checklist ==="
Write-Host "1. Keep only one serial client attached to $PortHint."
Write-Host "2. Close VS Code / Cursor serial monitors before flash or monitor."
Write-Host "3. Start TCP helpers (fake server, logs) separately; they do not need $PortHint."
Write-Host "4. If $PortHint still denies access, rerun this script with -StopKnownMonitors."

if (-not $StopKnownMonitors) {
    return
}

Write-Host ""
Write-Host "=== Stopping Known Monitors ==="
$stopped = $false
foreach ($proc in $processes) {
    if ($null -eq $proc.Id) {
        continue
    }

    try {
        Stop-Process -Id $proc.Id -Force -ErrorAction Stop
        Write-Host "Stopped $($proc.ProcessName) pid=$($proc.Id)"
        $stopped = $true
    } catch {
        Write-Warning "Could not stop $($proc.ProcessName) pid=$($proc.Id): $($_.Exception.Message)"
    }
}

if (-not $stopped) {
    Write-Host "No processes were stopped."
}
