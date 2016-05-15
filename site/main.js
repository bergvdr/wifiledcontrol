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
var heartbeat_msg = 'p', heartbeat_interval = null, missed_heartbeats = 0;

/*
 * === Receive Data from Websocket
 */
function onMessage(evt) {
    // Show we are connected
    missed_heartbeats=0;
    $('#settingstext').css('color', '#048C00');

    // Processed the received data
    var answer = evt.data[0];
    var answerdata = null;
    if(evt.data.length > 1) {
        answerdata = evt.data.slice(1);
    } else {
        answerdata = "[no data received]";
    }
    messages = document.getElementById("messages").value;
    // console.log(answer);
    switch(answer) {
        case 'a': //yihah still alive, don't update the messages field though
            break;
        case 'E': //error message
            document.getElementById("messages").value = messages + "Error! --> " + answerdata + "\n";
            break;
        case 'h': //heapsize
            document.getElementById("messages").value = messages + "Free heap size: " + answerdata  + "\n";
            break;
        case 'p': //pong message
            document.getElementById("messages").value = messages + "pong" + "\n";
            break;
        default:
            document.getElementById("messages").value = messages + evt.data + "\n";
            break;
    }
}

/*
 * === Send Data to Websocket
 */
function sendsinglecolor(color) {
    mysocket.send('s'+color);
}

function sendgrad() {
    var gradientbox = document.getElementById('gradientbox');
    var leftcolor = document.getElementById('gradientbutton1').innerText;
    var rightcolor = document.getElementById('gradientbutton2').innerText;
    gradientbox.style.backgroundImage = '-webkit-linear-gradient(' + 'left' + ', #' + leftcolor + ', #' + rightcolor + ')';
            gradientbox.style.backgroundImage = '-o-linear-gradient(' + 'right' + ', #' + leftcolor + ', #' + rightcolor + ')';
                gradientbox.style.backgroundImage = '-moz-linear-gradient(' + 'left' + ', #' + leftcolor + ', #' + rightcolor + ')';
                    gradientbox.style.backgroundImage = 'linear-gradient(' + 'to right' + ', #' + leftcolor + ', #' + rightcolor + ')';
                        }

                        function sendindividual(ledcolors) {
                            mysocket.send(ledcolors);
                        }
                        function sendindividualone(r,c,ledcolor) {
                            mysocket.send("updating row "+r+" and col "+c+" with color #"+ledcolor);
                        }

                        function drawindividual() {
                            var nrofcolumns = document.getElementById('individualcolumns').value;
                            var nrofrows = document.getElementById('individualrows').value;
                            var saturation = document.getElementById('saturation').value;
                            var value = document.getElementById('value').value;

                            $("#individualcontainer").empty();

                            var ledcolors = '';
                            var tot = nrofrows*nrofcolumns;
                            var steps = 360/tot;
                            for(var r = 0; r < nrofrows; r++) {
                                var tr = document.createElement('tr');
                                for(var c = 0; c < nrofcolumns; c++) {
                                    var td = document.createElement('td');
                                    var input = document.createElement('DIV');
                                    input.className = "individualbutton"
                                        var picker = new jscolor(input, {closable:'true', onFineChange:'sendindividualone('+r+','+c+',this)'});
                                    picker.fromHSV(tot*steps, saturation, value);
                                    ledcolors += picker;
                                    td.appendChild(input);
                                    tr.appendChild(td);
                                    tot -= 1;
                                }
                                $("#individualcontainer").append(tr);
                            }

                            //sendindividual(ledcolors);
                        }

// Hide all others when opening a section/view
$(".nav a").on('click',function(e) {
    e.preventDefault();
    $('.container').collapse('hide');
    $( $(this).attr('href') ).collapse('show');
    $(this).parent().addClass('active').siblings().removeClass('active');
});

// The same for when a view is called from the settings page
function changeview(caller) {
    var linkactive = $('.nav').find('a[href="'+caller+'"]');
    $(linkactive).parent().addClass('active').siblings().removeClass('active');
    caller = $(caller);
    $('.container').collapse('hide');
    caller.collapse('show');
}
$('#individualrows').keypress(function(event){
    var keycode = (event.keyCode ? event.keyCode : event.which);
    if(keycode == '13'){
        changeview('#individual');
    }
});
$('#individualcolumns').keypress(function(event){
    var keycode = (event.keyCode ? event.keyCode : event.which);
    if(keycode == '13'){
        changeview('#individual');
    }
});


// On small screens close the collapsed menu after a click
$('.navbar-collapse a').click(function(){
    $(".navbar-collapse").collapse('hide');
});

function wslost(msg = "error") {
    $('#settingstext').css('color', '#ff0000');
    $('#connected').addClass('hidden');
    $('#notconnected').removeClass('hidden');
    document.getElementById("messages").value = document.getElementById("messages").value + "Closing connection: " + msg + "\n";
    mysocket.close();
}

$(document).ready(function() {
    //Get the current settings from the websocket server
    mysocket.onopen = function () {
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
    mysocket.onclose = function () {
    };
    mysocket.onerror = function () {
        wslost("error");
    };
    mysocket.onmessage = function(evt) { 
        onMessage(evt)
    };

    //Add singlecolor functionality
    var $sbutton = $("<div id='singlecolorbutton' class='singlecolorbutton'>");
    $sbutton.appendTo($("#singlecolor"));
    var sColorPicker = new jscolor(document.getElementById('singlecolorbutton'), {closable:'true', onFineChange:'sendsinglecolor(this)', value:'FFAA11'});

    $('#singlecolor').on('shown.bs.collapse', function() {
        var singlecolorwidth = $("#singlecolor").width() - 64;
        var singlecolorheight = $(window).height() - $('#singlecolor').offset().top - $("#singlecolor").outerHeight(true) - 64;
        sColorPicker.width = singlecolorwidth;
        sColorPicker.height = singlecolorheight;
        sColorPicker.redraw();
        sColorPicker.show();
    });


    //Gradient
    var $gradientbutton1 = $("<div id='gradientbutton1' class='gradientbutton'>");
    var $gradientbutton2 = $("<div id='gradientbutton2' class='gradientbutton'>");
    $gradientbutton1.appendTo($("#gradientleftcol"));
    $gradientbutton2.appendTo($("#gradientrightcol"));

    var gradcolpick1 = new jscolor(document.getElementById('gradientbutton1'), {closable:'true', onFineChange:'sendgrad()', value:'F82000'});
    var gradcolpick2 = new jscolor(document.getElementById('gradientbutton2'), {closable:'true', onFineChange:'sendgrad()', value:'00028F'});

    $('#gradient').on('shown.bs.collapse', function() {
        var gradientwidth = $("#gradientleftcol").width() - 64;
        gradheight = $(window).height() - $('#gradientleftcol').offset().top - $("#gradientleftcol").outerHeight(true) - 64;
        gradcolpick1.width = gradientwidth;
        gradcolpick1.height = gradheight;
        gradcolpick2.width = gradientwidth;
        gradcolpick2.height = gradheight;
        gradcolpick2.redraw();
        gradcolpick1.redraw();
        gradcolpick1.show()
    });

    $('#individual').on('shown.bs.collapse', function() {
        drawindividual();
    });

    //Add individual led control functionality
    $( "#saturation" ).change(function() {
        drawindividual();
    });
    $( "#value" ).change(function() {
        drawindividual();
    });


    //Settings
    $('#ws1').text(ws1);
    $('#websocket1').val(ws1);
    $('#websocket1button').click( function(e) {
        e.preventDefault();
        ws1 = $('#websocket1').val();
        window.location = location.protocol + '//' + location.host + location.pathname + "?ws1=" + ws1;
    });
    $('#pingbutton').click( function(e) {
        e.preventDefault();
        mysocket.send('Pping');
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

    $( "#individualcolumns" ).change(function() {
        drawindividual();
    });
    $( "#individualrows" ).change(function() {
        drawindividual();
    });

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
