param([string]$OutputDirectory = (Join-Path $PSScriptRoot '..\materials\textures'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($outputPath) | Out-Null

# Two shared 8x8 atlases: no per-creature texture or material allocation.
foreach($kind in @('Experience', 'Recovery')) {
    $bitmap = New-Object Drawing.Bitmap 1024, 1024
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([Drawing.Color]::Transparent)
    $gold = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(255, 249, 195, 69)), 5
    $rim = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(210, 57, 32, 12)), 8
    $clock = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(165, 255, 235, 171))
    $shade = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(115, 55, 41, 25))
    for($frame = 0; $frame -lt 64; ++$frame) {
        $x = ($frame % 8) * 128
        $y = [Math]::Floor($frame / 8) * 128
        if($kind -eq 'Experience') {
            $rect = New-Object Drawing.RectangleF ($x + 39), ($y + 39), 50, 50
            $graphics.DrawEllipse($rim, $rect)
            if($frame -gt 0) { $graphics.DrawArc($gold, $rect, -90, (360.0 * $frame / 63)) }
        } elseif($frame -gt 0) {
            $rect = New-Object Drawing.Rectangle ($x + 43), ($y + 43), 42, 42
            $graphics.FillEllipse($shade, $rect)
            if($frame -gt 1) { $graphics.FillPie($clock, $rect, -90, (360.0 * ($frame - 1) / 62)) }
        }
    }
    $bitmap.Save((Join-Path $outputPath "Creature$kind.png"), [Drawing.Imaging.ImageFormat]::Png)
    $gold.Dispose(); $rim.Dispose(); $clock.Dispose(); $shade.Dispose()
    $graphics.Dispose(); $bitmap.Dispose()
}
