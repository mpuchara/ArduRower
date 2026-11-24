import Toybox.Application;
import Toybox.WatchUi;
import Toybox.Activity;
import Toybox.Lang;
import Toybox.System;
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
    var recorded=-1, fitValidity=null;
    var fitDistance,fitSpeed,fitCadence,fitPower,fitLink;
    var fitStrokes,fitElapsed,fitAvgPace,fitAvgCadence,fitPace,fitAvgPower;

    function initialize(connection as RowerLink){
        DataField.initialize();link=connection;
        // Keep IDs and data types of fields already used by previous releases.
        fitDistance=createField("Rower distance",0,Fit.DATA_TYPE_FLOAT,{:units=>"m",:nativeNum=>5});
        fitSpeed=createField("Rower speed",1,Fit.DATA_TYPE_FLOAT,{:units=>"m/s",:nativeNum=>6});
        fitCadence=createField("Rower cadence",2,Fit.DATA_TYPE_FLOAT,{:units=>"spm",:nativeNum=>4});
        fitPower=createField("Rower equiv power",3,Fit.DATA_TYPE_FLOAT,{:units=>"W",:nativeNum=>7});
        fitLink=createField("Rower BLE valid",4,Fit.DATA_TYPE_UINT8,{:units=>""});
        fitStrokes=createField("Rower strokes",5,Fit.DATA_TYPE_UINT16,{:units=>"strokes"});
        fitElapsed=createField("Rower active time",6,Fit.DATA_TYPE_UINT16,{:units=>"s"});
        fitAvgPace=createField("Rower average 500m pace",7,Fit.DATA_TYPE_UINT16,{:units=>"s/500m"});
        fitAvgCadence=createField("Rower average cadence",8,Fit.DATA_TYPE_FLOAT,{:units=>"spm"});
        fitPace=createField("Rower 500m pace",9,Fit.DATA_TYPE_UINT16,{:units=>"s/500m"});
        fitAvgPower=createField("Rower avg equiv power",10,Fit.DATA_TYPE_UINT16,{:units=>"W"});
    }

    function compute(info){
        link.tick(info==null?null:info.currentHeartRate);
        var now=System.getTimer();
        var valid=link.freshAt(now,2500)?1:0;
        var d=link.fitSample(now,recorded);
        if(valid!=fitValidity || d!=null){
            fitValidity=valid;
            fitLink.setData(valid);
        }
        if(d!=null){
            recorded=link.revision;
            writeSample(d);
        }
    }

    function writeSample(d as Dictionary){
        // Garmin writes setData at the next recording opportunity. Skip
        // unavailable values and never re-submit the previous packet.
        if(d[:distance]!=null){fitDistance.setData(d[:distance].toFloat());}
        if(d[:rate]!=null){fitCadence.setData(d[:rate].toFloat());}
        if(d[:pace]!=null){
            fitPace.setData(d[:pace]);
            fitSpeed.setData(500.0/d[:pace]);
        }
        if(d[:power]!=null && d[:power]>=0){fitPower.setData(d[:power].toFloat());}
        if(d[:strokes]!=null){fitStrokes.setData(d[:strokes]);}
        if(d[:elapsed]!=null){fitElapsed.setData(d[:elapsed]);}
        if(d[:avgPace]!=null){fitAvgPace.setData(d[:avgPace]);}
        if(d[:avgRate]!=null){fitAvgCadence.setData(d[:avgRate].toFloat());}
        if(d[:avgPower]!=null && d[:avgPower]>=0){fitAvgPower.setData(d[:avgPower]);}
    }

    function clockText(seconds){
        if(seconds==null){return "--:--";}
        return (seconds/60).toNumber().format("%d")+":"+(seconds%60).format("%02d");
    }

    function onUpdate(dc){render(dc,getBackgroundColor());}

    function fitText(dc,text,compact,width){
        if(dc.getTextWidthInPixels(text,G.FONT_XTINY)<=width){return text;}
        if(dc.getTextWidthInPixels(compact,G.FONT_XTINY)<=width){return compact;}
        return "";
    }

    function render(dc,background){
        var w=dc.getWidth(),h=dc.getHeight(),width=w-8;
        var foreground=background==G.COLOR_BLACK?G.COLOR_WHITE:G.COLOR_BLACK;
        dc.setColor(foreground,background);dc.clear();
        var fontHeight=dc.getFontHeight(G.FONT_XTINY);
        if(h<fontHeight || w<16){return;}

        var valid=link.fresh();
        if(!valid){
            var state=fitText(dc,link.subscribed?"Brak danych":link.state,"BLE --",width);
            dc.drawText(w/2,(h-fontHeight)/2,G.FONT_XTINY,state,G.TEXT_JUSTIFY_CENTER);
            return;
        }
        var d=link.data;
        var distance=d[:distance]==null?"-- m":d[:distance].format("%d")+" m";
        if(d[:distance]!=null && d[:distance]>=10000){
            distance=(d[:distance]/1000.0).format("%.1f")+" km";
        }
        var pace=clockText(d[:pace]);
        var speed=d[:pace]==null?"--":(1800.0/d[:pace]).format("%.1f");
        var power=d[:power]==null || d[:power]<0?"-- W":d[:power].format("%d")+" W";
        var rate=d[:rate]==null?"--":d[:rate].format("%.0f");
        var pulse=link.latestHr==null || link.latestHr<35 || link.latestHr>240?
            "--":link.latestHr.format("%d");
        var strokes=d[:strokes]==null?"--":d[:strokes].format("%d");
        var active=clockText(d[:elapsed]);
        var detail=fitText(dc,rate+" spm  "+pulse+" bpm",rate+"  HR "+pulse,width);
        var strokeTime=fitText(dc,strokes+" ruch  "+active,strokes+"  "+active,width);
        var paceLine=fitText(dc,pace+" /500m",pace,width);
        var rows=1;
        while(rows<6 && h>=(rows+1)*fontHeight+rows*2+4){rows+=1;}

        var lines;
        if(rows==6){
            lines=[distance,paceLine,power,fitText(dc,speed+" km/h",speed,width),strokeTime,detail];
        } else if(rows==5){
            lines=[distance,paceLine,fitText(dc,power+"  "+speed+" km/h",power,width),strokeTime,detail];
        } else if(rows==4){
            lines=[distance,paceLine,power,detail];
        } else if(rows==3){
            lines=[distance,paceLine,power];
        } else if(rows==2){
            var distancePace=fitText(dc,distance+"  "+pace,distance,width);
            var pacePower=distancePace.equals(distance)?fitText(dc,pace+"  "+power,power,width):power;
            lines=[distancePace,pacePower];
        } else {
            lines=[fitText(dc,distance+" "+pace+" "+power,fitText(dc,pace+" "+power,power,width),width)];
        }
        var y=(h-rows*fontHeight-(rows-1)*2)/2;
        for(var i=0;i<rows;i+=1){
            var line=fitText(dc,lines[i],"",width);
            if(line.length()>0){dc.drawText(w/2,y,G.FONT_XTINY,line,G.TEXT_JUSTIFY_CENTER);}
            y+=fontHeight+2;
        }
    }
}
