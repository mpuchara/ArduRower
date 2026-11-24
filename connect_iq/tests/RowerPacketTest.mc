import Toybox.Test;
import Toybox.Application;
(:test)
function originalPacket(logger){
    var parser=new RowerPacket();
    var r=parser.decode([0x1e,0x08,60,15,0,56,0xf4,1,0,120,0,130,0,60,0]b);
    Test.assert(r!=null);Test.assertEqual(r[:distance],500);Test.assertEqual(r[:pace],120);
    Test.assertEqual(r[:rate],30.0);Test.assertEqual(r[:avgRate],28.0);
    Test.assertEqual(r[:strokes],15);Test.assertEqual(r[:avgPace],130);
    Test.assertEqual(r[:elapsed],60);
    Test.assert(!r.hasKey(:power));return true;
}
(:test)
function optionalPowerAndInvalidPace(logger){
    var r=new RowerPacket().decode([0x7e,0x08,60,15,0,56,0xf4,1,0,255,255,130,0,100,0,90,0,60,0]b);
    Test.assert(r!=null);Test.assertEqual(r[:power],100);Test.assert(r[:pace]==null);
    Test.assertEqual(r[:avgPace],130);Test.assertEqual(r[:avgRate],28.0);return true;
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

(:test)
function powerFlagsAndMissingValues(logger){
    var parser=new RowerPacket();
    var idle=parser.decode([0x3e,8,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0]b);
    Test.assertEqual(idle[:power],0);Test.assert(!idle.hasKey(:avgPower));
    Test.assertEqual(idle[:distance],0);Test.assert(idle[:pace]==null);
    var avg=parser.decode([0x5e,8,3,254,255,4,254,255,255,1,0,254,255,0x41,1,254,255]b);
    Test.assertEqual(avg[:avgPower],321);Test.assert(!avg.hasKey(:power));
    Test.assertEqual(avg[:distance],16777214);Test.assertEqual(avg[:elapsed],65534);
    var missed=parser.decode([0x1e,8,255,0,0,255,255,255,255,255,255,255,255,255,255]b);
    Test.assert(missed[:rate]==null && missed[:avgRate]==null && missed[:distance]==null);
    Test.assert(missed[:elapsed]==null && missed[:avgPace]==null);
    var signed=parser.decode([0x60,0,0,0,0,0xff,0xff,0x00,0x80]b);
    Test.assertEqual(signed[:power],-1);Test.assertEqual(signed[:avgPower],-32768);
    return true;
}

(:test)
function staleAndReconnect(logger){
    var link=new RowerLink();
    var packet=[0x7e,8,60,15,0,56,0xf4,1,0,120,0,130,0,145,0,94,0,60,0]b;
    Test.assert(link.acceptSample(packet,0));
    Test.assertEqual(link.fitSample(1000,-1)[:power],145);
    var recorded=link.revision;
    Test.assert(link.fitSample(1200,recorded)==null);
    Test.assert(!link.acceptSample([0x7e,8]b,1500));
    Test.assertEqual(link.revision,recorded);
    Test.assert(link.fitSample(2500,-1)==null);
    link.clearSample();
    Test.assert(link.fitSample(1000,-1)==null && link.data.size()==0);
    Test.assert(link.acceptSample(packet,3000));
    Test.assertEqual(link.fitSample(3000,recorded)[:distance],500);
    return true;
}

(:test)
class TestFitField {
    var samples=[];
    function initialize(){}
    function setData(value){samples.add(value);}
}

(:test)
function fitValuesAndMissingPower(logger){
    // The simulator's activity already allocated this view's FIT budget.
    // Reuse its fields before substituting sinks that expose written values.
    var field=Application.getApp().getInitialView()[0] as RowerField;
    var sample=new RowerPacket().decode(
        [0x7e,8,60,15,0,56,0xf4,1,0,125,0,130,0,145,0,94,0,60,0]b);
    Test.assert(sample!=null);
    field.writeSample(sample);
    field.fitDistance=new TestFitField();field.fitSpeed=new TestFitField();
    field.fitCadence=new TestFitField();field.fitPower=new TestFitField();
    field.fitStrokes=new TestFitField();field.fitElapsed=new TestFitField();
    field.fitAvgPace=new TestFitField();field.fitAvgCadence=new TestFitField();
    field.fitPace=new TestFitField();field.fitAvgPower=new TestFitField();
    field.writeSample(sample);
    Test.assertEqual(field.fitDistance.samples[0],500.0);
    Test.assertEqual(field.fitSpeed.samples[0],4.0);
    Test.assertEqual(field.fitCadence.samples[0],30.0);
    Test.assertEqual(field.fitPower.samples[0],145.0);
    Test.assertEqual(field.fitStrokes.samples[0],15);
    Test.assertEqual(field.fitElapsed.samples[0],60);
    Test.assertEqual(field.fitAvgPace.samples[0],130);
    Test.assertEqual(field.fitAvgCadence.samples[0],28.0);
    Test.assertEqual(field.fitPace.samples[0],125);
    Test.assertEqual(field.fitAvgPower.samples[0],94);
    field.writeSample({:power=>0});
    field.writeSample({:power=>-1,:pace=>null});
    Test.assertEqual(field.fitPower.samples.size(),2);
    Test.assertEqual(field.fitPower.samples[1],0.0);
    Test.assertEqual(field.fitPace.samples.size(),1);
    return true;
}
