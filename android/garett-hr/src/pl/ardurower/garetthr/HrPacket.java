package pl.ardurower.garetthr;

final class HrPacket {
    private HrPacket() {}
    static boolean validBpm(int bpm) { return bpm >= 35 && bpm <= 240; }
    static boolean fresh(int bpm, long sampleMs, long nowMs) {
        return validBpm(bpm) && sampleMs >= 0 && nowMs >= sampleMs && nowMs-sampleMs <= 6500;
    }
    static byte[] encode(int seq, int bpm) {
        boolean valid = validBpm(bpm);
        int value = valid ? bpm : 0;
        return new byte[]{1,(byte)(valid?1:0),(byte)seq,(byte)(seq>>>8),(byte)value,(byte)(value>>>8)};
    }
}
