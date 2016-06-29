var path = require('path'),
    fs = require('fs'),
    express = require('express'),
    exec = require('child_process').exec,
    app = express();

function send_signal(command, req, res){
    res.send('You posted '+command);
    console.log(command);
    fs.writeFile('/tmp/em_command.conf', command, function(err1){
      if (err1) {
        res.send({
            command: command,    
            success: false,
            message: 'Failed to write command to file'
        });
      } else {  
        exec("/usr/bin/pkill -SIGUSR1 test_signal", 
            function(error, stdout, stderr) {
                var success = true;
                if (stderr || error){
                    console.error(command+': stderr: ' + stderr);
                    console.error(command+': error: ' + error.code);
                    success = false;
                }
                res.send({
                    command: command,    
                    success: success
                });
        });
      }
    });
}    

app.get('/', function (req, res) {
  res.send('Hello to simulation with test_signal!');
});

app.get('/reset_trip', function(req, res) {
  send_signal('reset_trip', req, res)
});   

app.get('/reset_string', function(req, res) {
  send_signal('reset_string', req, res)
});   

app.get('/stop_video_recording', function(req, res) {
  send_signal('stop_video_recording', req, res)
});   

app.get('/start_video_recording', function(req, res) {
  send_signal('start_video_recording', req, res)
});   

app.listen(3000, function () {
      console.log('test_signal listening on port 3000!');
});
