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
    $depletedPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(210, 48, 48, 48)), 17
    $segmentBorderPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(238, 18, 18, 18)), 23
    $ringRect = New-Object System.Drawing.RectangleF -ArgumentList 19, 19, 90, 90

    for($segment = 0; $segment -lt 8; ++$segment) {
        $startAngle = -87 + ($segment * 45)
        $sweepAngle = 39
        $graphics.DrawArc($segmentBorderPen, $ringRect, $startAngle, $sweepAngle)
        if($segment -lt $remainingSegments) {
            $graphics.DrawArc($healthyPen, $ringRect, $startAngle, $sweepAngle)
        }
        else {
            $graphics.DrawArc($depletedPen, $ringRect, $startAngle, $sweepAngle)
        }
    }

    $centreBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(232, 220, 220, 220))
    $centrePen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(238, 18, 18, 18)), 4
    $graphics.FillEllipse($centreBrush, 37, 37, 54, 54)
    $graphics.DrawEllipse($centrePen, 37, 37, 54, 54)

    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $centrePen.Dispose()
    $centreBrush.Dispose()
    $segmentBorderPen.Dispose()
    $depletedPen.Dispose()
    $healthyPen.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}

function New-UnhappyIcon([string]$path) {
    $bitmap = New-Object System.Drawing.Bitmap 32, 32,
        ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $faceBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
    $inkBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::Black)
    $inkPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 2
    $graphics.FillEllipse($faceBrush, 1, 1, 29, 29)
    $graphics.DrawEllipse($inkPen, 1, 1, 29, 29)
    $graphics.FillEllipse($inkBrush, 9, 10, 3, 4)
    $graphics.FillEllipse($inkBrush, 20, 10, 3, 4)
    $graphics.DrawArc($inkPen, 9, 17, 14, 9, 200, 140)
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $inkPen.Dispose()
    $inkBrush.Dispose()
    $faceBrush.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}

for($state = 0; $state -lt 8; ++$state) {
    New-HealthRing $state (Join-Path $outputPath "CreatureOverlay$state.png")
}
New-UnhappyIcon (Join-Path $outputPath 'CreatureUnhappy.png')

Write-Output "Generated eight segmented health rings and one unhappy status icon in $outputPath"
