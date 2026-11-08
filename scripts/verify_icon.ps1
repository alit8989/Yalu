Add-Type -AssemblyName System.Drawing

$exePath = (Resolve-Path ".\yalu.exe").Path
Write-Host "Checking executable: $exePath"

$icon = [System.Drawing.Icon]::ExtractAssociatedIcon($exePath)
if ($icon -ne $null) {
    Write-Host "SUCCESS: Windows successfully extracted application icon from yalu.exe!"
    Write-Host "Icon dimensions: $($icon.Width) x $($icon.Height)"
    $bmp = $icon.ToBitmap()
    $outPng = "c:\Users\opuntake\Documents\yalu\extracted_icon.png"
    $bmp.Save($outPng, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "Exported extracted icon preview to: $outPng"
    $bmp.Dispose()
    $icon.Dispose()
} else {
    Write-Host "FAILURE: No icon found in $exePath"
}
