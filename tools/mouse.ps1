# Real mouse helper. Usage: mouse.ps1 move x y | down | up
param([string]$Op, [int]$X, [int]$Y)
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class M2 {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, UIntPtr e);
}
"@
switch ($Op) {
  "move" { [M2]::SetCursorPos($X, $Y) | Out-Null }
  "down" { [M2]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero) }
  "up"   { [M2]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero) }
}
