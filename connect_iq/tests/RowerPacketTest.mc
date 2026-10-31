import Toybox.Test;
(:test)
function originalPacket(logger){
    var parser=new RowerPacket();
    var r=parser.decode([0x1e,0x08,60,15,0,56,0xf4,1,0,120,0,130,0,60,0]b);
    Test.assert(r!=null);Test.assertEqual(r[:distance],500);Test.assertEqual(r[:pace],120);
    Test.assertEqual(r[:rate],30.0);Test.assertEqual(r[:strokes],15);Test.assertEqual(r[:elapsed],60);
    Test.assert(!r.hasKey(:power));return true;
}
(:test)
function optionalPowerAndInvalidPace(logger){
    var r=new RowerPacket().decode([0x7e,0x08,60,15,0,56,0xf4,1,0,255,255,130,0,100,0,90,0,60,0]b);
    Test.assert(r!=null);Test.assertEqual(r[:power],100);Test.assert(r[:pace]==null);return true;
}
(:test)
function truncatedAndUnexpectedFlags(logger){
    var parser=new RowerPacket();
    Test.assert(parser.decode([]b)==null);
    Test.assert(parser.decode([0x1e,8,60,15,0]b)==null);
    Test.assert(parser.decode([0,0xe0]b)==null);
    Test.assert(parser.decode([1,0,99]b)==null);
    return true;
}
(:test)
function scanNameComparison(logger){
    var name="WR S4BL3x".substring(0,8);
    logger.debug("Runtime name == literal: "+(name=="WR S4BL3"));
    var link=new RowerLink();
    Test.assert(link.isTargetName(name));
    Test.assert(!link.isTargetName(null));
    Test.assert(!link.isTargetName("Other rower"));return true;
}
(:test)
function negativeSystemTimer(logger){
    var link=new RowerLink();
    Test.assert(link.retryReady(-100000));
    link.retryAt=-50000;
    Test.assert(!link.retryReady(-51000));
    Test.assert(link.retryReady(-50000));
    Test.assert(link.retryReady(-49000));return true;
}
