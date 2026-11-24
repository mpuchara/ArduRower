package pl.ardurower.garetthr;
import java.util.Arrays;
public final class HrPacketTest {
    public static void main(String[] args){
        expect(HrPacket.encode(0x1234,124),new byte[]{1,1,0x34,0x12,124,0});
        expect(HrPacket.encode(65535,240),new byte[]{1,1,-1,-1,(byte)240,0});
        expect(HrPacket.encode(0,0),new byte[]{1,0,0,0,0,0});
        expect(HrPacket.encode(1,241),new byte[]{1,0,1,0,0,0});
        if(!HrPacket.fresh(120,1000,7500)||HrPacket.fresh(120,1000,7501)||HrPacket.fresh(120,-1,1000)||HrPacket.fresh(34,1000,1001)||HrPacket.fresh(120,2000,1000))throw new AssertionError("freshness");
        System.out.println("PASS: HR encoding, ranges, wrap and freshness");
    }
    static void expect(byte[] a,byte[] b){if(!Arrays.equals(a,b))throw new AssertionError(Arrays.toString(a));}
}
