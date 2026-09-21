param(
    [string]$TextureDirectory = (Join-Path $PSScriptRoot '..\..\materials\textures')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$failures = 0
$checks = 0
$full = [System.Drawing.Bitmap]::new((Join-Path $TextureDirectory 'CreatureOverlay0.png'))
try {
    for ($state = 0; $state -lt 8; ++$state) {
        $bitmap = [System.Drawing.Bitmap]::new((Join-Path $TextureDirectory "CreatureOverlay$state.png"))
        try {
            $remaining = @(8, 7, 6, 5, 3, 2, 1, 0)[$state]
            for ($segment = 0; $segment -lt 8; ++$segment) {
                # Sample the fill and both border bands, not just the fill opacity.
                $angle = (-67.5 + $segment * 45) * [Math]::PI / 180
                foreach ($radius in 35, 45, 55) {
                    $x = [int][Math]::Round(64 + $radius * [Math]::Cos($angle))
                    $y = [int][Math]::Round(64 + $radius * [Math]::Sin($angle))
                    $pixel = $bitmap.GetPixel($x, $y)
                    $checks++
                    $valid = if ($segment -lt $remaining) {
                        $pixel.ToArgb() -eq $full.GetPixel($x, $y).ToArgb() -and $pixel.A -gt 200
                    } else {
                        $pixel.A -eq 0
                    }
                    if (-not $valid) {
                        $failures++
                        Write-Output "FAIL state=$state segment=$segment radius=$radius alpha=$($pixel.A)"
                    }
                }
            }
            # The centre remains readable and unchanged at every health level.
            $checks++
            $centreMatches = $true
            for ($y = 38; $y -le 90; ++$y) {
                for ($x = 38; $x -le 90; ++$x) {
                    if (($x - 64) * ($x - 64) + ($y - 64) * ($y - 64) -gt 26 * 26) { continue }
                    if ($bitmap.GetPixel($x, $y).ToArgb() -ne $full.GetPixel($x, $y).ToArgb()) {
                        $centreMatches = $false
                    }
                }
            }
            if (-not $centreMatches) { $failures++; Write-Output "FAIL centre changed in state=$state" }
        } finally { $bitmap.Dispose() }
    }
} finally { $full.Dispose() }
Write-Output "$checks checks, $failures failures"
if ($failures) { exit 1 }
