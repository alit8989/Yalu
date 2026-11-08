Add-Type -AssemblyName System.Drawing

$srcPath = "c:\Users\opuntake\Documents\yalu\yalu_logo.png"
$src = [System.Drawing.Bitmap]::FromFile($srcPath)
Write-Host "Source image size: $($src.Width)x$($src.Height)"

# Content box: 759x470 centered at (550, 340)
# Use a square box around the content with 6% margin
$boxSize = 840.0
$srcX = [float](550.0 - ($boxSize / 2.0))
$srcY = [float](340.0 - ($boxSize / 2.0))
$srcRect = [System.Drawing.RectangleF]::new($srcX, $srcY, [float]$boxSize, [float]$boxSize)

$sizes = @(16, 32, 48, 64, 128, 256)
$pngBytesList = @()

foreach ($sz in $sizes) {
    $bmp = [System.Drawing.Bitmap]::new([int]$sz, [int]$sz, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $destRect = [System.Drawing.RectangleF]::new(0.0, 0.0, [float]$sz, [float]$sz)
    $g.DrawImage($src, $destRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()

    $ms = [System.IO.MemoryStream]::new()
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $ms.ToArray()
    $ms.Dispose()
    $bmp.Dispose()

    $pngBytesList += ,@($sz, $bytes)
    Write-Host "Rendered size: ${sz}x${sz} ($($bytes.Length) bytes)"
}
$src.Dispose()

# Now write ICO file
# ICONDIR: idReserved (2), idType (2), idCount (2)
$icoMs = [System.IO.MemoryStream]::new()
$bw = [System.IO.BinaryWriter]::new($icoMs)

$bw.Write([uint16]0) # Reserved
$bw.Write([uint16]1) # 1 = Icon
$bw.Write([uint16]$sizes.Count) # Number of images

$offset = 6 + ($sizes.Count * 16)

# Write ICONDIRENTRY for each image
foreach ($item in $pngBytesList) {
    $sz = $item[0]
    $data = $item[1]

    $w = if ($sz -eq 256) { [byte]0 } else { [byte]$sz }
    $h = if ($sz -eq 256) { [byte]0 } else { [byte]$sz }

    $bw.Write([byte]$w)
    $bw.Write([byte]$h)
    $bw.Write([byte]0)   # Color count (0 = >=8bpp)
    $bw.Write([byte]0)   # Reserved
    $bw.Write([uint16]1) # Planes
    $bw.Write([uint16]32) # Bit count
    $bw.Write([uint32]$data.Length) # Image size in bytes
    $bw.Write([uint32]$offset)      # Image offset

    $offset += $data.Length
}

# Write image data
foreach ($item in $pngBytesList) {
    $data = $item[1]
    $bw.Write($data)
}

$bw.Flush()
$icoPath = "c:\Users\opuntake\Documents\yalu\yalu.ico"
[System.IO.File]::WriteAllBytes($icoPath, $icoMs.ToArray())
$bw.Dispose()
$icoMs.Dispose()

$fileInfo = [System.IO.FileInfo]::new($icoPath)
Write-Host "Successfully generated ICO file at $icoPath ($($fileInfo.Length) bytes)"
