param(
  [Parameter(Mandatory=$true)][string]$Program,
  [string]$WatchName='fenix 6X Pro Solar',
  [string]$ReceiptRoot=(Join-Path (Get-Location).Path 'work/garmin-usb')
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath $Program).Path
if([IO.Path]::GetFileName($source) -cne 'AROWERHR.PRG'){
  throw 'Only the own field AROWERHR.PRG may be installed with this tool'
}
$stamp=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6)
$receipt=Join-Path $ReceiptRoot $stamp
$backupPath=Join-Path $receipt 'before'
$verifyPath=Join-Path $receipt 'readback'
New-Item -ItemType Directory -Force $backupPath,$verifyPath | Out-Null
$shell=New-Object -ComObject Shell.Application
$watch=@($shell.Namespace(17).Items() | Where-Object {$_.Name -eq $WatchName})
if($watch.Count -ne 1){throw 'Expected exactly one matching USB/MTP watch'}
$root=$watch[0].GetFolder
$primary=$root.ParseName('Primary')
if(!$primary){throw 'Primary storage unavailable'}
$storage=$primary.GetFolder
$garmin=$storage.ParseName('Garmin').GetFolder
$apps=$garmin.ParseName('Apps').GetFolder
$old=$apps.ParseName('AROWERHR.PRG')
$backup=$shell.Namespace((Resolve-Path -LiteralPath $backupPath).Path)
if($old){
  $backup.CopyHere($old,0x414)
  $until=(Get-Date).AddSeconds(20)
  do {Start-Sleep -Milliseconds 250;$saved=Get-Item (Join-Path $backupPath 'AROWERHR.PRG') -ErrorAction SilentlyContinue}
  while((!$saved -or $saved.Length -eq 0) -and (Get-Date) -lt $until)
  if(!$saved -or $saved.Length -eq 0){throw 'Previous field was not backed up; update aborted'}
}
$logsItem=$apps.ParseName('Logs')
if($logsItem){
  $logs=$logsItem.GetFolder
  $log=$logs.ParseName('AROWERHR.TXT')
  if($log){$backup.CopyHere($log,0x414)}
  else {
    $empty=Join-Path $receipt 'AROWERHR.TXT'
    [IO.File]::WriteAllText($empty,'')
    $logs.CopyHere($empty,0x414)
  }
}
$apps.CopyHere($source,0x414)
Start-Sleep -Seconds 2
$target=$apps.ParseName('AROWERHR.PRG')
if(!$target){throw 'Copied field unavailable on watch'}
$verify=$shell.Namespace((Resolve-Path -LiteralPath $verifyPath).Path)
$verify.CopyHere($target,0x414)
$expected=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$until=(Get-Date).AddSeconds(20)
$actual=''
do {
  Start-Sleep -Milliseconds 250
  $copied=Join-Path $verifyPath 'AROWERHR.PRG'
  if(Test-Path -LiteralPath $copied){$actual=(Get-FileHash -LiteralPath $copied -Algorithm SHA256).Hash}
} while($actual -ne $expected -and (Get-Date) -lt $until)
if($actual -ne $expected){throw 'USB readback does not match; installation not accepted'}
$result=[pscustomobject]@{Verified=$true;SHA256=$actual;Watch=$WatchName;Source=$source;Receipt=$receipt}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $receipt 'receipt.json')
$result | ConvertTo-Json
