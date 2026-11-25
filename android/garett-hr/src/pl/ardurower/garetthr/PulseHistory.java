package pl.ardurower.garetthr;
final class PulseHistory {
    private final int[] samples=new int[60];
    private long second=-1;
    void clear(){java.util.Arrays.fill(samples,0);second=-1;}
    void add(long now,int bpm){
        if(now<0)return;
        if(second<0||now<second||now-second>=samples.length){clear();}
        if(second>=0&&now>second){
            int d=(int)(now-second);
            System.arraycopy(samples,d,samples,0,samples.length-d);
            java.util.Arrays.fill(samples,samples.length-d,samples.length,0);
        }
        samples[samples.length-1]=HrPacket.validBpm(bpm)?bpm:0;second=now;
    }
    int[] snapshot(){return samples.clone();}
}
