var _             = require('underscore'),
    sprintf       = require('sprintf-js').sprintf,
    child_process = require('child_process'),
    fs            = require('fs')
;

exports.decrypt = function(data, callback) {
    return fs.writeFile('key.bin', data, function(err) {
        if(err) {
            console.log(err);
        } else {
	    console.log("decypt","using dec now");
	    var cmd = './dec'
            var enc = child_process.exec(cmd, function(error, stdout, stderr) {
                if (error) {
                    console.log(error.stack);
                    console.log('error code: ' + error.code);
                    console.log('signal received: ' + error.signal);
                }
            });

            enc.on('exit', function(code) {
                if (code == 0) {
                    callback();
                } else {
                    console.log('code', code);
                }
            });
        }

    });

}
