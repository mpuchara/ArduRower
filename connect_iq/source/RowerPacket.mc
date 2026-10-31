import Toybox.Lang;

// WR S4BL3 compatibility: its legacy pace words contain WHOLE seconds/500m,
// not the 0.1 s units used by generic FTMS. Do not apply this to other rowers.
class RowerPacket {
    function initialize() {}
    function u16(b as ByteArray, p as Number) as Number { return b[p] | (b[p+1] << 8); }
    function decode(b as ByteArray) as Dictionary or Null {
        if (b.size() < 2) { return null; }
        var flags = u16(b, 0);
        if ((flags & 0xe000) != 0) { return null; }
        var needed = 2;
        if ((flags & 1) == 0) { needed += 3; }
        var sizes = [1,3,2,2,2,2,2,5,1,1,2,2];
        for (var bit=1; bit<=12; bit+=1) {
            if ((flags & (1<<bit)) != 0) { needed += sizes[bit-1]; }
        }
        if (b.size() != needed) { return null; }
        var result = {};
        var p=2;
        if ((flags & 1) == 0) {
            result[:rate] = b[p]/2.0;
            result[:strokes] = u16(b,p+1); p+=3;
        }
        if ((flags & 2) != 0) { p+=1; }
        if ((flags & 4) != 0) {
            result[:distance] = b[p] | (b[p+1]<<8) | (b[p+2]<<16); p+=3;
        }
        if ((flags & 8) != 0) {
            var pace = u16(b,p); p+=2;
            result[:pace] = (pace==0xffff || pace==0) ? null : pace;
        }
        if ((flags & 16) != 0) { p+=2; }
        if ((flags & 32) != 0) {
            var power=u16(b,p);p+=2;
            result[:power]=(power>32767) ? power-65536 : power;
        }
        if ((flags & 64) != 0) { p+=2; }
        if ((flags & 128) != 0) { p+=2; }
        if ((flags & 256) != 0) { p+=5; }
        if ((flags & 512) != 0) { p+=1; }
        if ((flags & 1024) != 0) { p+=1; }
        if ((flags & 2048) != 0) { result[:elapsed]=u16(b,p);p+=2; }
        return result;
    }
}
