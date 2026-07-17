[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,

    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateRange(0.0, 100.0)]
    [double]$MinimumLinePercent,

    [Parameter(Mandatory = $true)]
    [string]$TestExecutableList
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-VisualStudioRoots {
    $roots = [System.Collections.Generic.List[string]]::new()

    if ($env:VSINSTALLDIR) {
        $roots.Add($env:VSINSTALLDIR)
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $paths = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
        foreach ($path in $paths) {
            if ($path) {
                $roots.Add($path)
            }
        }
    }

    foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
        $roots.Add("$env:ProgramFiles\Microsoft Visual Studio\2022\$edition")
    }

    return @($roots | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique)
}

function Find-VSTestConsole([string[]]$VisualStudioRoots) {
    $onPath = Get-Command vstest.console.exe -ErrorAction SilentlyContinue
    if ($onPath) {
        return $onPath.Source
    }

    $relativePaths = @(
        "Common7\IDE\Extensions\TestPlatform\vstest.console.exe",
        "Common7\IDE\CommonExtensions\Microsoft\TestWindow\vstest.console.exe"
    )
    foreach ($root in $VisualStudioRoots) {
        foreach ($relativePath in $relativePaths) {
            $candidate = Join-Path $root $relativePath
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    throw "vstest.console.exe was not found in the active Visual Studio 2022 installation"
}

function Find-GoogleTestAdapter([string[]]$VisualStudioRoots) {
    foreach ($root in $VisualStudioRoots) {
        $extensionsRoot = Join-Path $root "Common7\IDE\Extensions"
        if (-not (Test-Path -LiteralPath $extensionsRoot)) {
            continue
        }

        $adapter = Get-ChildItem -LiteralPath $extensionsRoot -Recurse -File `
            -Filter "GoogleTestAdapter.TestAdapter.dll" -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($adapter) {
            return $adapter.DirectoryName
        }
    }

    throw @"
The Google Test Adapter was not found. In Visual Studio Installer, modify the
Desktop development with C++ workload and enable its Google Test Adapter option.
"@
}

function Find-CodeCoverageCollector([string[]]$VisualStudioRoots) {
    foreach ($root in $VisualStudioRoots) {
        $candidate = Join-Path $root `
            "Common7\IDE\Extensions\TestPlatform\Extensions\V1\Microsoft.VisualStudio.TraceCollector.dll"
        if (Test-Path -LiteralPath $candidate) {
            return Split-Path -Parent $candidate
        }
    }

    throw "The Visual Studio native code coverage collector was not found"
}

$visualStudioRoots = Get-VisualStudioRoots
$vstest = Find-VSTestConsole $visualStudioRoots
$testAdapterDirectory = Find-GoogleTestAdapter $visualStudioRoots
$coverageCollectorDirectory = Find-CodeCoverageCollector $visualStudioRoots
$sourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path
$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$testExecutables = @($TestExecutableList.Split("|", [StringSplitOptions]::RemoveEmptyEntries))

foreach ($testExecutable in $testExecutables) {
    if (-not (Test-Path -LiteralPath $testExecutable)) {
        throw "Coverage test executable does not exist: $testExecutable"
    }
}

$coverageDirectory = Join-Path $buildRoot "coverage"
$resultsDirectory = Join-Path $coverageDirectory "vstest-results"
$settingsPath = Join-Path $coverageDirectory "coverage.runsettings"
$reportPath = Join-Path $coverageDirectory "coverage.cobertura.xml"
New-Item -ItemType Directory -Force -Path $coverageDirectory | Out-Null
Remove-Item -LiteralPath $resultsDirectory -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $reportPath -Force -ErrorAction SilentlyContinue

$sourcePattern = [regex]::Escape((Join-Path $sourceRoot "src")) + "\\.*"
$escapedSourcePattern = [Security.SecurityElement]::Escape($sourcePattern)

$settings = @"
<?xml version="1.0" encoding="utf-8"?>
<RunSettings>
  <DataCollectionRunSettings>
    <DataCollectors>
      <DataCollector friendlyName="Code Coverage">
        <Configuration>
          <Format>Cobertura</Format>
          <CodeCoverage>
            <ModulePaths>
              <Include>
                <ModulePath>.*\\test_.*\.exe$</ModulePath>
                <ModulePath>.*\\irc_.*_tests\.exe$</ModulePath>
              </Include>
            </ModulePaths>
            <Sources>
              <Include>
                <Source>$escapedSourcePattern</Source>
              </Include>
            </Sources>
            <EnableStaticNativeInstrumentation>True</EnableStaticNativeInstrumentation>
            <EnableDynamicNativeInstrumentation>False</EnableDynamicNativeInstrumentation>
            <EnableStaticNativeInstrumentationRestore>True</EnableStaticNativeInstrumentationRestore>
          </CodeCoverage>
        </Configuration>
      </DataCollector>
    </DataCollectors>
  </DataCollectionRunSettings>
</RunSettings>
"@

Set-Content -LiteralPath $settingsPath -Value $settings -Encoding utf8

$vstestArguments = [System.Collections.Generic.List[string]]::new()
foreach ($testExecutable in $testExecutables) {
    $vstestArguments.Add($testExecutable)
}
$vstestArguments.Add("/TestAdapterPath:$testAdapterDirectory")
$vstestArguments.Add("/TestAdapterPath:$coverageCollectorDirectory")
$vstestArguments.Add("/Settings:$settingsPath")
$vstestArguments.Add("/Collect:Code Coverage;Format=Cobertura")
$vstestArguments.Add("/ResultsDirectory:$resultsDirectory")
$vstestArguments.Add("/Logger:console;Verbosity=minimal")

& $vstest @vstestArguments
if ($LASTEXITCODE -ne 0) {
    throw "Coverage collection or the VSTest run failed with exit code $LASTEXITCODE"
}

$generatedReport = Get-ChildItem -LiteralPath $resultsDirectory -Recurse -File `
    -Filter "*.cobertura.xml" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $generatedReport) {
    throw @"
VSTest ran the tests but did not create a Cobertura report. The native Code
Coverage data collector is not available in this Visual Studio installation.
Visual Studio 2022 Community 17.14 does not expose it as an installer component;
run this target with Visual Studio Enterprise or a CI image containing the
collector.
"@
}
Copy-Item -LiteralPath $generatedReport.FullName -Destination $reportPath -Force

[xml]$report = Get-Content -LiteralPath $reportPath -Raw
$lineRateText = $report.coverage.GetAttribute("line-rate")
if (-not $lineRateText) {
    throw "Cobertura report does not contain a root line-rate attribute"
}

$lineRate = [double]::Parse($lineRateText, [Globalization.CultureInfo]::InvariantCulture)
$linePercent = 100.0 * $lineRate
Write-Host ("src/ line coverage: {0:F2}% (required: {1:F2}%)" -f `
        $linePercent, $MinimumLinePercent)
Write-Host "Coverage report: $reportPath"

if ($linePercent + 1.0e-9 -lt $MinimumLinePercent) {
    throw ("Line coverage {0:F2}% is below the required {1:F2}%" -f `
            $linePercent, $MinimumLinePercent)
}
