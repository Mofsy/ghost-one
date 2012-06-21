// Global Variables for Ajax Support
var AreaUpdateName = "updateArea";
var lastline = "0";
var lastresult ="";
var timer = setTimeout("checkchat();",100);
var lastcmd = false;

/*@cc_on @if (@_win32 && @_jscript_version >= 5) if (!window.XMLHttpRequest)
function XMLHttpRequest() {
return new ActiveXObject('Microsoft.XMLHTTP');
}
@end @*/ 

function makeRequestID2(url)
{
	var xhr2;
	if (window.XMLHttpRequest) {
		xhr2 = new XMLHttpRequest();
		//if (xhr2.overrideMimeType) {
		//			xhr2.overrideMimeType('text/xml');

	} 
/*	else 
	{
		if (window.ActiveXObject) 
		{
			try 
			{
				xhr2 = new ActiveXObject("Msxml2.XMLHTTP");
			} catch(e)
			{
				try
				{
					xhr2 = new ActiveXObject("Microsoft.XMLHTTP");
				} catch(e) {}
			}
		}
	}*/
	if(xhr2) 
	{
		xhr2.onreadystatechange = function () {showContentsID2(xhr2);};
		xhr2.open("GET",url,true);
		xhr2.setRequestHeader( "If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT" );
//		xhr2.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");     
		xhr2.send(null);
	} else 
	{
		lastresult="";
	}
}

function showContentsID2(xhr2) 
{
	var outMsg = "";
	if (xhr2.readyState == 4) 
	{
		if (xhr2.status == 200) 
		{
			outMsg = xhr2.responseText;
		} else 
		{
			outMsg = "Error : " + xhr2.status;
		}
		lastresult = outMsg;
	}
}

function makeRequestID(url,updatePanelID) 
{
		var xhr;
		if (window.XMLHttpRequest) 
		{
			xhr = new XMLHttpRequest();
			//				if (xhr.overrideMimeType) {
			//					xhr.overrideMimeType('text/xml');

		} 
/*		else
		{ 		
			if (window.ActiveXObject) 
			{
				try {
					xhr = new ActiveXObject("Msxml2.XMLHTTP");
				} catch(e)
				{
					try
					{
						xhr = new ActiveXObject("Microsoft.XMLHTTP");
					} catch(e) {}
				}
			}
		}*/
		if(xhr) 
		{
			var panel = AreaUpdateName;
			if (updatePanelID != "") { panel  = updatePanelID; }
			xhr.onreadystatechange = function () {showContentsID(xhr, panel);};
			xhr.open("GET",url,true);
			xhr.setRequestHeader( "If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT" );
			//xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");     
			xhr.send(null);
		} else 
		{
			document.getElementById(AreaUpdateName).innerHtml = "Error Making Ajax Request";
		}
}

function showContentsID(xhr, updatePanelID) 
{
		var outMsg;
		if (xhr.readyState == 4) 
		{
			if (xhr.status == 200) 
			{
				outMsg = xhr.responseText;
			} else 
			{
				outMsg = "Error : " + xhr.status;
			}
			document.getElementById(updatePanelID).innerHTML = outMsg;
		}
}

function checkEnter(e)
{ //e is event object passed from function invocation
		var characterCode;

		if(e && e.which)
		{ //if which property of event object is supported (NN4)
			//			e = e;
			characterCode = e.which; //character code is contained in NN4's which property
		}
		else
		{
				//		e = event;
			characterCode = e.keyCode; //character code is contained in IE's keyCode property
		}

		if(characterCode == 13)
		{ //if generated character code is equal to ascii 13 (if enter key)
			return false;
		}
		else
		{
			return true;
		}
}

function checkchat()
{
		makeRequestID2('/checkchat');
		if (lastresult!=lastline)
		{
			lastline= lastresult;
			var p = "";
		 	if (document.getElementById("pass1").value)
		 	{
		 		p = document.getElementById("pass1").value;
			}
			makeRequestID('/chat?pass='+p,'chat');
		}

		clearTimeout(timer);
		if (lastcmd)
		{
			timer = setTimeout("checkchat();",50);
			lastcmd = false;			
		} else
		{
			timer = setTimeout("checkchat();",1500);
		}
}

function loadcommand(e)
{
		if (checkEnter(e)) { return; }
		var q = document.getElementById("command");
		var p = "";
		 	if (document.getElementById("pass1").value)
		 	{
		 		p = document.getElementById("pass1").value;
			}
			if(q && q!="")
			{
				makeRequestID('/cmd?pass='+p+'&cmd='+escape(q.value),'');
				document.getElementById("command").value="";
				clearTimeout(timer);
				timer = setTimeout("checkchat();",150);
				lastcmd = true;
			}
			else
			{
				alert('no command!');
			}
}

function loadcommand2(e)
{
		if (checkEnter(e)) { return; }
		var q = document.getElementById("command2");
		var p = "";
		 	if (document.getElementById("pass1").value)
		 	{
		 		p = document.getElementById("pass1").value;
			}
			if(q && q!="")
			{
				makeRequestID('/cmd?pass='+p+'&cmd=say '+escape(q.value),'');
				document.getElementById("command2").value="";
				clearTimeout(timer);
				timer = setTimeout("checkchat();",150);
				lastcmd = true;
//				clearTimeout(timer);
				//checkchat();				
			}
			else
			{
				alert('no command!');
			}
}