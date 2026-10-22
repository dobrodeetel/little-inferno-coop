# Stop any running copy (and wait until it is gone), then start the test copy.
# Usage: start_game.ps1 [-Shots] [-KeepSaves]
#   -Shots      screenshots 6..21 s after start (timing probe)
#   -KeepSaves  keep the test profile (by default it is wiped, so every run starts the same)
param([switch]$Shots, [switch]$KeepSaves)
$Game = "$PSScriptRoot\..\..\li-test"
Get-Process "Little Inferno" -ErrorAction SilentlyContinue | Stop-Process -Force
while (Get-Process "Little Inferno" -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 200 }
if (-not $KeepSaves) {
    $saves = "$PSScriptRoot\..\..\li-saves-test\Little Inferno"
    Get-ChildItem $saves -Exclude settings.txt -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
}
& "$PSScriptRoot\p2.ps1" at 0.5 0.9 0
$p = Start-Process -FilePath "$Game\Little Inferno.exe" -WorkingDirectory $Game -PassThru
if ($Shots) {
    foreach ($s in 6, 9, 12, 15, 18, 21) {
        while ((Get-Date) -lt $p.StartTime.AddSeconds($s)) { Start-Sleep -Milliseconds 100 }
        & $PSScriptRoot\shot.ps1 "$PSScriptRoot\..\..\li-shots\t$s.png" 0.2
    }
}
