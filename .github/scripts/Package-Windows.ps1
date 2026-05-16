[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"
    $ArchiveRootName = 'fly-score'
    $ArchivePluginDataName = 'fly-score'
    $PackageStage = "${ProjectRoot}/release/.windows-package-${Target}"
    $ArchiveRoot = "${PackageStage}/${ArchiveRootName}"
    $ArchiveDataRoot = "${ArchiveRoot}/data/obs-plugins/${ArchivePluginDataName}"
    $ArchiveBinaryRoot = "${ArchiveRoot}/obs-plugins/64bit"
    $InstallDataRoot = "${ProjectRoot}/release/${Configuration}/${ProductName}/data"
    $BuildBinary = "${ProjectRoot}/build_${Target}/${Configuration}/${ProductName}.dll"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Force = $true
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip",
            $PackageStage
        )
        Recurse = $true
    }

    Remove-Item @RemoveArgs

    if ( ! ( Test-Path -LiteralPath $InstallDataRoot -PathType Container ) ) {
        throw "Installed plugin data folder not found: ${InstallDataRoot}"
    }

    if ( ! ( Test-Path -LiteralPath $BuildBinary -PathType Leaf ) ) {
        throw "Built plugin DLL not found: ${BuildBinary}"
    }

    Log-Group "Archiving ${ProductName}..."
    New-Item -ItemType Directory -Path $ArchiveDataRoot, $ArchiveBinaryRoot -Force | Out-Null

    Copy-Item -LiteralPath "${InstallDataRoot}/locale" -Destination "${ArchiveDataRoot}/locale" -Recurse -Force
    Copy-Item -LiteralPath "${InstallDataRoot}/overlay" -Destination "${ArchiveDataRoot}/overlay" -Recurse -Force
    Copy-Item -LiteralPath "${ProjectRoot}/data/websocket-sample.html" -Destination "${ArchiveDataRoot}/websocket-sample.html" -Force
    Copy-Item -LiteralPath $BuildBinary -Destination "${ArchiveBinaryRoot}/${ProductName}.dll" -Force

    $CompressArgs = @{
        Path = $ArchiveRoot
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Remove-Item -LiteralPath $PackageStage -Recurse -Force
    Log-Group
}

Package
