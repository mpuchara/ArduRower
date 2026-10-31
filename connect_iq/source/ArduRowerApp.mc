import Toybox.Application;
import Toybox.WatchUi;
import Toybox.Activity;
import Toybox.Lang;
using Toybox.Graphics as G;
using Toybox.FitContributor as Fit;

class ArduRowerApp extends Application.AppBase {
    var link;
    function initialize(){AppBase.initialize();}
    function onStart(state){link=new RowerLink();link.start();}
    function onStop(state){if(link!=null){link.stop();}}
    function getInitialView(){return [new RowerField(link)];}
}

class RowerField extends WatchUi.DataField {
    var link as RowerLink;
    var hr=null, recorded=-1;
    var fitDistance,fitSpeed,fitCadence,fitPower,fitLink;
    function initialize(connection as RowerLink){
        DataField.initialize();link=connection;
        fitDistance=createField("Rower distance",0,Fit.DATA_TYPE_FLOAT,{:units=>"m",:nativeNum=>5});
        fitSpeed=createField("Rower speed",1,Fit.DATA_TYPE_FLOAT,{:units=>"m/s",:nativeNum=>6});
        fitCadence=createField("Rower cadence",2,Fit.DATA_TYPE_FLOAT,{:units=>"spm",:nativeNum=>4});
        fitPower=createField("Rower equiv power",3,Fit.DATA_TYPE_FLOAT,{:units=>"W",:nativeNum=>7});
        fitLink=createField("Rower BLE valid",4,Fit.DATA_TYPE_UINT8,{:units=>""});
    }
    function compute(info){
        hr=(info==null)?null:info.currentHeartRate;link.tick(hr);
        fitLink.setData(link.fresh()?1:0);
        if(link.fresh() && recorded!=link.revision){
            recorded=link.revision;var d=link.data;
            if(d.hasKey(:distance)){fitDistance.setData(d[:distance].toFloat());}
            if(d.hasKey(:rate)){fitCadence.setData(d[:rate]);}
            if(d.hasKey(:pace) && d[:pace]!=null){fitSpeed.setData(500.0/d[:pace]);}
            if(d.hasKey(:power) && d[:power]>=0){fitPower.setData(d[:power].toFloat());}
        }
    }
    function paceText(){
        if(!link.fresh() || !link.data.hasKey(:pace) || link.data[:pace]==null){return "--:--";}
        var s=link.data[:pace];return (s/60).toNumber().format("%d")+":"+(s%60).format("%02d");
    }
    function onUpdate(dc){
        render(dc,getBackgroundColor());
    }
    function fitText(dc,text,compact,width){
        if(dc.getTextWidthInPixels(text,G.FONT_XTINY)<=width){return text;}
        if(dc.getTextWidthInPixels(compact,G.FONT_XTINY)<=width){return compact;}
        return "";
    }
    function render(dc,background){
        var w=dc.getWidth(),h=dc.getHeight();
        var foreground=background==G.COLOR_BLACK?G.COLOR_WHITE:G.COLOR_BLACK;
        dc.setColor(foreground,background);dc.clear();
        var fontHeight=dc.getFontHeight(G.FONT_XTINY);
        if(h<fontHeight || w<16){return;}
        var distance=link.fresh() && link.data.hasKey(:distance) ? link.data[:distance].format("%d")+" m" : "-- m";
        var rate=link.fresh() && link.data.hasKey(:rate) ? link.data[:rate].format("%.0f") : "--";
        var pulse=link.latestHr==null || link.latestHr<35 || link.latestHr>240 ? "--" : link.latestHr.format("%d");
        var pace=paceText();
        var width=w-8;
        var shortDistance=distance;
        if(link.fresh() && link.data.hasKey(:distance) && link.data[:distance]>=10000){
            shortDistance=(link.data[:distance]/1000.0).format("%.1f")+" km";
        }
        var detail=fitText(dc,rate+" spm  "+pulse+" bpm",rate+"  HR "+pulse,width);
        if(!link.fresh()){detail=link.device!=null?"Laczenie":"Szukam";}
        var rows=3;
        if(h<3*fontHeight+8){rows=2;}
        if(h<2*fontHeight+6){rows=1;}
        var lines;
        if(rows==3){
            lines=[fitText(dc,distance,shortDistance,width),
                   fitText(dc,pace+" /500m",pace,width),detail];
        } else if(rows==2){
            lines=[fitText(dc,distance+"  "+pace,shortDistance,width),detail];
        } else {
            lines=[fitText(dc,distance+"  "+pace,shortDistance,width)];
        }
        // The same small font everywhere. Actual font metrics determine
        // how many rows fit; no fixed-position labels or large numerals.
        var blockHeight=rows*fontHeight+(rows-1)*2;
        var y=(h-blockHeight)/2;
        for(var i=0;i<rows;i+=1){
            if(lines[i].length()>0){dc.drawText(w/2,y,G.FONT_XTINY,lines[i],G.TEXT_JUSTIFY_CENTER);}
            y+=fontHeight+2;
        }
    }
}
