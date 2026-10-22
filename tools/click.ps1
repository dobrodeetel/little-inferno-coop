# Move the real mouse and left-click at screen coordinates. Usage: click.ps1 x y [x y ...]
param([Parameter(ValueFromRemainingArguments = $true)][int[]]$Coords)
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class M {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, UIntPtr e);
}
"@
for ($i = 0; $i -lt $Coords.Count; $i += 2) {
    [M]::SetCursorPos($Coords[$i], $Coords[$i + 1]) | Out-Null
    Start-Sleep -Milliseconds 300
    [M]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [M]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 2500
}
