//Get the websocketserver to connect to
var getUrlParameter = function getUrlParameter(sParam) {
    var sPageURL = decodeURIComponent(window.location.search.substring(1)),
    sURLVariables = sPageURL.split('&'),
    sParameterName,
    i;

    for (i = 0; i < sURLVariables.length; i++) {
        sParameterName = sURLVariables[i].split('=');

        if (sParameterName[0] === sParam) {
            return sParameterName[1] === undefined ? true : sParameterName[1];
        }
    }
};
ws1 = getUrlParameter('ws1');
if(ws1 == undefined || ws1 == '') {
    ws1 = location.host;
    if(ws1 == '') {
        ws1 = 'ws://127.0.0.1:81';
    } else {
        ws1 = 'ws://'+ws1.split(":")[0]+':81';
    }
}

//Set up the websocket connection
var mysocket = new WebSocket(ws1);
var heartbeat_msg = '>', heartbeat_interval = null, missed_heartbeats = 0;

/*
 * === Receive Data from Websocket
 */
function onMessage(evt) {
    // Reset missed heartbeat - connection alive
    missed_heartbeats=0;

    // Show we are connected
    wsconnected();

    // Processed the received data
    var answer = evt.data[0];
    var answerdata = null;
    if(evt.data.length > 1) {
        answerdata = evt.data.slice(1);
    } else {
        answerdata = "[no data received]";
    }
    messages = document.getElementById("messages").value;
    switch(answer) {
        case '<': // Yihah still alive, don't update the messages field though
            break;
        case 'e': case 'E': // Error message
            document.getElementById("messages").value = messages + "Error! --> " + answerdata + "\n";
            break;
        case 'h': // heapsize
            document.getElementById("messages").value = messages + "Free heap size: " + answerdata  + "\n";
            break;
        case 'i': case 'I': // Info message
            document.getElementById("messages").value = messages + "Info --> " + answerdata + "\n";
            break;
        case 'p': //pong message
            document.getElementById("messages").value = messages + "pong" + "\n";
            break;
        default:
            document.getElementById("messages").value = messages + evt.data + "\n";
            break;
    }
}
function wsconnected() {
    $('#settingstext').css('color', '#048C00');
    $('#connected').removeClass('hidden');
    $('#notconnected').addClass('hidden');
}
// When the websocket connection fails
function wslost(msg) {
    $('#settingstext').css('color', '#ff0000');
    $('#connected').addClass('hidden');
    $('#notconnected').removeClass('hidden');
    document.getElementById("messages").value = document.getElementById("messages").value + "Closing connection: " + msg + "\n";
    mysocket.close();
}


/*
 * === Send Data to Websocket
 * first value in the Uint8Array is the 'command'
 * the rest is used to set the pixels
 */

// setSinglecolor = 1
function sendsinglecolor(hexcolor) {
	var color = new Uint8Array(4);
	color[0] = 1; //Singlecolor
	color[1] = parseInt(hexcolor.substr(0,2), 16);
	color[2] = parseInt(hexcolor.substr(2,2), 16);
	color[3] = parseInt(hexcolor.substr(4,2), 16);
	mysocket.binaryType = 'arraybuffer';
	mysocket.send(color);
}

// setGradient = 2
function sendgradient() {
	var colors = new Uint8Array(7);
    var gradientbox = document.getElementById('gradientbox');
    var leftcolor = document.getElementById('gradientbuttonleft').jscolor.toHEXString();
    var rightcolor = document.getElementById('gradientbuttonright').jscolor.toHEXString();
    gradientbox.style.backgroundImage = '-webkit-linear-gradient(' + 'left' + ', ' + leftcolor + ', ' + rightcolor + ')';
    gradientbox.style.backgroundImage = '-o-linear-gradient(' + 'right' + ', ' + leftcolor + ', ' + rightcolor + ')';
    gradientbox.style.backgroundImage = '-moz-linear-gradient(' + 'left' + ', ' + leftcolor + ', ' + rightcolor + ')';
    gradientbox.style.backgroundImage = 'linear-gradient(' + 'to right' + ', ' + leftcolor + ', ' + rightcolor + ')';

	colors[0] = 2; // Gradient
	colors[1] = parseInt(leftcolor.substr(1,2), 16);
	colors[2] = parseInt(leftcolor.substr(3,2), 16);
	colors[3] = parseInt(leftcolor.substr(5,2), 16);
	colors[4] = parseInt(rightcolor.substr(1,2), 16);
	colors[5] = parseInt(rightcolor.substr(3,2), 16);
	colors[6] = parseInt(rightcolor.substr(5,2), 16);
	mysocket.binaryType = 'arraybuffer';
	mysocket.send(colors);
}

// Hide all others when opening a section/view
$(".nav a").on('click',function(e) {
    e.preventDefault();
    $('.container').collapse('hide');
    $( $(this).attr('href') ).collapse('show');
    $(this).parent().addClass('active').siblings().removeClass('active');
});

// On small screens close the collapsed menu after a click
$('.navbar-collapse a').click(function(){
    $(".navbar-collapse").collapse('hide');
});

$(document).ready(function() {
    //Bind websocket functions
    mysocket.onopen = function () {
    // TODO: Get the current settings from the websocket server
        if (heartbeat_interval === null) {
            missed_heartbeats = 0;
            heartbeat_interval = setInterval(function() {
                try {
                    missed_heartbeats++;
                    if (missed_heartbeats >= 3)
                        throw new Error("Too many missed heartbeats.");
                    mysocket.send(heartbeat_msg);
                } catch(e) {
                    clearInterval(heartbeat_interval);
                    heartbeat_interval = null;
                    wslost(e.message);
                    console.warn("Closing connection. Reason: " + e.message);
                }
            }, 3000);
        }
    };
    mysocket.onerror = function () {
        wslost("error");
    };
    mysocket.onmessage = function(evt) { 
        onMessage(evt)
    };

    //Add singlecolor functionality
    $('#singlecolor').on('shown.bs.collapse', function() {
        var singlecolorwidth = $("#singlecolor").width() - 64;
        var singlecolorheight = $(window).height() - $('#singlecolor').offset().top - $("#singlecolor").outerHeight(true) - 64;
        var singlecolorpicker = document.getElementById('singlecolorbutton').jscolor;
        singlecolorpicker.width = singlecolorwidth;
        singlecolorpicker.height = singlecolorheight;
        singlecolorpicker.redraw();
        singlecolorpicker.show();
    });

    //Gradient
    $('#gradient').on('shown.bs.collapse', function() {
        var gradientwidth = $("#gradientleftcol").width() - 64;
        var gradientheight = $(window).height() - $('#gradientleftcol').offset().top - $("#gradientleftcol").outerHeight(true) - 64;
        var gradientcolorpickerleft = document.getElementById('gradientbuttonleft').jscolor;
        var gradientcolorpickerright = document.getElementById('gradientbuttonright').jscolor;
        gradientcolorpickerleft.width = gradientwidth;
        gradientcolorpickerleft.height = gradientheight;
        gradientcolorpickerright.width = gradientwidth;
        gradientcolorpickerright.height = gradientheight;
        gradientcolorpickerright.redraw();
        gradientcolorpickerleft.redraw();
        gradientcolorpickerleft.show()
    });

    // Settings
    $('#ws1').text(ws1);
    $('#websocket1').val(ws1);
    $('#websocket1button').click( function(e) {
        e.preventDefault();
        ws1 = $('#websocket1').val();
        window.location = location.protocol + '//' + location.host + location.pathname + "?ws1=" + ws1;
    });
    $('#pingbutton').click( function(e) {
        e.preventDefault();
        mysocket.send('>ping');
    });
    $('#heapbutton').click( function(e) {
        e.preventDefault();
        mysocket.send('heap');
    });
    $('#clearbutton').click( function(e) {
        e.preventDefault();
        $('#messages').val('');
    });
    $( "#websocket1name" ).change(function() {
        $("#websitename").text($("#websocket1name").val());
    });


    // Setting of output modes
	$("input[name=gammacorrection]").change(function () {
		if(this.id == "yes") {
			mysocket.send('cy');
		}
		else if(this.id == "no") {
			mysocket.send('cn');
		}
	});
	$("input[name=singlecolormode]").change(function () {
		if(this.id == "all") {
			mysocket.send('sa');
		}
		else if(this.id == "one") {
			mysocket.send('sc');
		}
	});
	$("input[name=gradientorientation]").change(function () {
		if(this.id == "horizontal") {
			mysocket.send('oh');
		}
		else if(this.id == "vertical") {
			mysocket.send('ov');
		} else if(this.id == "consecutive") {
			mysocket.send('oc');
		}
	});

    // LED configuration
    $( "#pixelcolumns" ).change(function() {
    });
    $( "#pixelrows" ).change(function() {
    });
    $('#setpanelbutton').click( function(e) {
        mysocket.send('pr'+$("#pixelrows").val());
        mysocket.send('pc'+$("#pixelcolumns").val());
    });

    // Wifi reset
    $('#resetbutton').click( function(e) {
        mysocket.send('wr');
    });

    // Scroll to the correct subsection
    $('#settingslinks').find(('a[href^="#"]')).on('click',function (e) {
        e.preventDefault();

        var target = this.hash;
        var $target = $(target);

        $('html, body').stop().animate({
            'scrollTop': $target.offset().top
        }, 900, 'swing')
    });

    //Show the page after everything has been loaded and set
    jQuery(window).load(function () {
        $("body").fadeIn("slow");
    });
});
