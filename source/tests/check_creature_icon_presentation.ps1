$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$names = @('Angry','Furious','Paid','Leaving','KO','Hungry','Sleepy','Stunned','Prisoner','RallyTroops','Unhappy')
$checks = 0
$hashes = @{}
foreach($name in $names) {
    $path = Join-Path $root "materials\textures\Creature$name.png"
    $bitmap = [Drawing.Bitmap]::FromFile($path)
    try {
        $checks++
        if($bitmap.Width -ne 64 -or $bitmap.Height -ne 64) { throw "Wrong dimensions: $name" }
        $transparent = 0; $coloured = 0; $visible = 0
        for($y = 0; $y -lt 64; ++$y) { for($x = 0; $x -lt 64; ++$x) {
            $pixel = $bitmap.GetPixel($x,$y)
            if($pixel.A -eq 0) { ++$transparent }
            if($pixel.A -gt 160) {
                ++$visible
                if([Math]::Max($pixel.R,[Math]::Max($pixel.G,$pixel.B)) - [Math]::Min($pixel.R,[Math]::Min($pixel.G,$pixel.B)) -gt 20) { ++$coloured }
            }
        } }
        $checks += 3
        if($transparent -lt 800 -or $visible -lt 450 -or $coloured -lt 200) { throw "Bad silhouette/colour coverage: $name" }
        foreach($point in @(@(0,0),@(63,0),@(0,63),@(63,63))) {
            ++$checks
            if($bitmap.GetPixel($point[0],$point[1]).A -ne 0) { throw "Opaque corner: $name" }
        }
    } finally { $bitmap.Dispose() }
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    ++$checks
    if($hashes.ContainsKey($hash)) { throw "Duplicate mood artwork: $name" }
    $hashes[$hash] = $name
}
$materials = Get-Content -Raw (Join-Path $root 'materials\scripts\CreatureOverlayStatus.material')
foreach($name in $names) {
    ++$checks
    if(-not $materials.Contains("texture Creature$name.png")) { throw "Mood is not wired to its texture: $name" }
}
Write-Output "$checks checks, 0 failures"
