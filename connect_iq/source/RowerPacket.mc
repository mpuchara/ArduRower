import Toybox.Lang;

// FTMS Rower Data uses whole seconds per 500 m and signed whole watts.
class RowerPacket {
    function initialize() {}
    function u16(b as ByteArray, p as Number) as Number { return b[p] | (b[p+1] << 8); }
    function s16(b as ByteArray, p as Number) as Number {
        var value=u16(b,p);
        return value>=0x8000?value-0x10000:value;
    }
    function decode(b as ByteArray) as Dictionary or Null {
        if (b.size() < 2) { return null; }
        var flags = u16(b, 0);
        // The CYD sends a complete record, never a More Data fragment.
        if ((flags & 0xe001) != 0) { return null; }
        var needed = 5;
        var sizes = [1,3,2,2,2,2,2,5,1,1,2,2];
        for (var bit=1; bit<=12; bit+=1) {
            if ((flags & (1<<bit)) != 0) { needed += sizes[bit-1]; }
        }
        if (b.size() != needed) { return null; }
        var result = {};
        result[:rate]=b[2]==0xff?null:b[2]/2.0;
        var strokes=u16(b,3);
        result[:strokes]=strokes==0xffff?null:strokes;
        var p=5;
        if ((flags & 2) != 0) { result[:avgRate]=b[p]==0xff?null:b[p]/2.0; p+=1; }
        if ((flags & 4) != 0) {
            var distance=b[p] | (b[p+1]<<8) | (b[p+2]<<16); p+=3;
            result[:distance]=distance==0xffffff?null:distance;
        }
        if ((flags & 8) != 0) {
            var pace = u16(b,p); p+=2;
            result[:pace] = (pace==0xffff || pace==0) ? null : pace;
        }
        if ((flags & 16) != 0) {
            var avgPace = u16(b,p); p+=2;
            result[:avgPace] = (avgPace==0xffff || avgPace==0) ? null : avgPace;
        }
        if ((flags & 32) != 0) { result[:power]=s16(b,p);p+=2; }
        if ((flags & 64) != 0) { result[:avgPower]=s16(b,p);p+=2; }
        if ((flags & 128) != 0) { p+=2; }
        if ((flags & 256) != 0) { p+=5; }
        if ((flags & 512) != 0) { p+=1; }
        if ((flags & 1024) != 0) { p+=1; }
        if ((flags & 2048) != 0) {
            var elapsed=u16(b,p);
            result[:elapsed]=elapsed==0xffff?null:elapsed;
        }
        return result;
    }
}
