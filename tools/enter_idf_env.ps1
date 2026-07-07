param(
    [Parameter(Mandatory = $false, Position = 0)]
    [string[]]$IdfArgs,

    [switch]$PrintOnly
)

$idfPath = "C:\esp\v5.5.4\esp-idf"
$idfToolsPath = "C:\Espressif\tools"
$idfPythonEnvPath = "C:\Espressif\tools\python\v5.5.4\venv"
$idfPython = Join-Path $idfPythonEnvPath "Scripts\python.exe"
$xtensaBin = "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin"
$cmakeBin = "C:\Espressif\tools\cmake\3.30.2\bin"
$ninjaBin = "C:\Espressif\tools\ninja\1.12.1"

$env:IDF_PATH = $idfPath
$env:IDF_TOOLS_PATH = $idfToolsPath
$env:IDF_PYTHON_ENV_PATH = $idfPythonEnvPath
$env:PATH = "$cmakeBin;$ninjaBin;$xtensaBin;$env:PATH"

Write-Host "IDF_PATH=$env:IDF_PATH"
Write-Host "IDF_TOOLS_PATH=$env:IDF_TOOLS_PATH"
Write-Host "IDF_PYTHON_ENV_PATH=$env:IDF_PYTHON_ENV_PATH"
Write-Host "python=$idfPython"

if ($PrintOnly) {
    return
}

if ($IdfArgs.Count -gt 0) {
    & $idfPython "$idfPath\tools\idf.py" @IdfArgs
}
