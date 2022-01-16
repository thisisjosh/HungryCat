// https://github.com/thisisjosh/HungryCat
// This app is based on https://github.com/evothings/evothings-examples/tree/master/examples/redbearlab-simplechat

document.addEventListener(
	'deviceready',
	function() { evothings.scriptsLoaded(app.initialize) },
	false);

var app = {};

app.RBL_SERVICE_UUID = '713d0000-503e-4c75-ba94-3148f18d941e';
app.RBL_CHAR_TX_UUID = '713d0002-503e-4c75-ba94-3148f18d941e';
app.RBL_CHAR_RX_UUID = '713d0003-503e-4c75-ba94-3148f18d941e';
app.RBL_TX_UUID_DESCRIPTOR = '00002902-0000-1000-8000-00805f9b34fb';

app.initialize = function()
{
	app.connected = false;
};

app.sendMessage = function()
{
	// Get message from input
	var message = document.getElementById('messageField').value;
	app.sendMessageBle(message);
}

app.setAlarm = function(alarmNum)
{
	var message = alarmNum + " " + document.getElementById('time' + alarmNum).value + " " + document.getElementById('onOff' + alarmNum).value;
	app.sendMessageBle(message);
}

app.onAlarmTimeChange = function(alarmNum)
{
	console.log("onAlarmTimeChange for " + alarmNum);
	app.setAlarm(alarmNum);
}

app.onOffChanged = function(alarmNum)
{
	console.log("onOffChanged for " + alarmNum);
	app.setAlarm(alarmNum);
}

// oldDate format: 22020-02-02T10:55:50-0800
app.updateClock = function(oldDateStr)
{
	console.log("oldDateStr: " + oldDateStr);
	var oldDate = new Date(oldDateStr);
	var date = new Date();
	var y2k = Math.round((new Date(2000, 0, 1, 0, 0, 0)).getTime() / 1000);
	var secondsSinceEpoch = Math.round(date.getTime() / 1000);
	var secondsSince2000 = secondsSinceEpoch - y2k;
	var message = "now " +  secondsSince2000;
	console.log("updating the clock: "+ message);
	app.sendMessageBle(message);

	if(oldDate.getTime() > date.getTime()){
		var dif = oldDate.getTime() - date.getTime();
		var difSeconds = Math.abs(dif / 1000);
		var timeMessage = "Hungry Cat clock was fast: " + difSeconds + "s";
		console.log(timeMessage);
		$('#deviceClock').text(timeMessage);
	}
	else if(oldDate.getTime() < date.getTime()) {
		var dif = date.getTime() - oldDate.getTime();
		var difSeconds = Math.abs(dif / 1000);
		var timeMessage = "Hungry Cat clock was slow: " + difSeconds + "s";
		console.log(timeMessage);
		$('#deviceClock').text(timeMessage);
	}
	else {
		var timeMessage = "Hungry Cat clock matched phone time.";
		console.log(timeMessage);
		$('#deviceClock').text(timeMessage);
	}
}

app.adjustLeft = function()
{
	var steps = document.getElementById("steps").value;
	console.log("adjust left " + steps);
	app.sendMessageBle("left " + steps);
}

app.adjustRight = function()
{
	var steps = document.getElementById("steps").value;
	console.log("adjust right " + steps);
	app.sendMessageBle("right " + steps);
}

app.setLeft = function()
{
	var steps = document.getElementById("steps").value;
	console.log("set left " + steps);
	app.sendMessageBle("setleft " + steps);
}

app.setRight = function()
{
	var steps = document.getElementById("steps").value;
	console.log("set right " + steps);
	app.sendMessageBle("setright " + steps);
}

app.sendMessageBle = function(message)
{
	if (app.connected)
	{
		function onMessageSendSucces()
		{
			console.log('Succeded to send message.');
		}

		function onMessageSendFailure(errorCode)
		{
			// Disconnect and show an error message to the user.
			app.disconnect('Disconnected');

			// Write debug information to console
			console.log('Error - No device connected.');
		};

		// Convert message
		var data = new Uint8Array(message.length);

		for (var i = 0, messageLength = message.length; i < messageLength; i++)
		{
			data[i] = message.charCodeAt(i);
		}

		app.device.writeCharacteristic(app.RBL_CHAR_RX_UUID, data, onMessageSendSucces, onMessageSendFailure);
	}
	else
	{
		// Disconnect and show an error message to the user.
		app.disconnect('Disconnected');

		// Write debug information to console
		console.log('Error - No device connected.');
	}
};

app.setLoadingLabel = function(message)
{
	console.log(message);
	$('#loadingStatus').text(message);
};

app.connectTo = function(address)
{
	device = app.devices[address];

	$('#loadingView').css('display', 'table');

	app.setLoadingLabel('Trying to connect to ' + device.name);

	function onConnectSuccess(device)
	{
		function onServiceSuccess(device)
		{
			// Application is now connected
			app.connected = true;
			app.device = device;

			console.log('Connected to ' + device.name);

			device.writeDescriptor(
				app.RBL_CHAR_TX_UUID, // Characteristic for accelerometer data
				app.RBL_TX_UUID_DESCRIPTOR, // Configuration descriptor
				new Uint8Array([1,0]),
				function()
				{
					console.log('Status: writeDescriptor ok.');

					$('#loadingView').hide();
					$('#scanResultView').hide();
					$('#conversationView').show();
				},
				function(errorCode)
				{
					// Disconnect and give user feedback.
					app.disconnect('Failed to set descriptor.');

					// Write debug information to console.
					console.log('Error: writeDescriptor: ' + errorCode + '.');
				}
			);

			function failedToEnableNotification(erroCode)
			{
				console.log('BLE enableNotification error: ' + errorCode);
			};

			device.enableNotification(
				app.RBL_CHAR_TX_UUID,
				app.receivedMessage,
				function(errorcode)
				{
					console.log('BLE enableNotification error: ' + errorCode);
				}
			);

			$('#scanResultView').hide();
			$('#conversationView').show();

			// query the HungryCat for current state of time and alarms
			app.sendMessageBle('list');
		}

		function onServiceFailure(errorCode)
		{
			// Disconnect and show an error message to the user.
			app.disconnect('Device is not from RedBearLab');

			// Write debug information to console.
			console.log('Error reading services: ' + errorCode);
		}

		app.setLoadingLabel('Identifying services...');

		// Connect to the appropriate BLE service
		device.readServices(
			[app.RBL_SERVICE_UUID],
			onServiceSuccess,
			onServiceFailure
		);
	}

	function onConnectFailure(errorCode)
	{
		app.disconnect('Disconnected from device');

		// Show an error message to the user
		console.log('Error ' + errorCode);
	}

	// Stop scanning
	evothings.easyble.stopScan();

	// Connect to our device
	console.log('Identifying service for communication');
	device.connect(onConnectSuccess, onConnectFailure);
};

app.startScan = function()
{
	app.disconnect();

	console.log('Scanning started...');

	app.devices = {};

	var htmlString =
		'<img src="img/loader_small.gif" style="display:inline; vertical-align:middle">' +
		'<p style="display:inline">   Scanning...</p>';

	$('#scanResultView').append($(htmlString));

	$('#scanResultView').show();

	function onScanSuccess(device)
	{
		if (device.name != null)
		{
			app.devices[device.address] = device;

			console.log('Found: ' + device.name + ', ' + device.address + ', ' + device.rssi);

			var divClass = (device.name == 'HungryCat') ? 'deviceContainerHungryCat' : 'deviceContainer';

			var htmlString =
				'<div class="' + divClass + '" onclick="app.connectTo(\'' +
					device.address + '\')">' +
				'<p class="deviceName">' + device.name + '</p>' +
				'<p class="deviceAddress">' + device.address + '</p>' +
				'</div>';

			$('#scanResultView').append($(htmlString));
		}
	};

	function onScanFailure(errorCode)
	{
		// Show an error message to the user
		app.disconnect('Failed to scan for devices.');

		// Write debug information to console.
		console.log('Error ' + errorCode);
	};

	evothings.easyble.reportDeviceOnce(true);
	evothings.easyble.startScan(onScanSuccess, onScanFailure);

	$('#startView').hide();
};

app.receivedMessage = function(data)
{
	var alarmRegex = /alarm (\d) (\d+):(\d+) (\d)/; // alarm 2 18:30 1
	var nowRegex = /(\d\d\d\d-\d\d-\d\dT\d\d:\d\d:\d\d)/; //2020-02-02T10:55:50

	if (app.connected)
	{
		// Convert data to String
		var message = String.fromCharCode.apply(null, new Uint8Array(data));
		var alarmMatches = message.match(alarmRegex);
		var dateMatch = message.match(nowRegex);

		if(alarmMatches != null){
			var alarmNum = alarmMatches[1];
			var hour = ('00'+alarmMatches[2]).slice(-2);
			var minute = ('00'+alarmMatches[3]).slice(-2);
			var isEnabled = alarmMatches[4];
			var time = hour + ":" + minute;

			$('#time' + alarmNum).val(time);
			$('#onOff' + alarmNum).val(isEnabled);
			console.log("found alarm "  + alarmNum + " is " + time + " isEnabled:" + isEnabled);
		}
		else if(dateMatch != null){
			var dateStr = dateMatch[1] + "-0800";
			console.log('found rtc is ' + dateStr);
			app.updateClock(dateStr); // Assuming PST for now, but this should be changed to UTC
		}

		console.log('Message received: ' + message);
	}
	else
	{
		// Disconnect and show an error message to the user.
		app.disconnect('Disconnected');

		// Write debug information to console
		console.log('Error - No device connected.');
	}
};

app.disconnect = function(errorMessage)
{
	if (errorMessage)
	{
		navigator.notification.alert(errorMessage, function() {});
	}

	app.connected = false;
	app.device = null;

	// Stop any ongoing scan and close devices.
	evothings.easyble.stopScan();
	evothings.easyble.closeConnectedDevices();

	console.log('Disconnected');

	$('#loadingView').hide();
	$('#scanResultView').hide();
	$('#scanResultView').empty();
	$('#conversationView').hide();

	$('#startView').show();
};
