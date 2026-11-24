package pl.ardurower.garetthr;

import android.Manifest;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.content.Context;
import android.content.pm.PackageManager;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.hardware.*;
import android.location.LocationManager;
import android.graphics.Color;
import android.view.Gravity;
import android.view.WindowManager;
import android.widget.*;
import java.util.UUID;
import java.util.Collections;

public final class MainActivity extends Activity implements SensorEventListener {
    private static final UUID SERVICE=UUID.fromString("00001826-0000-1000-8000-00805f9b34fb");
    private static final UUID ROW=UUID.fromString("00002ad1-0000-1000-8000-00805f9b34fb");
    private static final UUID CCCD=UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private static final UUID HR=UUID.fromString("c9f10002-6f50-4fca-9b8d-3f8d90ae1b65");
    private static final String NAME="WR S4BL3";
    private final Handler h=new Handler(Looper.getMainLooper());
    private TextView value,status,counts,sensorStatus,details;
    private Button connect;
    private SensorManager sensors;
    private Sensor heart;
    private BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic tx;
    private boolean active,wanted,scanning,ready,pending,registered;
    private long sampled=-1,deadline=0,nextAttempt=0,writeAt=0;
    private int bpm=0,seq=0,rx=0,acked=0,validAck=0,samples=0,lastSent=0;
    private String message="Rozłączono", sensorMessage="Sprawdzanie";
    private final Runnable ticker=new Runnable(){public void run(){
        if(!active)return;
        long now=SystemClock.elapsedRealtime();
        if(wanted){
            if(scanning && now>=deadline){stopScan(); retry("Nie znaleziono ArduRower");}
            else if(gatt!=null && ((!ready && now>=deadline)||(pending && now-writeAt>5000))){retry("Timeout BLE");}
            else if(gatt==null && !scanning && now>=nextAttempt){startScan();}
            else if(ready && !pending && now-writeAt>=1000){send(now);}
        }
        paint(now);
        h.postDelayed(this,250);
    }};
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        sensors=(SensorManager)getSystemService(SENSOR_SERVICE);
        heart=sensors==null?null:sensors.getDefaultSensor(Sensor.TYPE_HEART_RATE);
        BluetoothManager m=(BluetoothManager)getSystemService(BLUETOOTH_SERVICE);
        adapter=m==null?null:m.getAdapter();
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);root.setPadding(12,8,12,8);root.setBackgroundColor(0xff07111d);
        scroll.addView(root);setContentView(scroll);
        add(root,"ArduRower HR",20,Color.WHITE);
        value=add(root,"--",58,0xffff627e);
        add(root,"BPM",16,0xffc0cbd9);
        sensorStatus=add(root,"",14,0xffc0cbd9);
        status=add(root,message,16,0xff56dec5);
        counts=add(root,"",13,0xffc0cbd9);
        connect=new Button(this);connect.setText("Połącz");root.addView(connect,new LinearLayout.LayoutParams(-1,-2));
        connect.setOnClickListener(v->{if(wanted){wanted=false;stopScan();closeGatt();message="Rozłączono";}else{
            wanted=true;nextAttempt=0;askPermissions();}paint(SystemClock.elapsedRealtime());});
        Button diag=new Button(this);diag.setText("Czujniki / diagnostyka");root.addView(diag,new LinearLayout.LayoutParams(-1,-2));
        details=add(root,"",12,0xffc0cbd9);details.setGravity(Gravity.START);details.setVisibility(android.view.View.GONE);
        diag.setOnClickListener(v->{boolean show=details.getVisibility()!=android.view.View.VISIBLE;details.setVisibility(show?android.view.View.VISIBLE:android.view.View.GONE);if(show)listSensors();});
    }
    private TextView add(LinearLayout root,String s,int sp,int color){
        TextView t=new TextView(this);t.setGravity(Gravity.CENTER);t.setText(s);t.setTextSize(sp);t.setTextColor(color);
        root.addView(t,new LinearLayout.LayoutParams(-1,-2));return t;
    }
    @Override protected void onStart(){super.onStart();active=true;startHeart();askPermissions();h.removeCallbacks(ticker);h.post(ticker);}
    @Override protected void onStop(){active=false;h.removeCallbacks(ticker);stopScan();closeGatt();
        if(sensors!=null)sensors.unregisterListener(this);registered=false;invalidateHeart();super.onStop();}
    private boolean granted(String p){return checkSelfPermission(p)==PackageManager.PERMISSION_GRANTED;}
    private void askPermissions(){
        java.util.ArrayList<String> needed=new java.util.ArrayList<>();
        if(heart!=null&&!granted(Manifest.permission.BODY_SENSORS))needed.add(Manifest.permission.BODY_SENSORS);
        if(wanted&&!granted(Manifest.permission.ACCESS_FINE_LOCATION))needed.add(Manifest.permission.ACCESS_FINE_LOCATION);
        if(!needed.isEmpty())requestPermissions(needed.toArray(new String[0]),7);
    }
    @Override public void onRequestPermissionsResult(int req,String[] p,int[] result){super.onRequestPermissionsResult(req,p,result);startHeart();nextAttempt=0;}
    private void startHeart(){
        if(!active||registered)return;
        if(heart==null){sensorMessage="Brak TYPE_HEART_RATE";return;}
        if(!granted(Manifest.permission.BODY_SENSORS)){sensorMessage="Brak zgody na pulsometr";return;}
        try {registered=sensors.registerListener(this,heart,SensorManager.SENSOR_DELAY_NORMAL);
            sensorMessage=registered?"Czekam na puls":"Czujnik nie uruchomił się";
        }catch(RuntimeException e){sensorMessage="Błąd czujnika: "+e.getClass().getSimpleName();}
    }
    private void invalidateHeart(){bpm=0;sampled=-1;}
    @Override public void onSensorChanged(SensorEvent event){
        if(!active||event.sensor.getType()!=Sensor.TYPE_HEART_RATE)return;
        samples++;
        if(event.values.length==0 || event.accuracy<=0 || !Float.isFinite(event.values[0]) || event.values[0]<35 || event.values[0]>240){
            invalidateHeart();sensorMessage="Brak wiarygodnego pulsu";return;
        }
        bpm=Math.round(event.values[0]);sampled=event.timestamp/1000000L;
        if(sampled>SystemClock.elapsedRealtime()||sampled<0){invalidateHeart();return;}
        sensorMessage="Czujnik: "+event.sensor.getName();
    }
    @Override public void onAccuracyChanged(Sensor sensor,int accuracy){if(accuracy<=0)invalidateHeart();}
    private void listSensors(){
        StringBuilder b=new StringBuilder("Android "+android.os.Build.VERSION.RELEASE+" / API "+android.os.Build.VERSION.SDK_INT+"\n"+android.os.Build.MODEL+"\nPuls: "+sensorMessage+"\n");
        if(sensors!=null)for(Sensor s:sensors.getSensorList(Sensor.TYPE_ALL))b.append(s.getName()).append(" / ").append(s.getVendor()).append(" / ").append(s.getType()).append(" / ").append(s.getStringType()).append('\n');
        details.setText(b.toString());
    }
    private void paint(long now){
        boolean fresh=HrPacket.fresh(bpm,sampled,now);
        value.setText(fresh?String.valueOf(bpm):"--");
        sensorStatus.setText(sensorMessage+(registered&&!fresh?" • brak świeżej próbki":""));
        status.setText(message);
        counts.setText("Próbki HR: "+samples+" | FTMS: "+rx+"\nHR ACK: "+acked+" (BPM: "+validAck+")"+(ready?"\nOstatni ACK: "+(lastSent==0?"brak":lastSent+" BPM"):""));
        connect.setText(wanted?"Rozłącz":"Połącz");
    }
    private void startScan(){
        if(!active||!wanted||scanning||gatt!=null)return;
        if(adapter==null){message="Brak Bluetooth";nextAttempt=SystemClock.elapsedRealtime()+4000;return;}
        if(!adapter.isEnabled()){message="Włącz Bluetooth w zegarku";nextAttempt=SystemClock.elapsedRealtime()+4000;return;}
        if(!granted(Manifest.permission.ACCESS_FINE_LOCATION)){message="Zezwól na lokalizację dla BLE";nextAttempt=SystemClock.elapsedRealtime()+4000;return;}
        try{
            LocationManager lm=(LocationManager)getSystemService(LOCATION_SERVICE);
            if(lm!=null&&!lm.isProviderEnabled(LocationManager.GPS_PROVIDER)&&!lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)){
                message="Włącz lokalizację dla skanowania BLE";nextAttempt=SystemClock.elapsedRealtime()+4000;return;
            }
            scanner=adapter.getBluetoothLeScanner();
            if(scanner==null){retry("Skaner BLE niedostępny");return;}
            scanning=true;deadline=SystemClock.elapsedRealtime()+12000;message="Szukam WR S4BL3";
            scanner.startScan(Collections.singletonList(new ScanFilter.Builder().setDeviceName(NAME).build()),
                new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),scanCallback);
        }catch(RuntimeException e){retry("Skan BLE: "+e.getClass().getSimpleName());}
    }
    private final ScanCallback scanCallback=new ScanCallback(){
        @Override public void onScanResult(int type,ScanResult result){h.post(()->{
            if(!active||!wanted||!scanning)return;
            stopScan();message="Łączenie z ArduRower";deadline=SystemClock.elapsedRealtime()+15000;
            try{gatt=result.getDevice().connectGatt(MainActivity.this,false,callback,BluetoothDevice.TRANSPORT_LE);
                if(gatt==null)retry("Brak połączenia GATT");
            }catch(RuntimeException e){retry("GATT: "+e.getClass().getSimpleName());}
        });}
        @Override public void onScanFailed(int error){h.post(()->{if(scanning)retry("Skan BLE: "+error);});}
    };
    private void stopScan(){
        boolean was=scanning;scanning=false;
        if(was&&scanner!=null)try{scanner.stopScan(scanCallback);}catch(RuntimeException ignored){}
        scanner=null;
    }
    private void closeGatt(){
        BluetoothGatt old=gatt;gatt=null;tx=null;ready=false;pending=false;seq=0;lastSent=0;
        if(old!=null){try{old.disconnect();}catch(RuntimeException ignored){}try{old.close();}catch(RuntimeException ignored){}}
    }
    private void retry(String why){stopScan();closeGatt();message=why+" • ponawiam";nextAttempt=SystemClock.elapsedRealtime()+3000;}
    private final BluetoothGattCallback callback=new BluetoothGattCallback(){
        @Override public void onConnectionStateChange(BluetoothGatt g,int code,int state){h.post(()->{
            if(g!=gatt)return;
            if(code!=BluetoothGatt.GATT_SUCCESS||state==BluetoothProfile.STATE_DISCONNECTED){retry("BLE rozłączono ("+code+")");return;}
            if(state==BluetoothProfile.STATE_CONNECTED){
                message="Odkrywam usługę FTMS";deadline=SystemClock.elapsedRealtime()+12000;
                try{if(!g.discoverServices())retry("Nie można odkryć usług");}catch(RuntimeException e){retry("Błąd usług");}
            }
        });}
        @Override public void onServicesDiscovered(BluetoothGatt g,int code){h.post(()->{
            if(g!=gatt)return;
            if(code!=BluetoothGatt.GATT_SUCCESS){retry("Błąd usług "+code);return;}
            BluetoothGattService service=g.getService(SERVICE);
            BluetoothGattCharacteristic row=service==null?null:service.getCharacteristic(ROW);
            tx=service==null?null:service.getCharacteristic(HR);
            BluetoothGattDescriptor desc=row==null?null:row.getDescriptor(CCCD);
            if(tx==null||desc==null||(tx.getProperties()&BluetoothGattCharacteristic.PROPERTY_WRITE)==0){retry("Brak kanału HR/FTMS");return;}
            try{
                if(!g.setCharacteristicNotification(row,true)){retry("Nie można włączyć FTMS");return;}
                desc.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                deadline=SystemClock.elapsedRealtime()+8000;
                if(!g.writeDescriptor(desc))retry("Błąd subskrypcji FTMS");
            }catch(RuntimeException e){retry("Błąd subskrypcji");}
        });}
        @Override public void onDescriptorWrite(BluetoothGatt g,BluetoothGattDescriptor desc,int code){h.post(()->{
            if(g!=gatt||!CCCD.equals(desc.getUuid())||!ROW.equals(desc.getCharacteristic().getUuid()))return;
            if(code!=BluetoothGatt.GATT_SUCCESS){retry("CCCD: "+code);return;}
            ready=true;seq=0;writeAt=0;message="Połączono • wysyłam HR";
        });}
        @Override public void onCharacteristicChanged(BluetoothGatt g,BluetoothGattCharacteristic c){h.post(()->{if(g==gatt&&ROW.equals(c.getUuid()))rx++;});}
        @Override public void onCharacteristicWrite(BluetoothGatt g,BluetoothGattCharacteristic c,int code){h.post(()->{
            if(g!=gatt||!HR.equals(c.getUuid())||!pending)return;
            pending=false;
            if(code!=BluetoothGatt.GATT_SUCCESS){retry("Zapis HR: "+code);return;}
            acked++;if(lastSent!=0)validAck++;
        });}
    };
    private void send(long now){
        if(gatt==null||tx==null||!ready||pending)return;
        int measured=HrPacket.fresh(bpm,sampled,now)?bpm:0;
        byte[] packet=HrPacket.encode(seq,measured);
        try{
            tx.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
            tx.setValue(packet);pending=true;writeAt=now;
            if(!gatt.writeCharacteristic(tx)){pending=false;retry("Nie można wysłać HR");return;}
            lastSent=measured;seq=(seq+1)&0xffff;
        }catch(RuntimeException e){retry("Błąd zapisu HR");}
    }
}
