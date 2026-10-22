# Screenshot of the primary screen. Usage: shot.ps1 <out.png> [scale]
param([string]$Out, [double]$Scale = 1.0)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$b = [Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
$g = [Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Location, [Drawing.Point]::Empty, $b.Size)
if ($Scale -ne 1.0) {
    $s = New-Object Drawing.Bitmap ([int]($b.Width * $Scale)), ([int]($b.Height * $Scale))
    [Drawing.Graphics]::FromImage($s).DrawImage($bmp, 0, 0, $s.Width, $s.Height)
    $bmp = $s
}
$bmp.Save($Out)
