package pl.ardurower.garetthr;
import java.util.Arrays;
public final class HrPacketTest {
    public static void main(String[] args){
        expect(HrPacket.encode(0x1234,124),new byte[]{1,1,0x34,0x12,124,0});
        expect(HrPacket.encode(65535,240),new byte[]{1,1,-1,-1,(byte)240,0});
        expect(HrPacket.encode(0,0),new byte[]{1,0,0,0,0,0});
        expect(HrPacket.encode(1,241),new byte[]{1,0,1,0,0,0});
        if(!HrPacket.fresh(120,1000,7500)||HrPacket.fresh(120,1000,7501)||HrPacket.fresh(120,-1,1000)||HrPacket.fresh(34,1000,1001)||HrPacket.fresh(120,2000,1000))throw new AssertionError("freshness");
        if(!HrPacket.usable(120,1)||HrPacket.usable(120,0)||HrPacket.usable(120,-1)||HrPacket.usable(Float.NaN,3)||HrPacket.usable(300,3))throw new AssertionError("accuracy");
        if(!HrPacket.sensorFresh(120,1000,46000,true)||HrPacket.sensorFresh(120,1000,46001,true)||HrPacket.sensorFresh(120,1000,7501,false))throw new AssertionError("on change");
        if(HrPacket.sampleTime(0,10000,9000)!=10000||HrPacket.sampleTime(50000,10000,9000)!=10000||HrPacket.sampleTime(1000,10000,9000)!=-1||HrPacket.sampleTime(9500,10000,9000)!=9500)throw new AssertionError("timestamp");
        if(HrPacket.sampleTimeWithUptime(99000,200000,100000,198000)!=199000||HrPacket.sampleTimeWithUptime(1000,200000,100000,198000)!=-1||HrPacket.sampleTimeWithUptime(199000,200000,100000,198000)!=199000)throw new AssertionError("uptime");
        if(!HrPacket.plausible(85)||HrPacket.usable(85,0)||HrPacket.usable(85,-1))throw new AssertionError("raw must not be transmitted as reliable");
        System.out.println("PASS: HR encoding, ranges, wrap and freshness");
    }
    static void expect(byte[] a,byte[] b){if(!Arrays.equals(a,b))throw new AssertionError(Arrays.toString(a));}
}
