# ------------------------------------------------------------
# Firmware für den Web-Installer bereitstellen
# ------------------------------------------------------------
# Kopiert die Teile aus dem IDE-Export nach docs/firmware/ und trägt die
# Version aus FW_VERSION (.ino) in docs/manifest.json ein.
#
# Ablauf:
#   1. Arduino IDE: Sketch > Kompilierte Binärdatei exportieren
#      (ohne secrets.h im Sketch-Ordner - die Firmware für den Installer
#      darf keine WLAN-Daten enthalten)
#   2. Doppelklick auf firmware_fuer_installer.bat (startet dieses Skript)
#
# Vor dem Kopieren wird jede Datei geprüft. Enthält eine davon den Text
# "secrets.h eingebunden", bricht das Skript ab und kopiert nichts.
# boot_app0.bin liegt fest in docs/firmware/ und wird nicht angefasst.

$ErrorActionPreference = 'Stop'
$repo     = Split-Path -Parent $PSScriptRoot
$sketch   = 'MVG_Abfahrtsdisplay_E290'
$build    = Join-Path $repo 'build\esp32.esp32.heltec_vision_master_e290'
$ziel     = Join-Path $repo 'docs\firmware'
$manifest = Join-Path $repo 'docs\manifest.json'
$maxApp   = 3342336   # Größe einer App-Partition (partitions.csv)

function Ende($code) {
  # Aus der .bat gestartet: die .bat wartet selbst (pause)
  if (-not $env:FW_EXPORT_BAT) {
    Write-Host ''
    Read-Host 'Enter zum Schließen' | Out-Null
  }
  exit $code
}
function Abbruch($text) {
  Write-Host ''
  Write-Host "ABBRUCH: $text" -ForegroundColor Red
  Write-Host 'Es wurde nichts kopiert.'
  Ende 1
}

try {
# ---- Teile einlesen ----------------------------------------
$teile = [ordered]@{
  'bootloader.bin' = "$sketch.ino.bootloader.bin"
  'partitions.bin' = "$sketch.ino.partitions.bin"
  'firmware.bin'   = "$sketch.ino.bin"
}
$daten = @{}
foreach ($name in $teile.Keys) {
  $pfad = Join-Path $build $teile[$name]
  if (-not (Test-Path $pfad)) {
    Abbruch "$($teile[$name]) fehlt in build\. Zuerst in der Arduino IDE exportieren (Sketch > Kompilierte Binärdatei exportieren)."
  }
  $daten[$name] = [IO.File]::ReadAllBytes($pfad)
}

# ---- Prüfungen ---------------------------------------------
# Latin-1 bildet jedes Byte auf genau ein Zeichen ab -> Textsuche im Binärinhalt
$latin1 = [Text.Encoding]::GetEncoding(28591)
foreach ($name in $teile.Keys) {
  if ($latin1.GetString($daten[$name]).Contains('secrets.h eingebunden')) {
    Abbruch "$name wurde MIT secrets.h gebaut (enthält deine WLAN-Daten). secrets.h aus dem Sketch-Ordner entfernen und neu exportieren."
  }
}
$fw = $daten['firmware.bin']
if ($fw.Length -lt 1024 -or $fw[0] -ne 0xE9) { Abbruch 'firmware.bin ist keine ESP32-Firmware.' }
if ($fw.Length -gt $maxApp) { Abbruch 'firmware.bin ist größer als die App-Partition (merged .bin erwischt?).' }
if (-not $latin1.GetString($fw).Contains('MVG_Abfahrtsdisplay_E290/firmware')) {
  Abbruch 'firmware.bin enthält die Projektkennung nicht (falscher Sketch?).'
}
if ($daten['bootloader.bin'][0] -ne 0xE9) { Abbruch 'bootloader.bin ist kein ESP32-Bootloader.' }
$pt = $daten['partitions.bin']
if ($pt.Length -ne 3072 -or $pt[0] -ne 0xAA -or $pt[1] -ne 0x50) { Abbruch 'partitions.bin ist keine Partitionstabelle.' }
if (-not (Test-Path (Join-Path $ziel 'boot_app0.bin'))) { Abbruch 'docs\firmware\boot_app0.bin fehlt.' }

# Export älter als der Code? (vergessen neu zu exportieren)
$fwZeit = (Get-Item (Join-Path $build $teile['firmware.bin'])).LastWriteTime
$code = @(Get-ChildItem $repo -File | Where-Object { $_.Extension -in '.ino', '.h', '.csv' }) +
        @(Get-ChildItem (Join-Path $repo 'src') -File)
$neuer = $code | Where-Object { $_.LastWriteTime -gt $fwZeit -and $_.Name -ne 'secrets.h' }
if ($neuer) {
  Write-Host 'Achtung: nach dem Export geändert:' -ForegroundColor Yellow
  $neuer | ForEach-Object { Write-Host "  $($_.Name)" }
  if ((Read-Host 'Trotzdem übernehmen? (j/n)') -ne 'j') { Abbruch 'Bitte neu exportieren.' }
}

# ---- Version -----------------------------------------------
$ino = [IO.File]::ReadAllText((Join-Path $repo "$sketch.ino"))
$treffer = [regex]::Match($ino, '#define\s+FW_VERSION\s+"([^"]+)"')
if (-not $treffer.Success) { Abbruch 'FW_VERSION nicht in der .ino gefunden.' }
$version = $treffer.Groups[1].Value

# ---- Schreiben ---------------------------------------------
$json = [IO.File]::ReadAllText($manifest)   # vorher lesen: Fehler -> nichts kopiert
foreach ($name in $teile.Keys) {
  [IO.File]::WriteAllBytes((Join-Path $ziel $name), $daten[$name])
}
$json = [regex]::Replace($json, '"version"\s*:\s*"[^"]*"', "`"version`": `"$version`"")
[IO.File]::WriteAllText($manifest, $json, (New-Object Text.UTF8Encoding($false)))

Write-Host ''
Write-Host "Fertig: Firmware $version nach docs\firmware kopiert." -ForegroundColor Green
foreach ($name in $teile.Keys) { Write-Host ("  {0,-15} {1,10:N0} Bytes" -f $name, $daten[$name].Length) }
Write-Host 'Prüfung "secrets.h eingebunden": nicht enthalten.'
} catch {
  Abbruch "Unerwarteter Fehler: $($_.Exception.Message)"
}
Ende 0
