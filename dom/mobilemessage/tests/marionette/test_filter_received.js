/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.addPermission("sms", true, document);
SpecialPowers.setBoolPref("dom.sms.enabled", true);

let sms = window.navigator.mozSms;
let numberMsgs = 10;
let smsList = new Array();

function verifyInitialState() {
  log("Verifying initial state.");
  ok(sms, "mozSms");
  // Ensure test is starting clean with no existing sms messages
  deleteAllMsgs(simulateIncomingSms);
}

function deleteAllMsgs(nextFunction) {
  let msgList = new Array();
  let filter = new MozSmsFilter;

  let request = sms.getMessages(filter, false);
  ok(request instanceof MozSmsRequest,
      "request is instanceof " + request.constructor);

  request.onsuccess = function(event) {
    ok(event.target.result, "smsrequest event.target.result");
    cursor = event.target.result;
    // Check if message was found
    if (cursor.message) {
      msgList.push(cursor.message.id);
      // Now get next message in the list
      cursor.continue();
    } else {
      // No (more) messages found
      if (msgList.length) {
        log("Found " + msgList.length + " SMS messages to delete.");
        deleteMsgs(msgList, nextFunction);
      } else {
        log("No SMS messages found.");
        nextFunction();
      }
    }
  };

  request.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    log("sms.getMessages error: " + event.target.error.name);
    ok(false,"Could not get SMS messages");
    cleanUp();
  };
}

function deleteMsgs(msgList, nextFunction) {
  let smsId = msgList.shift();

  log("Deleting SMS (id: " + smsId + ").");
  let request = sms.delete(smsId);
  ok(request instanceof MozSmsRequest,
      "request is instanceof " + request.constructor);

  request.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if (event.target.result) {
      // Message deleted, continue until none are left
      if (msgList.length) {
        deleteMsgs(msgList, nextFunction);
      } else {
        log("Finished deleting SMS messages.");
        nextFunction();
      }
    } else {
      log("SMS delete failed.");
      ok(false,"sms.delete request returned false");
      cleanUp();
    }
  };

  request.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "sms.delete request returned unexpected error: "
        + event.target.error.name );
    cleanUp();
  };
}

function simulateIncomingSms() {
  let text = "Incoming SMS number " + (smsList.length + 1);
  let remoteNumber = "5552229797";

  log("Simulating incoming SMS number " + (smsList.length + 1) + " of "
      + (numberMsgs - 1) + ".");

  // Simulate incoming sms sent from remoteNumber to our emulator
  rcvdEmulatorCallback = false;
  runEmulatorCmd("sms send " + remoteNumber + " " + text, function(result) {
    is(result[0], "OK", "emulator callback");
    rcvdEmulatorCallback = true;
  });
}

// Callback for incoming sms
sms.onreceived = function onreceived(event) {
  log("Received 'onreceived' sms event.");
  let incomingSms = event.message;
  log("Received SMS (id: " + incomingSms.id + ").");

  smsList.push(incomingSms);

  // Wait for emulator to catch up before continuing
  waitFor(nextRep,function() {
    return(rcvdEmulatorCallback);
  });
};

function nextRep() {
  if (smsList.length < (numberMsgs - 1)) {
    simulateIncomingSms();
  } else {
    // Now send one message also so the filter won't find all
    sendSms();
  }
}

function sendSms() {  
  let gotSmsSent = false;
  let gotRequestSuccess = false;
  let remoteNumber = "5557779999";
  let text = "Outgoing SMS brought to you by Firefox OS!";

  log("Sending an SMS.");

  sms.onsent = function(event) {
    log("Received 'onsent' smsmanager event.");
    gotSmsSent = true;
    let sentSms = event.message;
    log("Sent SMS (id: " + sentSms.id + ").");
    is(sentSms.delivery, "sent", "delivery");
    if (gotSmsSent && gotRequestSuccess) {
      // Test the filter
      getMsgs();
    }
  };

  let request = sms.send(remoteNumber, text);
  ok(request instanceof MozSmsRequest,
      "request is instanceof " + request.constructor);

  request.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if(event.target.result) {
      gotRequestSuccess = true;
      if (gotSmsSent && gotRequestSuccess) {
        // Test the filter
        getMsgs(); 
      }
    } else {
      log("smsrequest returned false for sms.send");
      ok(false,"SMS send failed");
      cleanUp();
    }
  };

  request.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "sms.send request returned unexpected error: "
        + event.target.error.name );
    cleanUp();
  };
}

function getMsgs() {
  var filter = new MozSmsFilter();
  let foundSmsList = new Array();

  // Set filter for received messages
  filter.delivery = "received";

  log("Getting the received SMS messages.");
  let request = sms.getMessages(filter, false);
  ok(request instanceof MozSmsRequest,
      "request is instanceof " + request.constructor);

  request.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    cursor = event.target.result;

    if (cursor.message) {
      // Another message found
      log("Got SMS (id: " + cursor.message.id + ").");
      // Store found message
      foundSmsList.push(cursor.message);
      // Now get next message in the list
      cursor.continue();
    } else {
      // No more messages; ensure correct number found
      if (foundSmsList.length == smsList.length) {
        log("SMS getMessages returned " + foundSmsList.length +
            " messages as expected.");
        verifyFoundMsgs(foundSmsList);
      } else {
        log("SMS getMessages returned " + foundSmsList.length +
            " messages, but expected " + smsList.length + ".");
        ok(false, "Incorrect number of messages returned by sms.getMessages");
        deleteAllMsgs(cleanUp);
      }
    }
  };

  request.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    log("sms.getMessages error: " + event.target.error.name);
    ok(false,"Could not get SMS messages");
    cleanUp();
  };
}

function verifyFoundMsgs(foundSmsList) {
  for (var x = 0; x < foundSmsList.length; x++) {
    is(foundSmsList[x].id, smsList[x].id, "id");
    is(foundSmsList[x].delivery, "received", "delivery");
  }
  deleteAllMsgs(cleanUp);
}

function cleanUp() {
  sms.onreceived = null;
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

// Start the test
verifyInitialState();
