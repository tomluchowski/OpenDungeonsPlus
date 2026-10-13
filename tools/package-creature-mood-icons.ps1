param(
    [string]$Atlas = (Join-Path $PSScriptRoot 'artwork\CreatureMoodAtlas.png'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\materials\textures')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$source = [Drawing.Bitmap]::FromFile([IO.Path]::GetFullPath($Atlas))
try {
    if($source.Width % 4 -ne 0 -or $source.Height % 3 -ne 0 -or $source.Width / 4 -ne $source.Height / 3) {
        throw 'Expected a four-column, three-row atlas with square cells'
    }
    $cell = [int]($source.Width / 4)
    $names = @('Angry','Furious','Paid','Leaving','KO','Hungry','Sleepy','Stunned','Prisoner','RallyTroops','Unhappy')
    [IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($OutputDirectory)) | Out-Null
    for($index = 0; $index -lt $names.Count; ++$index) {
        $bitmap = New-Object Drawing.Bitmap 64, 64, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $rect = New-Object Drawing.RectangleF (($index % 4) * $cell), ([Math]::Floor($index / 4) * $cell), $cell, $cell
            $graphics.DrawImage($source, (New-Object Drawing.RectangleF 0, 0, 64, 64), $rect, [Drawing.GraphicsUnit]::Pixel)
            $bitmap.Save((Join-Path $OutputDirectory ('Creature' + $names[$index] + '.png')), [Drawing.Imaging.ImageFormat]::Png)
        } finally { $graphics.Dispose(); $bitmap.Dispose() }
    }
} finally { $source.Dispose() }
Write-Output 'Packaged eleven coloured creature-mood icons, preserving transparent alpha.'
