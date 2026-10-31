param(
  [Parameter(Mandatory=$true)][string]$SdkPath,
  [string]$JavaPath='java',
  [Parameter(Mandatory=$true)][string]$SigningKey,
  [string]$Device='fenix6xpro',
  [switch]$RunTests
)
$ErrorActionPreference='Stop'
$sdk=(Resolve-Path -LiteralPath $SdkPath).Path
$key=(Resolve-Path -LiteralPath $SigningKey).Path
$dest=Join-Path $PSScriptRoot 'bin'
New-Item -ItemType Directory -Path $dest -Force | Out-Null
$program=Join-Path $dest $(if($RunTests){'AROWERHR-test.prg'}else{'AROWERHR.PRG'})
$buildLog=Join-Path $dest 'build.log'
$argsList=@('-Xms256m','-cp',(Join-Path $sdk 'bin/monkeybrains.jar'),
  'com.garmin.monkeybrains.Monkeybrains','-f',(Join-Path $PSScriptRoot 'monkey.jungle'),
  '-d',$Device,'-o',$program,'-y',$key,'-w')
if($RunTests){$argsList+='-t'}
& $JavaPath @argsList *> $buildLog
Get-Content -LiteralPath $buildLog
if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $program) -or
   !(Select-String -LiteralPath $buildLog -Pattern '^BUILD SUCCESSFUL$' -Quiet)){
  throw 'Connect IQ build failed; existing output is not a validated new build'
}
if($RunTests){
  $testLog=Join-Path $dest 'tests.log'
  & $JavaPath -cp (Join-Path $sdk 'bin/monkeybrains.jar') `
    com.garmin.monkeybrains.monkeydodeux.MonkeyDoDeux -f $program -d $Device `
    -s (Join-Path $sdk 'bin/shell.exe') -t *> $testLog
  Get-Content -LiteralPath $testLog
  if(!(Select-String -LiteralPath $testLog -Pattern '^PASSED \(passed=' -Quiet)){
    throw 'Simulator tests did not report PASSED'
  }
}
Get-FileHash -LiteralPath $program -Algorithm SHA256
