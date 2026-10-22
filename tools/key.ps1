# Press keys by virtual-key code, or type text. Usage: key.ps1 vk 0xC0 | key.ps1 text "coins 500"
param([string]$Op, [string]$Arg)
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class K {
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, UIntPtr e);
  [DllImport("user32.dll")] public static extern short VkKeyScanW(char c);
}
"@
function Tap([int]$vk, [bool]$shift) {
  if ($shift) { [K]::keybd_event(0x10, 0, 0, [UIntPtr]::Zero) }
  [K]::keybd_event([byte]$vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 40
  [K]::keybd_event([byte]$vk, 0, 2, [UIntPtr]::Zero)
  if ($shift) { [K]::keybd_event(0x10, 0, 2, [UIntPtr]::Zero) }
  Start-Sleep -Milliseconds 40
}
if ($Op -eq "vk") { Tap ([Convert]::ToInt32($Arg, 16)) $false }
if ($Op -eq "text") { foreach ($ch in $Arg.ToCharArray()) { $s = [K]::VkKeyScanW($ch); Tap ($s -band 0xff) (($s -band 0x100) -ne 0) }; Tap 0x0D $false }
