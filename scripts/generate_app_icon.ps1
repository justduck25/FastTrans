Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$out = Join-Path $repoRoot "assets\app_icon.ico"
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngs = @()

foreach ($iconSize in $sizes) {
    $script:iconScale = [float]$iconSize

    function Scale([float]$value) {
        return [single]($value * $script:iconScale / 1024.0)
    }

    function RoundedRect([float]$x, [float]$y, [float]$width, [float]$height, [float]$radius) {
        $path = New-Object System.Drawing.Drawing2D.GraphicsPath
        $diameter = (Scale $radius) * 2
        $path.AddArc((Scale $x), (Scale $y), $diameter, $diameter, 180, 90)
        $path.AddArc((Scale ($x + $width - 2 * $radius)), (Scale $y), $diameter, $diameter, 270, 90)
        $path.AddArc((Scale ($x + $width - 2 * $radius)), (Scale ($y + $height - 2 * $radius)), $diameter, $diameter, 0, 90)
        $path.AddArc((Scale $x), (Scale ($y + $height - 2 * $radius)), $diameter, $diameter, 90, 90)
        $path.CloseFigure()
        return $path
    }

    $bitmap = New-Object System.Drawing.Bitmap $iconSize, $iconSize, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    $backgroundBounds = [System.Drawing.RectangleF]::new((Scale 96), (Scale 96), (Scale 832), (Scale 832))
    $backgroundPath = RoundedRect 96 96 832 832 224
    $backgroundBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
        $backgroundBounds,
        [System.Drawing.Color]::FromArgb(255, 31, 182, 255),
        [System.Drawing.Color]::FromArgb(255, 255, 93, 184),
        45
    )
    $graphics.FillPath($backgroundBrush, $backgroundPath)

    $cardPath = RoundedRect 192 282 594 432 106
    $cardBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 255, 255, 255))
    $graphics.FillPath($cardBrush, $cardPath)

    $tailPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $tailPath.AddPolygon(@(
        [System.Drawing.PointF]::new((Scale 365), (Scale 682)),
        [System.Drawing.PointF]::new((Scale 365), (Scale 812)),
        [System.Drawing.PointF]::new((Scale 505), (Scale 682))
    ))
    $graphics.FillPath($cardBrush, $tailPath)

    $darkPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 13, 35, 55), (Scale 56))
    $darkPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $darkPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pinkPen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255, 255, 79, 168), (Scale 48))
    $pinkPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pinkPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round

    $graphics.DrawLine($darkPen, (Scale 360), (Scale 404), (Scale 494), (Scale 404))
    $graphics.DrawLine($darkPen, (Scale 427), (Scale 404), (Scale 427), (Scale 606))
    $graphics.DrawLine($darkPen, (Scale 344), (Scale 606), (Scale 510), (Scale 606))
    $graphics.DrawLine($pinkPen, (Scale 598), (Scale 408), (Scale 704), (Scale 408))
    $graphics.DrawLine($pinkPen, (Scale 612), (Scale 502), (Scale 740), (Scale 502))
    $graphics.DrawBezier($pinkPen, (Scale 652), (Scale 408), (Scale 646), (Scale 499), (Scale 606), (Scale 573), (Scale 558), (Scale 606))
    $graphics.DrawBezier($pinkPen, (Scale 704), (Scale 502), (Scale 684), (Scale 558), (Scale 644), (Scale 614), (Scale 582), (Scale 650))

    $dotBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 13, 35, 55))
    $graphics.FillEllipse($dotBrush, (Scale 686), (Scale 260), (Scale 108), (Scale 108))
    $shineBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(235, 255, 255, 255))
    $graphics.FillEllipse($shineBrush, (Scale 744), (Scale 276), (Scale 32), (Scale 32))

    $stream = New-Object System.IO.MemoryStream
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += ,$stream.ToArray()
    $graphics.Dispose()
    $bitmap.Dispose()
}

$fileStream = [System.IO.File]::Create($out)
$writer = New-Object System.IO.BinaryWriter $fileStream
$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]$sizes.Count)

$offset = 6 + (16 * $sizes.Count)
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $size = $sizes[$i]
    $dimension = if ($size -eq 256) { 0 } else { $size }
    $writer.Write([byte]$dimension)
    $writer.Write([byte]$dimension)
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]32)
    $writer.Write([UInt32]$pngs[$i].Length)
    $writer.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}

foreach ($png in $pngs) {
    $writer.Write($png)
}

$writer.Close()
$fileStream.Close()
Get-Item $out | Select-Object FullName, Length
