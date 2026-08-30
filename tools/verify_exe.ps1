# verify_exe.ps1 - validates a speed.exe against the identity and byte guards
# used by HelicopterOptions V2.x.
#
# FIXED vs the audit-era script: SizeOfImage is READ FROM THE OPTIONAL HEADER,
# never derived from section math, and section addresses are printed with RVA
# and absolute VA as separate, clearly-labeled columns. (The old script once
# summed an absolute VA with a section size and produced a bogus 0x00A79000.)
param(
    [string]$ExePath = "C:\Users\Benje\Downloads\nl-NFSMW_v1.3\speed.exe"
)

$expected = @{
    Machine     = 0x014C
    TimeStamp   = 0x438E4C8C
    ImageBase   = 0x00400000
    SizeOfImage = 0x00678E4E
    Sha256      = "80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c"
}

$bytes = [System.IO.File]::ReadAllBytes($ExePath)
$e     = [BitConverter]::ToInt32($bytes, 0x3C)
$fileH = $e + 4
$opt   = $fileH + 20

$machine   = [BitConverter]::ToUInt16($bytes, $fileH)
$numSec    = [BitConverter]::ToUInt16($bytes, $fileH + 2)
$stamp     = [BitConverter]::ToUInt32($bytes, $fileH + 4)
$optSize   = [BitConverter]::ToUInt16($bytes, $fileH + 16)
$imageBase = [BitConverter]::ToUInt32($bytes, $opt + 28)
$sizeOfImg = [BitConverter]::ToUInt32($bytes, $opt + 56)   # <- authoritative source
$sha       = (Get-FileHash $ExePath -Algorithm SHA256).Hash.ToLower()

Write-Output ("EXE: {0} ({1} bytes)" -f $ExePath, $bytes.Length)
Write-Output ("Machine     0x{0:X4}   expected 0x{1:X4}   {2}" -f $machine,   $expected.Machine,     $(if ($machine  -eq $expected.Machine)     {"OK"} else {"MISMATCH"}))
Write-Output ("TimeStamp   0x{0:X8} expected 0x{1:X8} {2}"     -f $stamp,     $expected.TimeStamp,   $(if ($stamp    -eq $expected.TimeStamp)   {"OK"} else {"MISMATCH"}))
Write-Output ("ImageBase   0x{0:X8} expected 0x{1:X8} {2}"     -f $imageBase, $expected.ImageBase,   $(if ($imageBase -eq $expected.ImageBase)  {"OK"} else {"MISMATCH"}))
Write-Output ("SizeOfImage 0x{0:X8} expected 0x{1:X8} {2}  (read from OptionalHeader+56)" -f $sizeOfImg, $expected.SizeOfImage, $(if ($sizeOfImg -eq $expected.SizeOfImage) {"OK"} else {"MISMATCH"}))
Write-Output ("SHA-256     {0}" -f $sha)
Write-Output ("            expected {0}   {1}" -f $expected.Sha256, $(if ($sha -eq $expected.Sha256) {"OK"} else {"MISMATCH"}))

# Section table: RVA and VA printed separately; never mix the two.
Write-Output ""
Write-Output "Idx Name      RVA        AbsoluteVA  VSize      RawOff     RawSize"
$secBase = $opt + $optSize
$sections = @()
for ($i = 0; $i -lt $numSec; $i++) {
    $off  = $secBase + $i * 40
    $name = [System.Text.Encoding]::ASCII.GetString($bytes, $off, 8).TrimEnd([char]0)
    $vsz  = [BitConverter]::ToUInt32($bytes, $off + 8)
    $rva  = [BitConverter]::ToUInt32($bytes, $off + 12)
    $rsz  = [BitConverter]::ToUInt32($bytes, $off + 16)
    $raw  = [BitConverter]::ToUInt32($bytes, $off + 20)
    $sections += [PSCustomObject]@{ RVA=$rva; VSize=$vsz; Raw=$raw; RSize=$rsz }
    Write-Output ("{0,-3} {1,-8} 0x{2:X8} 0x{3:X8}  0x{4:X8} 0x{5:X8} 0x{6:X8}" -f $i, $name, $rva, ($imageBase + $rva), $vsz, $raw, $rsz)
}

# VA -> file offset (input is an ABSOLUTE VA; converted to RVA exactly once here)
function VaToOff([uint32]$va) {
    $rva = $va - $script:imageBase
    foreach ($s in $script:sections) {
        $span = [Math]::Max($s.VSize, $s.RSize)
        if ($rva -ge $s.RVA -and $rva -lt ($s.RVA + $span)) {
            $delta = $rva - $s.RVA
            if ($delta -lt $s.RSize) { return [int64]($s.Raw + $delta) }
            return -2
        }
    }
    return -1
}

# Spot-check the two hook-site guards (full guard list lives in src/Core/Addresses.h).
$checks = @(
    @("OnDriving prologue @0x00417A20", 0x00417A20, "83EC685355"),
    @("HeliCtor prologue @0x0041A5E0",  0x0041A5E0, "6AFF688E7C8600")
)
Write-Output ""
foreach ($c in $checks) {
    $off = VaToOff ([uint32]$c[1])
    if ($off -lt 0) { Write-Output ("{0}: UNMAPPED" -f $c[0]); continue }
    $len = $c[2].Length / 2
    $act = -join (($bytes[$off..($off+$len-1)]) | ForEach-Object { $_.ToString("X2") })
    Write-Output ("{0}: {1}" -f $c[0], $(if ($act -eq $c[2]) {"OK"} else {"MISMATCH actual=$act"}))
}
