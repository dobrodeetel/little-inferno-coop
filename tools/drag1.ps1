# Player 1 (real mouse) drag in one process: press at (x0,y0), move to (x1,y1) in steps.
# Usage: drag1.ps1 x0 y0 x1 y1 [steps] [-Hold]  (-Hold keeps the button down at the end)
param([int]$X0, [int]$Y0, [int]$X1, [int]$Y1, [int]$Steps = 12, [switch]$Hold)
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class M3 {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, UIntPtr e);
}
"@
[M3]::SetCursorPos($X0, $Y0) | Out-Null; Start-Sleep -Milliseconds 300
[M3]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 200
for ($i = 1; $i -le $Steps; $i++) {
    [M3]::SetCursorPos([int]($X0 + ($X1 - $X0) * $i / $Steps), [int]($Y0 + ($Y1 - $Y0) * $i / $Steps)) | Out-Null
    Start-Sleep -Milliseconds 50
}
if (-not $Hold) { Start-Sleep -Milliseconds 300; [M3]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero) }
