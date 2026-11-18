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
    var fitStrokes,fitElapsed,fitAvgPace,fitAvgCadence;
    function initialize(connection as RowerLink){
        DataField.initialize();link=connection;
        fitDistance=createField("Rower distance",0,Fit.DATA_TYPE_FLOAT,{:units=>"m",:nativeNum=>5});
        fitSpeed=createField("Rower speed",1,Fit.DATA_TYPE_FLOAT,{:units=>"m/s",:nativeNum=>6});
        fitCadence=createField("Rower cadence",2,Fit.DATA_TYPE_FLOAT,{:units=>"spm",:nativeNum=>4});
        fitPower=createField("Rower equiv power",3,Fit.DATA_TYPE_FLOAT,{:units=>"W",:nativeNum=>7});
        fitLink=createField("Rower BLE valid",4,Fit.DATA_TYPE_UINT8,{:units=>""});
        fitStrokes=createField("Rower strokes",5,Fit.DATA_TYPE_UINT16,{:units=>"strokes"});
        fitElapsed=createField("Rower active time",6,Fit.DATA_TYPE_UINT16,{:units=>"s"});
        fitAvgPace=createField("Rower average 500m pace",7,Fit.DATA_TYPE_UINT16,{:units=>"s/500m"});
        fitAvgCadence=createField("Rower average cadence",8,Fit.DATA_TYPE_FLOAT,{:units=>"spm"});
    }
    function compute(info){
        hr=(info==null)?null:info.currentHeartRate;
        link.tick(hr);
        var valid=link.fresh();
        fitLink.setData(valid?1:0);
        if(valid && recorded!=link.revision){
            recorded=link.revision;var d=link.data;
            if(d.hasKey(:distance)){fitDistance.setData(d[:distance].toFloat());}
            if(d.hasKey(:rate)){fitCadence.setData(d[:rate].toFloat());}
            if(d.hasKey(:pace) && d[:pace]!=null){fitSpeed.setData(500.0/d[:pace]);}
            if(d.hasKey(:power) && d[:power]>=0){fitPower.setData(d[:power].toFloat());}
            if(d.hasKey(:strokes)){fitStrokes.setData(d[:strokes]);}
            if(d.hasKey(:elapsed)){fitElapsed.setData(d[:elapsed]);}
            if(d.hasKey(:avgPace) && d[:avgPace]!=null){fitAvgPace.setData(d[:avgPace]);}
            if(d.hasKey(:avgRate)){fitAvgCadence.setData(d[:avgRate].toFloat());}
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
        var valid=link.fresh(),d=link.data;
        var distance=valid && d.hasKey(:distance) ? d[:distance].format("%d")+" m" : "-- m";
        var shortDistance=distance;
        if(valid && d.hasKey(:distance) && d[:distance]>=10000){
            shortDistance=(d[:distance]/1000.0).format("%.1f")+" km";
        }
        var pace=paceText();
        var speedValue=valid && d.hasKey(:pace) && d[:pace]!=null ?
            (1800.0/d[:pace]).format("%.1f") : "--";
        var speed=speedValue+" km/h";
        var rate=valid && d.hasKey(:rate) ? d[:rate].format("%.0f") : "--";
        var pulse=link.latestHr==null || link.latestHr<35 || link.latestHr>240 ?
            "--" : link.latestHr.format("%d");
        var strokes=valid && d.hasKey(:strokes) ? d[:strokes].format("%d") : "--";
        var active="--:--";
        if(valid && d.hasKey(:elapsed)){
            var secs=d[:elapsed];
            active=(secs/60).toNumber().format("%d")+":"+(secs%60).format("%02d");
        }
        var width=w-8;
        var detail=fitText(dc,rate+" spm  "+pulse+" bpm",rate+"  HR "+pulse,width);
        if(!valid){detail=link.device!=null?"Laczenie":"Szukam";}
        var paceLine=fitText(dc,pace+" /500m",pace,width);
        var paceSpeed=fitText(dc,pace+"  "+speed,pace+"  "+speedValue,width);
        if(paceSpeed.length()==0){paceSpeed=paceLine;}
        var strokeTime=fitText(dc,strokes+" ruch  "+active,strokes+" ruch",width);
        var rows=3;
        if(h>=6*fontHeight+16){rows=6;}
        else if(h>=5*fontHeight+14){rows=5;}
        else if(h>=4*fontHeight+12){rows=4;}
        else if(h<3*fontHeight+8){rows=2;}
        if(h<2*fontHeight+6){rows=1;}
        var lines;
        if(rows==6){
            lines=[fitText(dc,distance,shortDistance,width),paceLine,
                   fitText(dc,speed,speedValue,width),fitText(dc,strokes+" ruch",strokes,width),
                   fitText(dc,active+" aktywnie",active,width),detail];
        } else if(rows==5){
            lines=[fitText(dc,distance,shortDistance,width),paceLine,
                   fitText(dc,speed,speedValue,width),strokeTime,detail];
        } else if(rows==4){
            lines=[fitText(dc,distance,shortDistance,width),paceSpeed,strokeTime,detail];
        } else if(rows==3){
            lines=[fitText(dc,distance,shortDistance,width),paceSpeed,detail];
        } else if(rows==2){
            lines=[fitText(dc,distance+"  "+pace,shortDistance,width),detail];
        } else {
            lines=[fitText(dc,distance+"  "+pace,shortDistance,width)];
        }
        var blockHeight=rows*fontHeight+(rows-1)*2;
        var y=(h-blockHeight)/2;
        for(var i=0;i<rows;i+=1){
            if(lines[i].length()>0){dc.drawText(w/2,y,G.FONT_XTINY,lines[i],G.TEXT_JUSTIFY_CENTER);}
            y+=fontHeight+2;
        }
    }
}
