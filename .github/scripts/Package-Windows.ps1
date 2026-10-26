[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [switch] $Package
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
    $ArchiveRootName = $ProductName
    $PackageStage = "${ProjectRoot}/release/.windows-package-${Target}"
    $ArchiveRoot = "${PackageStage}/${ArchiveRootName}"
    $ArchiveDataRoot = "${ArchiveRoot}/data"
    $ArchiveBinaryRoot = $ArchiveRoot
    $InstallDataRoot = "${ProjectRoot}/release/${Configuration}/${ProductName}/data"
    $BuildBinary = "${ProjectRoot}/release/${Configuration}/${ProductName}/${ProductName}.dll"

    $ResolvedStage = [IO.Path]::GetFullPath($PackageStage)
    $ReleaseRoot = [IO.Path]::GetFullPath("${ProjectRoot}/release") + [IO.Path]::DirectorySeparatorChar
    if ( ! $ResolvedStage.StartsWith($ReleaseRoot, [StringComparison]::OrdinalIgnoreCase) ) {
        throw "Package staging directory must be inside release: ${ResolvedStage}"
    }
    if ( Test-Path -LiteralPath $ResolvedStage ) {
        Remove-Item -LiteralPath $ResolvedStage -Recurse -Force
    }

    if ( ! ( Test-Path -LiteralPath $InstallDataRoot -PathType Container ) ) {
        throw "Installed plugin data folder not found: ${InstallDataRoot}"
    }

    if ( ! ( Test-Path -LiteralPath $BuildBinary -PathType Leaf ) ) {
        throw "Built plugin DLL not found: ${BuildBinary}"
    }

    Log-Group "Archiving ${ProductName}..."
    New-Item -ItemType Directory -Path $ArchiveDataRoot, $ArchiveBinaryRoot -Force | Out-Null

    Copy-Item -LiteralPath "${InstallDataRoot}/locale" -Destination "${ArchiveDataRoot}/locale" -Recurse -Force
    Copy-Item -LiteralPath "${InstallDataRoot}/templates" -Destination "${ArchiveDataRoot}/templates" -Recurse -Force
    Copy-Item -LiteralPath "${ProjectRoot}/data/websocket-sample.html" -Destination "${ArchiveDataRoot}/websocket-sample.html" -Force
    Copy-Item -LiteralPath $BuildBinary -Destination "${ArchiveBinaryRoot}/${ProductName}.dll" -Force

    $RequiredPackagePaths = @(
        "${ArchiveBinaryRoot}/${ProductName}.dll",
        "${ArchiveDataRoot}/locale/en-US.ini",
        "${ArchiveDataRoot}/locale/ro-RO.ini",
        "${ArchiveDataRoot}/templates/README.md",
        "${ArchiveDataRoot}/templates/Modular Football/index.html",
        "${ArchiveDataRoot}/templates/Modular Football/fouls.html",
        "${ArchiveDataRoot}/templates/Modular Football/cards.html",
        "${ArchiveDataRoot}/templates/Modular Football/corners.html",
        "${ArchiveDataRoot}/templates/Modular Football/manifest.ini",
        "${ArchiveDataRoot}/templates/Modular Football/style.css",
        "${ArchiveDataRoot}/templates/Modular Football/script.js",
        "${ArchiveDataRoot}/templates/Modular Football/plugin.json"
    )
    foreach ( $RequiredPath in $RequiredPackagePaths ) {
        if ( ! ( Test-Path -LiteralPath $RequiredPath -PathType Leaf ) ) {
            throw "Required package file is missing or misplaced: ${RequiredPath}"
        }
    }
    if ( Test-Path -LiteralPath "${ArchiveDataRoot}/overlay" ) {
        throw "Legacy overlay directory must not be published; templates belong in ${ArchiveDataRoot}/templates"
    }

    $CompressArgs = @{
        Path = $ArchiveRoot
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Remove-Item -LiteralPath $PackageStage -Recurse -Force
    if ( $Package ) {
        $Nsis = Join-Path ${env:ProgramFiles(x86)} 'NSIS/makensis.exe'
        if ( ! ( Test-Path -LiteralPath $Nsis ) ) {
            throw "NSIS compiler not found: ${Nsis}"
        }
        Invoke-External $Nsis /V2 "/DPRODUCT_NAME=${ProductName}" "/DPRODUCT_VERSION=${ProductVersion}" "/DPROJECT_ROOT=${ProjectRoot}" "/DCONFIGURATION=${Configuration}" "/DTARGET=${Target}" "/DOUTPUT_EXE=${ProjectRoot}/release/${OutputName}-setup.exe" "${ProjectRoot}/installer/fly-scoreboard-installer.nsi"
    }
    Log-Group
}

Package
