var _             = require('underscore'),
    sprintf       = require('sprintf-js').sprintf,
    child_process = require('child_process'),

    db            = require('./dbschema'),
    directories   = require('./directories');

var without_extension = directories.without_extension;
var CodedModel = db.CodedModel;

var build_cmd = function(user, video, frames) {

    //var flags = [ 'no-deblock', 'no-scenecut', 'no-mbtree', 'constrained-intra', 'no-interlaced' ]; //var default_params = {'profile'   : 'baseline', //   'input-res'   : sprintf('%dx%d', video.width, video.height), //   'slices'      : 1, //   'frames'      : frames, //   'ref'         : 1, //        'rc4-keyfile' : '../angexp/key_dec.bin'}; //     'rc4-keyfile' : '../angexp/key.bin'}; 
    var exec = '../aomenc';
    var videos_dir = '../videos';
    var coded_path = sprintf("%s/coded/%s/%s.av1", videos_dir, user, without_extension(video.name));
    var video_path = sprintf("%s/raw/%s", videos_dir, video.name);

    var params = {
	    'w' : video.width,
	    'h' : video.height,
	    'o' : coded_path,
    }
    var params2 = {
	    'codec' : 'av1',
	    'target-bitrate' : '4000' 
    }
	
    var stringify_params = function(params) {
        var foo = function(p) { return ['-', p[0], ' ', p[1]].join(''); };
        return (_.map(_.pairs(params), foo)).join(' ');
    }

    var stringify_params2 = function(params) {
		return (_.map(_.pairs(params),
			function(p) {
				return ['--',p[0],'=',p[1]].join('');
			}).join(' '));
    }

    var remove_double_spaces = function(str) {
        return str.replace(/ +/g, " ");
    }


    var cmd = [exec,  stringify_params(params), stringify_params2(params2), video_path ].join(' ');
    return remove_double_spaces(cmd);
};

exports.encode = function(user, video, frames, callback) {
    if (!frames) {
        frames = 100;
    }
	console.log(user,video,frames);
    var cmd = build_cmd(user, video, frames);
    console.log('cmd', cmd);

    var enc = child_process.exec(cmd, function(error, stdout, stderr) {
        if (error) {
            console.log(error.stack);
            console.log('error code: ' + error.code);
            console.log('signal received: ' + error.signal);
        }
        if (stdout) {console.log('child process STDOUT: ' + stdout);}
        if (stderr) {console.log('child process STDERR: ' + stderr);}
    });

    enc.on('exit', function(code) {
        if (code == 0) {
            console.log('finished encoding successfully');
            callback();
        } else {
            console.log('encoder return code:', code);
        }
    });
}
