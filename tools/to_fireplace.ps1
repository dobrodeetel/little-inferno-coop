# Start the test copy and go to the fireplace of the second classic slot, burning the
# tutorial sign so the item tray opens. The menus take player 1 only (the real mouse); player 2
# (licoop_cmd.txt) lights the fire. Usage: to_fireplace.ps1 [-NoStart] [-KeepSign]
param([switch]$NoStart, [switch]$KeepSign)
$Game = "$PSScriptRoot\..\..\li-test"
$click = "$PSScriptRoot\click.ps1"
$p2 = "$PSScriptRoot\p2.ps1"
function Click2($x, $y) {
    & $p2 at $x $y 0; Start-Sleep -Milliseconds 400
    & $p2 at $x $y 1; Start-Sleep -Milliseconds 500
    & $p2 at $x $y 0
}
if (-not $NoStart) {
    & "$PSScriptRoot\start_game.ps1"
    Start-Sleep 10     # the warning stays until clicked
    & $click 700 400   # warning
    Start-Sleep 10     # Tomorrow Corporation, then the match lights the title
    & $click 746 360   # title
    Start-Sleep 5
}
& $click 450 400   # classic
Start-Sleep 7
& $click 710 360   # slot 2
Start-Sleep 8
if (-not $KeepSign) {
    & $p2 at 0.665 0.71 1; Start-Sleep 10   # fire under the sign
    & $p2 at 0.665 0.71 0; Start-Sleep 9
}
