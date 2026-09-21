param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\materials\textures')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($outputPath) | Out-Null

function New-HealthRing([int]$healthState, [string]$path) {
    $bitmap = New-Object System.Drawing.Bitmap 128, 128,
        ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([System.Drawing.Color]::Transparent)

    $remainingSegments = @(8, 7, 6, 5, 3, 2, 1, 0)[$healthState]
    $healthyPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(232, 220, 220, 220)), 17
    $segmentBorderPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(238, 18, 18, 18)), 23
    $ringRect = New-Object System.Drawing.RectangleF -ArgumentList 19, 19, 90, 90

    # Depleted segments leave the scene visible, including where their borders were.
    for($segment = 0; $segment -lt $remainingSegments; ++$segment) {
        $startAngle = -87 + ($segment * 45)
        $sweepAngle = 39
        $graphics.DrawArc($segmentBorderPen, $ringRect, $startAngle, $sweepAngle)
        $graphics.DrawArc($healthyPen, $ringRect, $startAngle, $sweepAngle)
    }

    $centreBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(232, 220, 220, 220))
    $centrePen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(238, 18, 18, 18)), 4
    $graphics.FillEllipse($centreBrush, 37, 37, 54, 54)
    $graphics.DrawEllipse($centrePen, 37, 37, 54, 54)

    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $centrePen.Dispose()
    $centreBrush.Dispose()
    $segmentBorderPen.Dispose()
    $healthyPen.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}

for($state = 0; $state -lt 8; ++$state) {
    New-HealthRing $state (Join-Path $outputPath "CreatureOverlay$state.png")
}
Write-Output "Generated eight segmented health rings in $outputPath; mood artwork is packaged separately."
