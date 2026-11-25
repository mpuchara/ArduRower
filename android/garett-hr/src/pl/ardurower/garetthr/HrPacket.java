package pl.ardurower.garetthr;

final class HrPacket {
    private HrPacket() {}
    static boolean validBpm(int bpm) { return bpm >= 35 && bpm <= 240; }
    static boolean fresh(int bpm, long sampleMs, long nowMs) {
        return validBpm(bpm) && sampleMs >= 0 && nowMs >= sampleMs && nowMs-sampleMs <= 6500;
    }
    static boolean usable(float value,int accuracy){
        return accuracy>0 && Float.isFinite(value) && value>=35 && value<=240;
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
