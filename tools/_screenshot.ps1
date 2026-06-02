# Capture the PokemonStadiumRecomp window to a PNG.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools/_screenshot.ps1 <out_path>
#
# Falls back to full-screen capture if window can't be found.

param(
    [string]$OutPath = "build/_window_capture.png",
    [int]$ProcessId = 0,
    [switch]$NoPrintWindow
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

# Win32 helpers
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll", CharSet=CharSet.Auto)]
    public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder lpString, int nMaxCount);
    [DllImport("user32.dll")]
    public static extern int GetWindowTextLength(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT pt);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern bool IsIconic(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")]
    public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }
}
"@

$found = [IntPtr]::Zero
$cb = [Win32+EnumWindowsProc]{ param($h, $l)
    if (-not [Win32]::IsWindowVisible($h)) { return $true }
    $len = [Win32]::GetWindowTextLength($h)
    if ($len -lt 1) { return $true }
    $sb = New-Object System.Text.StringBuilder ($len + 1)
    [Win32]::GetWindowText($h, $sb, $sb.Capacity) | Out-Null
    $title = $sb.ToString()
    $procId = 0
    [Win32]::GetWindowThreadProcessId($h, [ref]$procId) | Out-Null
    $procName = ''
    try {
        $procName = (Get-Process -Id $procId -ErrorAction Stop).ProcessName
    } catch {
        $procName = ''
    }
    if ($ProcessId -ne 0 -and [int]$procId -ne $ProcessId) {
        return $true
    }
    if ($procName -eq 'PokemonStadiumRecomp' -or $title -match 'PokemonStadiumRecomp|Pok.MON.*STADIUM') {
        $script:found = $h
        return $false
    }
    return $true
}
[Win32]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null

if ($found -eq [IntPtr]::Zero) {
    Write-Host "Could not find PokemonStadiumRecomp window; capturing primary screen instead."
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
} else {
    if ([Win32]::IsIconic($found)) {
        [Win32]::ShowWindow($found, 9) | Out-Null # SW_RESTORE
        Start-Sleep -Milliseconds 250
    }

    # Capture the full window rect via PrintWindow so we get window contents
    # even if another window is in front. PW_RENDERFULLCONTENT = 0x2 is
    # required for DWM/hardware-accelerated content (RT64).
    $wrect = New-Object Win32+RECT
    [Win32]::GetWindowRect($found, [ref]$wrect) | Out-Null
    $ww = $wrect.Right - $wrect.Left
    $wh = $wrect.Bottom - $wrect.Top
    $crect = New-Object Win32+RECT
    [Win32]::GetClientRect($found, [ref]$crect) | Out-Null
    $cw = $crect.Right - $crect.Left
    $ch = $crect.Bottom - $crect.Top

    if ($cw -le 0 -or $ch -le 0) {
        [Win32]::ShowWindow($found, 9) | Out-Null # SW_RESTORE
        [Win32]::MoveWindow($found, 100, 100, 976, 759, $true) | Out-Null
        Start-Sleep -Milliseconds 250
        [Win32]::GetWindowRect($found, [ref]$wrect) | Out-Null
        $ww = $wrect.Right - $wrect.Left
        $wh = $wrect.Bottom - $wrect.Top
        [Win32]::GetClientRect($found, [ref]$crect) | Out-Null
        $cw = $crect.Right - $crect.Left
        $ch = $crect.Bottom - $crect.Top
    }

    if ($cw -le 0 -or $ch -le 0) {
        Write-Host "PokemonStadiumRecomp window has empty client area; capturing primary screen instead."
        $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
        $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    } else {
    $origin = New-Object Win32+POINT
    $origin.X = 0; $origin.Y = 0
    [Win32]::ClientToScreen($found, [ref]$origin) | Out-Null
    Write-Host "Found window hwnd=$found at ($($origin.X),$($origin.Y)) client=${cw}x${ch} window=${ww}x${wh}"

    $ok = $false
    if (-not $NoPrintWindow) {
        $full = New-Object System.Drawing.Bitmap $ww, $wh
        $gFull = [System.Drawing.Graphics]::FromImage($full)
        $hdc = $gFull.GetHdc()
        $ok = [Win32]::PrintWindow($found, $hdc, 0x2)
        $gFull.ReleaseHdc($hdc)
        $gFull.Dispose()
    }
    if (-not $ok) {
        Write-Host "PrintWindow failed (ok=$ok); falling back to CopyFromScreen"
        $bmp = New-Object System.Drawing.Bitmap $cw, $ch
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($origin.X, $origin.Y, 0, 0, (New-Object System.Drawing.Size $cw, $ch))
    } else {
        # Crop window rect down to client area.
        $clientOffsetX = $origin.X - $wrect.Left
        $clientOffsetY = $origin.Y - $wrect.Top
        $bmp = New-Object System.Drawing.Bitmap $cw, $ch
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $srcRect = New-Object System.Drawing.Rectangle $clientOffsetX, $clientOffsetY, $cw, $ch
        $dstRect = New-Object System.Drawing.Rectangle 0, 0, $cw, $ch
        $g.DrawImage($full, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
        $full.Dispose()
    }
    }
}

$resolved = $OutPath
if (-not [System.IO.Path]::IsPathRooted($resolved)) {
    $resolved = Join-Path (Get-Location) $resolved
}
$dir = Split-Path -Parent $resolved
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
$bmp.Save($resolved, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "Saved: $resolved"
