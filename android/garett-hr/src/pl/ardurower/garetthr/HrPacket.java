package pl.ardurower.garetthr;

final class HrPacket {
    private HrPacket() {}
    static boolean validBpm(int bpm) { return bpm >= 35 && bpm <= 240; }
    static boolean fresh(int bpm, long sampleMs, long nowMs) {
        return validBpm(bpm) && sampleMs >= 0 && nowMs >= sampleMs && nowMs-sampleMs <= 6500;
    }
    static String hex(float value){return String.format(java.util.Locale.US,"%08X",Float.floatToRawIntBits(value));}
    static String bytes(float value){
        int bits=Float.floatToRawIntBits(value);
        return ((bits>>>24)&255)+" / "+((bits>>>16)&255)+" / "+((bits>>>8)&255)+" / "+(bits&255);
    }
    // Observed Garett layout hypothesis: [oxygen-like, HR-like, upper, lower].
    // Only recognize the observed structured range, never reinterpret ordinary
    // BPM, no-contact events or arbitrary invalid floats. Garett mode is enabled by default after the user's hardware trial.
    // The signature and accuracy checks remain mandatory.
    static int packedBpm(float value,int accuracy){
        if(accuracy<=0||!Float.isFinite(value)||plausible(value)||value<1e12f)return 0;
        int bits=Float.floatToRawIntBits(value);
        int a=(bits>>>24)&255,b=(bits>>>16)&255,c=(bits>>>8)&255,d=bits&255;
        if(a<70||a>100||!validBpm(b)||c<80||c>220||d<35||d>140||c<=d)return 0;
        return b;
    }
    static boolean plausible(float value){return Float.isFinite(value)&&value>=35&&value<=240;}
    static boolean usable(float value,int accuracy){return accuracy>0&&plausible(value);}
    static long sampleTimeWithUptime(long stamp,long elapsed,long uptime,long registeredAt){
        long normal=sampleTime(stamp,elapsed,registeredAt);
        if(normal>=0)return normal;
        // Some old vendor HALs use CLOCK_MONOTONIC rather than BOOTTIME.
        // Translate only if the original timestamp is near current uptime.
        if(stamp>0&&uptime>=stamp&&uptime-stamp<=6500&&elapsed>=uptime){
            long corrected=stamp+(elapsed-uptime);
            if(corrected>=registeredAt-1000&&corrected<=elapsed)return corrected;
        }
        return -1;
    }
    // Some vendor HALs report zero or a different epoch. Only a newly
    // delivered valid event may use receipt time; never refresh on BLE writes.
    static long sampleTime(long stamp,long now,long registeredAt){
        if(stamp<=0 || stamp>now+1000)return now;
        if(stamp>now)return now;
        if(stamp<registeredAt-1000 || now-stamp>10000)return -1;
        return stamp;
    }
    static boolean sensorFresh(int bpm,long sampleMs,long nowMs,boolean onChange){
        return validBpm(bpm)&&sampleMs>=0&&nowMs>=sampleMs&&nowMs-sampleMs<=(onChange?45000:6500);
    }
    static byte[] encode(int seq, int bpm) {
        boolean valid = validBpm(bpm);
        int value = valid ? bpm : 0;
        return new byte[]{1,(byte)(valid?1:0),(byte)seq,(byte)(seq>>>8),(byte)value,(byte)(value>>>8)};
    }
}
