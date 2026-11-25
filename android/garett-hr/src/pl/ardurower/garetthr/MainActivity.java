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
import android.graphics.*;
import android.view.View;
import android.view.MotionEvent;
import android.graphics.drawable.GradientDrawable;
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
    private TextView value,status,counts,sensorStatus,details,rawView;
    private Button connect;
    private LinearLayout advanced;
    private HeartView heartView;
    private TrendView trendView;
    private final PulseHistory history=new PulseHistory();
    private long lastTouch=0,lastChartSecond=-1,lastAckAt=-1;
    private int lastAckBpm=0;
    private boolean dimmed=false;
    private static final long DIM_AFTER_MS=15000;
    private static final float DIM_LEVEL=0.06f;
    private int dp(float v){return Math.round(v*getResources().getDisplayMetrics().density);}
    private void setDim(boolean dim){
        if(dimmed==dim)return;
        dimmed=dim;
        WindowManager.LayoutParams p=getWindow().getAttributes();
        p.screenBrightness=dim?DIM_LEVEL:WindowManager.LayoutParams.BRIGHTNESS_OVERRIDE_NONE;
        getWindow().setAttributes(p);
        if(heartView!=null)heartView.invalidate();
    }
    @Override public boolean dispatchTouchEvent(MotionEvent e){
        lastTouch=SystemClock.elapsedRealtime();
        setDim(false);
        return super.dispatchTouchEvent(e);
    }
    private void reconnect(){
        wanted=true;stopScan();closeGatt();
        message="Ponowne łączenie";
        nextAttempt=SystemClock.elapsedRealtime()+800;
        askPermissions();
        paint(SystemClock.elapsedRealtime());
    }
    private GradientDrawable rounded(int color,int radius){
        GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));return d;
    }
    private Button action(LinearLayout parent,String label,int background){
        Button b=new Button(this);b.setText(label);b.setAllCaps(false);b.setTextSize(15);
        b.setTextColor(Color.WHITE);b.setBackground(rounded(background,14));b.setMinHeight(dp(46));
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,dp(46));lp.topMargin=dp(8);
        parent.addView(b,lp);return b;
    }
    private SensorManager sensors;
    private Sensor heart;
    private final java.util.ArrayList<Sensor> heartSensors=new java.util.ArrayList<>();
    private int heartIndex=0,rawAccuracy=-999,rawEvents=0,validEvents=0,invalidEvents=0,regAttempts=0;
    private long registeredAt=-1,lastEventAt=-1,lastGoodAt=-1,nextSensorAttempt=0,rawTimestamp=-1;
    private float rawBpm=Float.NaN,lastPlausible=Float.NaN;
    private int noContact=0,unreliable=0,badValue=0,badTime=0,uptimeAdjusted=0,accuracyCallback=-999;
    private long lastAnyEvent=-1,lastPlausibleAt=-1;
    private final java.util.ArrayDeque<String> recent=new java.util.ArrayDeque<>();
    private boolean slowSampling=false;
    private boolean packedConfirmed=true,packedReading=false;
    private int packedCandidate=0;
    private int packedCount=0;
    private Button packedButton;
    private void refreshPackedButton(){
        packedButton.setText(packedConfirmed?"Tryb Garett: włączony":"Tryb Garett: wyłączony");
    }
    private void recordRaw(String reason,long now){
        rawNote=reason;
        if(recent.size()>=8)recent.removeFirst();
        recent.addLast("BPM "+rawBpm+" a="+rawAccuracy+" t="+rawTimestamp+" | "+reason);
    }
    private boolean permissionAsked=false;
    private String registrationNote="",rawNote="Brak zdarzeń";
    private BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic tx;
    private boolean active,scanning,ready,pending,registered;
    private boolean wanted=true;
    private long sampled=-1,deadline=0,nextAttempt=0,writeAt=0;
    private int bpm=0,seq=0,rx=0,acked=0,validAck=0,samples=0,lastSent=0;
    private String message="Szukam komputerka", sensorMessage="Sprawdzanie";
    private final Runnable ticker=new Runnable(){public void run(){
        if(!active)return;
        long now=SystemClock.elapsedRealtime();
        if(wanted){
            if(scanning && now>=deadline){stopScan(); retry("Nie znaleziono ArduRower");}
            else if(gatt!=null && ((!ready && now>=deadline)||(pending && now-writeAt>5000))){retry("Timeout BLE");}
            else if(gatt==null && !scanning && now>=nextAttempt){startScan();}
            else if(ready && !pending && now-writeAt>=1000){send(now);}
        }
        maintainHeart(now);
        setDim(now-lastTouch>=DIM_AFTER_MS);
        long second=now/1000;
        if(second!=lastChartSecond){
            history.add(second,heartFresh(now)?bpm:0);lastChartSecond=second;
            if(!dimmed)trendView.invalidate();
        }
        paint(now);
        h.postDelayed(this,250);
    }};
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        sensors=(SensorManager)getSystemService(SENSOR_SERVICE);
        discoverHeartSensors();
        BluetoothManager m=(BluetoothManager)getSystemService(BLUETOOTH_SERVICE);
        adapter=m==null?null:m.getAdapter();
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);root.setPadding(12,8,12,8);root.setBackgroundColor(0xff07111d);
        scroll.addView(root);setContentView(scroll);
        TextView heading=add(root,"ArduRower",19,Color.WHITE);
        heading.setTypeface(null,android.graphics.Typeface.BOLD);
        LinearLayout pulseRow=new LinearLayout(this);
        pulseRow.setGravity(Gravity.CENTER);pulseRow.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams pulseLp=new LinearLayout.LayoutParams(-1,dp(76));pulseLp.topMargin=dp(5);
        root.addView(pulseRow,pulseLp);
        heartView=new HeartView();pulseRow.addView(heartView,new LinearLayout.LayoutParams(dp(54),dp(62)));
        value=new TextView(this);value.setText("--");value.setTextSize(58);value.setTextColor(Color.WHITE);
        value.setGravity(Gravity.CENTER_VERTICAL);value.setTypeface(null,android.graphics.Typeface.BOLD);
        pulseRow.addView(value,new LinearLayout.LayoutParams(-2,-1));
        add(root,"uderzeń/min",13,0xffa7bac9);
        sensorStatus=add(root,"Mierzę puls…",13,0xffa7bac9);
        trendView=new TrendView();LinearLayout.LayoutParams trendLp=new LinearLayout.LayoutParams(-1,dp(74));trendLp.topMargin=dp(9);
        root.addView(trendView,trendLp);
        add(root,"Ostatnia minuta",11,0xffa7bac9);
        status=add(root,message,15,0xff56dec5);
        status.setPadding(0,dp(8),0,0);
        connect=action(root,"Połącz ponownie",0xff185e59);
        connect.setOnClickListener(v->reconnect());
        TextView settings=add(root,"Ustawienia i diagnostyka",12,0xff98aec0);
        settings.setPadding(0,dp(14),0,dp(10));settings.setMinHeight(dp(44));settings.setClickable(true);
        advanced=new LinearLayout(this);advanced.setOrientation(LinearLayout.VERTICAL);
        root.addView(advanced,new LinearLayout.LayoutParams(-1,-2));advanced.setVisibility(View.GONE);
        settings.setOnClickListener(v->{
            boolean show=advanced.getVisibility()!=View.VISIBLE;
            advanced.setVisibility(show?View.VISIBLE:View.GONE);
            settings.setText(show?"Ukryj ustawienia":"Ustawienia i diagnostyka");
            if(show)listSensors();
        });
        add(advanced,"ArduRower HR 0.1.5 • pomiar sportowy",12,0xffa7bac9);
        rawView=add(advanced,"Surowy HR: --",12,0xffffc66d);
        counts=add(advanced,"",12,0xffc0cbd9);
        packedButton=action(advanced,"",0xff293c50);
        refreshPackedButton();
        packedButton.setOnClickListener(v->{
            packedConfirmed=!packedConfirmed;invalidateHeart();refreshPackedButton();
        });
        Button restart=action(advanced,"Uruchom pomiar ponownie",0xff293c50);
        restart.setOnClickListener(v->{if(!granted(Manifest.permission.BODY_SENSORS)){requestPermissions(new String[]{Manifest.permission.BODY_SENSORS},7);return;}restartHeart(false);});
        Button next=action(advanced,slowSampling?"Tryb pomiaru: 1 s":"Tryb pomiaru: szybki",0xff293c50);
        next.setOnClickListener(v->{slowSampling=!slowSampling;next.setText(slowSampling?"Tryb pomiaru: 1 s":"Tryb pomiaru: szybki");restartHeart(false);});
        Button disconnect=action(advanced,"Rozłącz",0xff293c50);
        disconnect.setOnClickListener(v->{wanted=false;stopScan();closeGatt();message="Rozłączono";paint(SystemClock.elapsedRealtime());});
        details=add(advanced,"",12,0xffc0cbd9);details.setGravity(Gravity.START);
        details.setVisibility(View.GONE);
        Button diag=action(advanced,"Szczegóły czujnika",0xff293c50);
        diag.setOnClickListener(v->{boolean show=details.getVisibility()!=View.VISIBLE;details.setVisibility(show?View.VISIBLE:View.GONE);if(show)listSensors();});
        lastTouch=SystemClock.elapsedRealtime();

    }
    private TextView add(LinearLayout root,String s,int sp,int color){
        TextView t=new TextView(this);t.setGravity(Gravity.CENTER);t.setText(s);t.setTextSize(sp);t.setTextColor(color);
        root.addView(t,new LinearLayout.LayoutParams(-1,-2));return t;
    }
    @Override protected void onStart(){super.onStart();active=true;wanted=true;nextAttempt=0;lastTouch=SystemClock.elapsedRealtime();setDim(false);discoverHeartSensors();startHeart();if(!permissionAsked){permissionAsked=true;askPermissions();}h.removeCallbacks(ticker);h.post(ticker);}
    @Override protected void onResume(){super.onResume();if(active&&!registered&&granted(Manifest.permission.BODY_SENSORS)){discoverHeartSensors();startHeart();}}
    @Override public void onWindowFocusChanged(boolean hasFocus){super.onWindowFocusChanged(hasFocus);if(hasFocus&&active&&!registered&&granted(Manifest.permission.BODY_SENSORS)){discoverHeartSensors();startHeart();}}
    @Override protected void onStop(){active=false;h.removeCallbacks(ticker);stopScan();closeGatt();stopHeart();setDim(false);history.clear();lastChartSecond=-1;super.onStop();}
    private boolean granted(String p){return checkSelfPermission(p)==PackageManager.PERMISSION_GRANTED;}
    private void askPermissions(){
        java.util.ArrayList<String> needed=new java.util.ArrayList<>();
        if(!granted(Manifest.permission.BODY_SENSORS))needed.add(Manifest.permission.BODY_SENSORS);
        if(wanted&&!granted(Manifest.permission.ACCESS_FINE_LOCATION))needed.add(Manifest.permission.ACCESS_FINE_LOCATION);
        if(!needed.isEmpty())requestPermissions(needed.toArray(new String[0]),7);
    }
    @Override public void onRequestPermissionsResult(int req,String[] p,int[] result){super.onRequestPermissionsResult(req,p,result);if(granted(Manifest.permission.BODY_SENSORS)){discoverHeartSensors();startHeart();}nextAttempt=0;}
    private void addHeart(Sensor sensor){if(sensor!=null&&!heartSensors.contains(sensor))heartSensors.add(sensor);}
    private void discoverHeartSensors(){
        if(sensors==null||registered)return;
        heartSensors.clear();
        try{
            addHeart(sensors.getDefaultSensor(Sensor.TYPE_HEART_RATE,false));
            addHeart(sensors.getDefaultSensor(Sensor.TYPE_HEART_RATE,true));
            for(Sensor x:sensors.getSensorList(Sensor.TYPE_HEART_RATE))addHeart(x);
        }catch(RuntimeException e){registrationNote="Lista HR: "+e.getClass().getSimpleName();}
        if(heartIndex>=heartSensors.size())heartIndex=0;
        heart=heartSensors.isEmpty()?null:heartSensors.get(heartIndex);
    }
    private void stopHeart(){
        if(sensors!=null)try{sensors.unregisterListener(this);}catch(RuntimeException ignored){}
        registered=false;registeredAt=-1;lastEventAt=-1;lastGoodAt=-1;invalidateHeart();
    }
    private void restartHeart(boolean next){
        stopHeart();discoverHeartSensors();
        if(next&&!heartSensors.isEmpty()){heartIndex=(heartIndex+1)%heartSensors.size();heart=heartSensors.get(heartIndex);}
        nextSensorAttempt=0;startHeart();
    }
    private void maintainHeart(long now){
        if(!active||!granted(Manifest.permission.BODY_SENSORS))return;
        if(!registered){if(now>=nextSensorAttempt){discoverHeartSensors();startHeart();}return;}
        // Try another standard HR sensor if the current source does not produce
        // a valid reading even after a full optical warm-up. Do not re-register
        // during the first 60 seconds. A valid on-change value does not require
        // new events every second.
        // Do not restart an optical measurement that is actively producing
        // events: the vendor may need a longer warm-up. Only retry silence.
        long reference=lastEventAt>=0?lastEventAt:registeredAt;
        if(reference>=0 && now-reference>=120000 && !heartFresh(now)){
            restartHeart(heartSensors.size()>1);
        }
    }
    private boolean heartFresh(long now){
        return registered&&HrPacket.sensorFresh(bpm,sampled,now,heart!=null&&heart.getReportingMode()==Sensor.REPORTING_MODE_ON_CHANGE);
    }
    private void startHeart(){
        if(!active||registered)return;
        long now=SystemClock.elapsedRealtime();
        nextSensorAttempt=now+10000;
        if(!granted(Manifest.permission.BODY_SENSORS)){sensorMessage="Brak zgody na pulsometr";return;}
        if(heart==null){sensorMessage="Brak publicznego czujnika HR";return;}
        try {
            regAttempts++;
            // Explicit Handler, zero batching, request immediate events.
            registered=sensors.registerListener(this,heart,slowSampling?1000000:SensorManager.SENSOR_DELAY_FASTEST,0,h);
            registeredAt=registered?now:-1;
            // Preserve the previous raw event and rejection counters on restart.
            if(rawEvents==0)rawNote="Czekam na zdarzenia";
            registrationNote=registered?"Rejestracja OK":"registerListener=false";
            sensorMessage=registered?"Pomiar: przyłóż zegarek do skóry":"Czujnik nie uruchomił się";
            if(!registered&&!heartSensors.isEmpty()){heartIndex=(heartIndex+1)%heartSensors.size();heart=heartSensors.get(heartIndex);}
        }catch(RuntimeException e){registrationNote=e.getClass().getSimpleName();sensorMessage="Błąd czujnika: "+registrationNote;}
    }
    private void invalidateHeart(){bpm=0;sampled=-1;packedReading=false;}
    @Override public void onSensorChanged(SensorEvent event){
        if(!active||!registered||event.sensor!=heart)return;
        long now=SystemClock.elapsedRealtime();
        samples++;rawEvents++;lastEventAt=now;lastAnyEvent=now;rawAccuracy=event.accuracy;
        rawBpm=event.values.length>0?event.values[0]:Float.NaN;
        rawTimestamp=event.timestamp/1000000L;
        packedCandidate=HrPacket.packedBpm(rawBpm,rawAccuracy);
        if(packedCandidate>0)packedCount++;
        if(HrPacket.plausible(rawBpm)){lastPlausible=rawBpm;lastPlausibleAt=now;}
        if(packedCandidate>0 && packedConfirmed){
            long stamp=HrPacket.sampleTimeWithUptime(rawTimestamp,now,SystemClock.uptimeMillis(),registeredAt);
            if(stamp<0){invalidEvents++;badTime++;invalidateHeart();recordRaw("Garett: stara próbka",now);sensorMessage=rawNote;return;}
            bpm=packedCandidate;sampled=stamp;lastGoodAt=now;validEvents++;packedReading=true;
            recordRaw("Garett: dekodowany BPM (test)",now);sensorMessage="Garett: dekoder zweryfikowany w próbie użytkownika";return;
        }
        if(packedCandidate>0){invalidEvents++;invalidateHeart();recordRaw("Garett: kandydat "+packedCandidate+" BPM",now);sensorMessage="Potwierdź format po porównaniu";return;}
        if(!HrPacket.usable(rawBpm,rawAccuracy)){
            invalidEvents++;invalidateHeart();
            if(rawAccuracy<0){noContact++;recordRaw("Brak kontaktu",now);}
            else if(!HrPacket.plausible(rawBpm)){badValue++;recordRaw("BPM poza zakresem",now);}
            else {unreliable++;recordRaw("Accuracy 0 - niewiarygodne",now);}
            sensorMessage=rawNote;return;
        }
        long uptime=SystemClock.uptimeMillis();
        long stamp=HrPacket.sampleTimeWithUptime(rawTimestamp,now,uptime,registeredAt);
        if(stamp<0){invalidEvents++;badTime++;invalidateHeart();recordRaw("Stara próbka (timestamp)",now);sensorMessage=rawNote;return;}
        if(stamp!=HrPacket.sampleTime(rawTimestamp,now,registeredAt))uptimeAdjusted++;
        bpm=Math.round(rawBpm);sampled=stamp;lastGoodAt=now;validEvents++;packedReading=false;
        recordRaw(stamp!=rawTimestamp?"OK; korekta zegara HAL":"OK",now);
        sensorMessage="Czujnik: "+event.sensor.getName();
    }
    @Override public void onAccuracyChanged(Sensor sensor,int accuracy){
        if(!active||sensor!=heart)return;
        accuracyCallback=accuracy;
        // Do not overwrite the status of the last raw sample with a callback.
        if(accuracy<=0){invalidateHeart();sensorMessage=accuracy<0?"Brak kontaktu":"Niewiarygodna dokładność";}
    }
    private void listSensors(){
        StringBuilder b=new StringBuilder("Android "+android.os.Build.VERSION.RELEASE+" / API "+android.os.Build.VERSION.SDK_INT+"\n"+android.os.Build.MODEL+"\nPuls: "+sensorMessage+"\n");
        long now=SystemClock.elapsedRealtime();
        b.append("BODY_SENSORS: ").append(granted(Manifest.permission.BODY_SENSORS)).append("\n");
        b.append("HR standard: ").append(heartSensors.size()).append("; wybrany: ").append(heartIndex+1).append("\n");
        b.append("Rejestracja: ").append(registrationNote).append("; próby: ").append(regAttempts).append("\n");
        if(heart!=null)b.append(heart.getName()).append(" / ").append(heart.getVendor()).append(" / tryb ").append(heart.getReportingMode()).append(" / wake=").append(heart.isWakeUpSensor()).append("\n");
        b.append("Surowy BPM: ").append(Float.isNaN(rawBpm)?"--":Float.toString(rawBpm)).append(" / accuracy: ").append(rawAccuracy==-999?"--":Integer.toString(rawAccuracy)).append("\n");
        b.append("Zdarzenia: ").append(rawEvents).append(" / ważne: ").append(validEvents).append(" / odrzucone: ").append(invalidEvents).append("\n");
        b.append("HEX: ").append(HrPacket.hex(rawBpm)).append(" / bajty: ").append(HrPacket.bytes(rawBpm)).append("\n");
        b.append("Garett kandydat BPM: ").append(packedCandidate).append(" / pakiety: ").append(packedCount).append(" / tryb: ").append(packedConfirmed).append("\n");
        b.append("Ostatnie: ").append(rawNote).append("\n");
        b.append("Odrzucone: kontakt ").append(noContact).append(", a0 ").append(unreliable).append(", BPM ").append(badValue).append(", czas ").append(badTime).append("\n");
        b.append("Accuracy callback: ").append(accuracyCallback).append(" / korekty uptime: ").append(uptimeAdjusted).append("\n");
        b.append("Ostatni BPM w zakresie: ").append(lastPlausible).append(" / wiek ").append(lastPlausibleAt<0?"--":Long.toString((now-lastPlausibleAt)/1000)+" s").append("\n");
        b.append("Ostatnie zdarzenia (takze po restarcie):\n");
        for(String line:recent)b.append(line).append("\n");
        b.append("Wiek zdarzenia: ").append(lastAnyEvent<0?"--":Long.toString((now-lastAnyEvent)/1000)+" s").append("\n");
        b.append("Czas HAL: ").append(rawTimestamp).append(" / zegar: ").append(now).append("\n");
        b.append("Wszystkie czujniki:\n");
        if(sensors!=null)for(Sensor s:sensors.getSensorList(Sensor.TYPE_ALL))b.append(s.getName()).append(" / ").append(s.getVendor()).append(" / ").append(s.getType()).append(" / ").append(s.getStringType()).append(" / tryb ").append(s.getReportingMode()).append('\n');
        details.setText(b.toString());
    }
    private void paint(long now){
        boolean fresh=heartFresh(now);
        value.setText(fresh?String.valueOf(bpm):"--");
        sensorStatus.setText(fresh?"Puls zmierzony":!granted(Manifest.permission.BODY_SENSORS)?"Zezwól na pomiar pulsu":"Mierzę puls…");
        heartView.setPulse(fresh?bpm:0);
        boolean delivered=ready&&fresh&&lastAckBpm>0&&lastAckAt>=0&&now-lastAckAt<=3500;
        String label;
        if(delivered)label="Połączono • puls wysyłany";
        else if(ready)label=fresh?"Połączono • wysyłam…":"Połączono • mierzę puls";
        else if(!wanted)label="Rozłączono";
        else if(adapter==null||!adapter.isEnabled()||!granted(Manifest.permission.ACCESS_FINE_LOCATION)||message.contains("lokalizacj"))label=message;
        else if(gatt!=null)label="Łączę z komputerkiem…";
        else label="Szukam komputerka…";
        status.setText(label);
        status.setTextColor(delivered?0xff56dec5:0xffe9bd71);
        if(advanced.getVisibility()==View.VISIBLE){
            rawView.setText("Surowy HR: "+(Float.isNaN(rawBpm)?"--":Float.toString(rawBpm))+" | a="+rawAccuracy+"\\n"+rawNote+"\\n"+sensorMessage);
            counts.setText("Próbki HR: "+samples+" | FTMS: "+rx+"\\nHR ACK: "+acked+" (BPM: "+validAck+")\\nOstatni ACK: "+(lastAckBpm==0?"brak":lastAckBpm+" BPM")+"\\n"+message);
            if(details.getVisibility()==View.VISIBLE)listSensors();
        }
    }
    private final class HeartView extends View {
        private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path=new Path();
        private int current=0;
        HeartView(){super(MainActivity.this);}
        void setPulse(int value){
            if(current!=value){current=value;invalidate();}
            else if(!dimmed&&current>0)invalidate();
        }
        @Override protected void onDraw(Canvas canvas){
            super.onDraw(canvas);
            float scale=Math.min(getWidth(),getHeight())/100f;
            float beat=1f;
            if(current>0&&!dimmed){
                double phase=(SystemClock.elapsedRealtime()%(60000.0/current))/(60000.0/current);
                beat=1f+0.13f*(float)Math.exp(-Math.pow((phase-0.16)/0.11,2));
            }
            canvas.save();canvas.translate(getWidth()/2f,getHeight()/2f);canvas.scale(scale*beat,scale*beat);
            path.reset();path.moveTo(0,31);
            path.cubicTo(-11,20,-42,-1,-42,-18);
            path.cubicTo(-42,-43,-12,-47,0,-25);
            path.cubicTo(12,-47,42,-43,42,-18);
            path.cubicTo(42,-1,11,20,0,31);path.close();
            paint.setColor(current>0?0xffff627e:0xff586475);paint.setStyle(Paint.Style.FILL);
            canvas.drawPath(path,paint);canvas.restore();
            if(active&&current>0&&!dimmed)postInvalidateDelayed(50);
        }
    }
    private final class TrendView extends View {
        private final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
        TrendView(){super(MainActivity.this);setBackground(rounded(0xff101f30,12));}
        @Override protected void onDraw(Canvas canvas){
            super.onDraw(canvas);
            float l=dp(9),r=getWidth()-dp(9),t=dp(9),b=getHeight()-dp(9);
            if(r<=l||b<=t)return;
            int[] points=history.snapshot();
            int lo=240,hi=35,n=0;
            for(int v:points)if(HrPacket.validBpm(v)){lo=Math.min(lo,v);hi=Math.max(hi,v);n++;}
            p.setColor(0xff294053);p.setStrokeWidth(dp(1));canvas.drawLine(l,(t+b)/2,r,(t+b)/2,p);
            if(n==0){p.setColor(0xff9aafbf);p.setTextSize(dp(12));p.setTextAlign(Paint.Align.CENTER);canvas.drawText("Czekam na pomiar",getWidth()/2f,(t+b)/2-dp(4),p);return;}
            lo=Math.max(30,lo-8);hi=Math.min(250,Math.max(hi+8,lo+20));
            p.setColor(0xffff627e);p.setStrokeWidth(dp(2));p.setStrokeCap(Paint.Cap.ROUND);
            boolean prev=false;float px=0,py=0;
            for(int i=0;i<points.length;i++){
                int v=points[i];if(!HrPacket.validBpm(v)){prev=false;continue;}
                float x=l+(r-l)*i/(points.length-1f),y=b-(b-t)*(v-lo)/(hi-lo);
                if(prev)canvas.drawLine(px,py,x,y,p);else canvas.drawCircle(x,y,dp(2),p);
                px=x;py=y;prev=true;
            }
        }
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
        BluetoothGatt old=gatt;gatt=null;tx=null;ready=false;pending=false;seq=0;lastSent=0;lastAckAt=-1;lastAckBpm=0;
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
            acked++;lastAckBpm=lastSent;lastAckAt=SystemClock.elapsedRealtime();if(lastSent!=0)validAck++;
        });}
    };
    private void send(long now){
        if(gatt==null||tx==null||!ready||pending)return;
        int measured=heartFresh(now)?bpm:0;
        byte[] packet=HrPacket.encode(seq,measured);
        try{
            tx.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
            tx.setValue(packet);pending=true;writeAt=now;
            if(!gatt.writeCharacteristic(tx)){pending=false;retry("Nie można wysłać HR");return;}
            lastSent=measured;seq=(seq+1)&0xffff;
        }catch(RuntimeException e){retry("Błąd zapisu HR");}
    }
}
