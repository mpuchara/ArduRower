using Toybox.BluetoothLowEnergy as Ble;
import Toybox.System;
import Toybox.Lang;
import Toybox.Activity;
import Toybox.WatchUi;

class RowerLink extends Ble.BleDelegate {
    var ftms=Ble.stringToUuid("00001826-0000-1000-8000-00805f9b34fb");
    var rowing=Ble.stringToUuid("00002ad1-0000-1000-8000-00805f9b34fb");
    var cccd=Ble.stringToUuid("00002902-0000-1000-8000-00805f9b34fb");
    var hrValue=Ble.stringToUuid("c9f10002-6f50-4fca-9b8d-3f8d90ae1b65");
    var device=null, rowChar=null, hrChar=null;
    var state="Start", profiles=0, running=false, pairing=false, scanning=false;
    var subscribed=false, pending=false, pendingAt=0, retryAt=0, sequence=0;
    var lastRx=null, lastAck=0, packets=0, revision=0;
    var latestHr=null,hrStatus="HR czeka",lastWrite=0;
    var data as Dictionary={}, decoder;

    function initialize() { BleDelegate.initialize();decoder=new RowerPacket(); }
    function isTargetName(name as String or Null) as Boolean {
        return name!=null && name.equals("WR S4BL3");
    }
    function retryReady(now as Number) as Boolean {
        // System.getTimer is signed and may already be negative on a watch
        // with a long uptime. Compare elapsed time, never raw timestamps.
        return retryAt==0 || now-retryAt>=0;
    }
    function start() {
        running=true;Ble.setDelegate(self);
        System.println("AROWER 0.1.7: start clock="+System.getTimer());
        try {
            Ble.registerProfile({:uuid=>ftms,:characteristics=>[
                {:uuid=>rowing,:descriptors=>[cccd]}, {:uuid=>hrValue}]});
            // Older watches may omit onProfileRegister. Garmin's SDK sample
            // starts scanning immediately after registerProfile returns.
            profiles=1;System.println("AROWER: profile registered; start scan");scan();
        } catch(e) {state="Profil BLE zajety";}
    }
    function stop() {
        running=false;Ble.setScanState(Ble.SCAN_STATE_OFF);
        if (device!=null) { Ble.unpairDevice(device); }
        device=null;hrChar=null;rowChar=null;pending=false;subscribed=false;
        clearSample();
    }
    function onProfileRegister(uuid,status) {
        if (status!=Ble.STATUS_SUCCESS) { state="Blad profilu";return; }
        profiles=1;scan();
    }
    function scan() {
        if (!running || profiles!=1 || device!=null || pairing || scanning) { return; }
        state="Szukam WR S4BL3";
        try { Ble.setScanState(Ble.SCAN_STATE_SCANNING);scanning=true; }
        catch(e) { state="BLE zajete";retryAt=System.getTimer()+5000; }
    }
    function onScanResults(results) {
        if (!running || device!=null || pairing) { return; }
        for (var r=results.next();r!=null;r=results.next()) {
            if (!(r instanceof Ble.ScanResult)) { continue; }
            if (!isTargetName(r.getDeviceName())) { continue; }
            var uuids=r.getServiceUuids();var match=false;
            for (var u=uuids.next();u!=null;u=uuids.next()) { if(u.equals(ftms)){match=true;} }
            if (!match) { continue; }
            pairing=true;state="Laczenie";
            System.println("AROWER: found WR S4BL3; pair");
            try {
                Ble.setScanState(Ble.SCAN_STATE_OFF);
                scanning=false;
                device=Ble.pairDevice(r);
                if(device==null){pairing=false;retryAt=System.getTimer()+5000;}
            } catch(e) { pairing=false;state="Blad laczenia";retryAt=System.getTimer()+5000; }
            return;
        }
    }
    function onConnectedStateChanged(peer,connectionState) {
        System.println("AROWER: connection state="+connectionState);
        if (!running) { return; }
        clearSample();
        if (!peer.isConnected()) {
            state="Rozlaczono";subscribed=false;pending=false;hrChar=null;rowChar=null;
            // pairDevice keeps this device in the system reconnect list.
            device=peer;pairing=false;lastAck=0;WatchUi.requestUpdate();return;
        }
        device=peer;pairing=false;sequence=0;pending=false;lastAck=0;
        var service=peer.getService(ftms);
        if(service==null){state="Wgraj ESP 0.09.23";return;}
        rowChar=service.getCharacteristic(rowing);hrChar=service.getCharacteristic(hrValue);
        System.println("AROWER: GATT rowing="+(rowChar!=null)+" HR="+(hrChar!=null));
        if(rowChar==null || hrChar==null){state="Brak kanalu HR";return;}
        var descriptor=rowChar.getDescriptor(cccd);
        if(descriptor==null){state="Brak CCCD";return;}
        state="Subskrypcja";
        try { descriptor.requestWrite([1,0]b); }
        catch(e){state="Blad subskrypcji";}
    }
    function onDescriptorWrite(descriptor,status) {
        System.println("AROWER: rowing CCCD status="+status);
        if(status==Ble.STATUS_SUCCESS){subscribed=true;state="BLE OK";}
        else{state="CCCD blad "+status;subscribed=false;}
    }
    function onCharacteristicChanged(characteristic,value) {
        if(!running || rowChar==null || device==null || !device.isConnected() ||
            !characteristic.getUuid().equals(rowing)){return;}
        if(!acceptSample(value,System.getTimer())){
            System.println("AROWER: rejected FTMS bytes="+value.size());return;
        }
        // A real notification proves the CCCD is enabled even on firmware
        // that omits the descriptor completion callback.
        subscribed=true;state="BLE OK";
        var info=Activity.getActivityInfo();
        tick(info==null?null:info.currentHeartRate);
        WatchUi.requestUpdate();
    }
    function acceptSample(value,now) {
        var next=decoder.decode(value);
        if(next==null){return false;}
        data=next;lastRx=now;packets+=1;revision+=1;
        return true;
    }
    function clearSample() {data={};lastRx=null;}
    function freshAt(now,limit) {
        if(lastRx==null){return false;}
        var age=now-lastRx;
        return age>=0 && age<limit;
    }
    function fresh() { return freshAt(System.getTimer(),6500); }
    function fitSample(now,recorded) {
        return revision!=recorded && freshAt(now,2500)?data:null;
    }
    function hrFresh() { return lastAck!=0 && System.getTimer()-lastAck<6500; }
    function tick(bpm) {
        latestHr=bpm;
        var now=System.getTimer();
        if(device==null && !pairing && retryReady(now) && profiles==1){scan();retryAt=now+10000;}
        if(pending && now-pendingAt>8000){hrStatus="HR timeout";pending=false;retryAt=now+5000;}
        if(!subscribed){hrStatus="HR bez CCCD";return;}
        if(hrChar==null){hrStatus="Brak HR GATT";return;}
        if(pending){return;}
        if(!retryReady(now)){hrStatus="HR opoznienie";return;}
        if(lastWrite!=0 && now-lastWrite<950){return;}
        var valid=bpm!=null && bpm>=35 && bpm<=240;
        var value=valid ? bpm.toNumber() : 0;
        sequence=(sequence+1)&0xffff;
        var frame=[1,valid?1:0,sequence&255,(sequence>>8)&255,value&255,(value>>8)&255]b;
        pending=true;pendingAt=now;lastWrite=now;hrStatus="HR wysylam";
        if(sequence<=3){System.println("AROWER: HR request seq="+sequence+" bpm="+value);}
        try {hrChar.requestWrite(frame,{:writeType=>Ble.WRITE_TYPE_WITH_RESPONSE});}
        catch(e){pending=false;hrStatus="HR zapis zajety";retryAt=now+2000;System.println("AROWER: HR write exception "+e);}
    }
    function onCharacteristicWrite(characteristic,status) {
        if(!characteristic.getUuid().equals(hrValue)){return;}
        pending=false;
        if(status==Ble.STATUS_SUCCESS){lastAck=System.getTimer();hrStatus="HR OK";}
        else{hrStatus="HR blad "+status;retryAt=System.getTimer()+3000;}
        if(sequence<=3 || sequence%10==0){System.println("AROWER: HR write status="+status+" seq="+sequence);}
    }
    function onScanStateChange(scanState,status) {
        System.println("AROWER: scan state="+scanState+" status="+status);
        if(status!=Ble.STATUS_SUCCESS){scanning=false;state="Skan blad "+status;retryAt=System.getTimer()+5000;}
    }
}
