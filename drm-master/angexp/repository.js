var _            = require('underscore'),
    mediainfo    = require('mediainfo'),
    fs           = require('fs'),
    dbschema     = require('./dbschema'),
    directories  = require('./directories'),
    childProcess = require('child_process'),
    sprintf       = require('sprintf-js').sprintf,
    path         = require('path');

var VideoModel = dbschema.VideoModel;
var without_extension = directories.without_extension;
var without_path       = directories.without_path;
var raw_dir = '/home/aviva/thesis/drm-master/videos/raw';
var path_not_processed = '/home/aviva/thesis/drm-master/videos/preprocess/';
var RAW_EXT = '.yuv';

exports.read_videos = function() {
    var files = fs.readdirSync(path_not_processed);
    for (var i = 0; i < files.length; i++) {
	    var abs_file = path_not_processed+ '/' + files[i];
            mediainfo(abs_file, function(err, res) {
		    if (err) {
			return console.log(err);
		    }
		    _.each(res, function(file) { 
			var video = extract_media_data(file);
			var abs = [path_not_processed,'/',video.name ].join('');
			var avconv = 'avconv -i ' + abs + ' -s 200x200 ' + __dirname + '/public/images/videos/' + without_extension(without_path(file.complete_name)) + '.jpg';
			childProcess.exec(avconv, function(error, stdout, stderr) {
			    if (error != null) {
				console.log('avconv error', error);
			    };
			});
		    });
        });
    };
}

var insert_to_db = function (filename,width,height,frames) {
	var video = new VideoModel({
	    name: without_path(filename),
	    width : width,
	    height : height,
	    frames : frames
	});

	var upsertData = video.toObject();
	delete upsertData._id;
	VideoModel.update({name: video.name}, upsertData, {upsert: true}, function(err) {
	    if (err) {
		console.log(err);
	    }
	});
};

var extract_media_data = function (file) {
	var video = _.find(file.tracks,function(track) {return track.type == 'Video'});
	var filename = without_path(file.complete_name);
	var width = 200, height = 200, frames = 100;
	if (video != undefined) { 
		width = video.width.split(' ')[0];
		height = video.height.split(' ')[0];
		frames = video.frame_rate.split(' ')[0];
	}
	else {
		console.log(stderr,"file had no media info");
		console.log(stderr,"using default values for frame");
	}
	return {name:filename,
		width:width,
		height:height,
		frames:frames };

};
exports.load_videos = function() {

    var up_files = fs.readdirSync(path_not_processed);
    var r_files = fs.readdirSync(raw_dir);

    var find_unprocessed_files = function(up_files,raw_files) {
	    var convertion_files = []; 
	    for (var i = 0; i < up_files.length; i++) {
		    var not_found = true;
		    var up_v_no_ext = without_extension(up_files[i]);
		    var abs_up_file = [path_not_processed,'/',up_files[i]].join('');
		    for (var j = 0; not_found && j < raw_files.length; j++) {
		    	var r_file_no_ext = without_extension(raw_files[j]);
			if (r_file_no_ext==up_v_no_ext) {
			   not_found = false;
			}
		    }
		    if (not_found) {
			console.log('added ' + up_files[i]);
		    	convertion_files.push(abs_up_file);
		    }
		    mediainfo(abs_up_file,function(err,res){
			    _.each(res,function (file) {
				    var vi = extract_media_data(file);
				    insert_to_db(without_extension(vi.name)+RAW_EXT,vi.width,vi.height,vi.frames);
			    })
		    })
	    }
	    return convertion_files;
    };
    var videos_for_conversion = find_unprocessed_files(up_files,r_files);
    for (var i = 0; i < videos_for_conversion.length; i++ ){
	    var v = videos_for_conversion[i];
            mediainfo(v, function(err, res) {
		    _.each(res,function (file) {
			    var video = extract_media_data(file);
			    var params = {
				    '-i':file.complete_name,
				    '-s':sprintf('%dx%d',video.width,video.height),
				    '-vcodec' : 'rawvideo',
				    '-pix_fmt': 'yuv420p'
			    }

			    var stringified_params = function(params) {
				    return (_.map(_.pairs(params),
					    function(p) {
						    return [p[0],' ',p[1]].join('');
					    }).join(' '));
			    };

			    var filename = without_path(without_extension(video.name));
			    var output_raw_file = [raw_dir,'/',filename+RAW_EXT].join('');
			    var cmd = ['ffmpeg',stringified_params(params),output_raw_file].join(' ');

			    //post process
			    var rawer = childProcess.exec(cmd, function(error, stdout, stderr) {
				if (error) {
				    console.log(error.stack);
				    console.log('error code: ' + error.code);
				    console.log('signal received: ' + error.signal);
				}
				if (stdout) {console.log('child process STDOUT: ' + stdout);}
				if (stderr) {console.log('child process STDERR: ' + stderr);}
			    });
			    rawer.on('exit', function(code) {
				if (code == 0) {
				    console.log('finished decoding successfully');
				} else {
				    console.log('decoder return code:', code);
				}
			    });
		    });
	    });
    }
} 
