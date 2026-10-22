# Drive player 2 through licoop_cmd.txt. Usage:
#   p2.ps1 at x y b                    set position and button
#   p2.ps1 drag x0 y0 x1 y1 [steps]    press at (x0,y0), move to (x1,y1), keep holding
#   p2.ps1 off                         remove the file (hand back to the pad)
param([string]$Op, [Parameter(ValueFromRemainingArguments = $true)][double[]]$A)
$c = "$PSScriptRoot\..\..\li-test\licoop_cmd.txt"
$inv = [Globalization.CultureInfo]::InvariantCulture
function Put($x, $y, $b) { [IO.File]::WriteAllText($c, [string]::Format($inv, "{0:F4} {1:F4} {2}", $x, $y, $b)) }
switch ($Op) {
  "at" { Put $A[0] $A[1] ([int]$A[2]) }
  "drag" {
    $n = if ($A.Count -gt 4) { [int]$A[4] } else { 15 }
    Put $A[0] $A[1] 0; Start-Sleep -Milliseconds 300
    Put $A[0] $A[1] 1; Start-Sleep -Milliseconds 300
    for ($i = 1; $i -le $n; $i++) {
      $t = $i / $n
      Put ($A[0] + ($A[2] - $A[0]) * $t) ($A[1] + ($A[3] - $A[1]) * $t) 1
      Start-Sleep -Milliseconds 60
    }
  }
  "off" { Remove-Item $c -ErrorAction SilentlyContinue }
}
