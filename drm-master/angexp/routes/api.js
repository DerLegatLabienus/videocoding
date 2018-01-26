var repository = require('../repository'),
    encoder    = require('../encoder'),
    db         = require('../dbschema');

var VideoModel = db.VideoModel;

exports.videos = function (req, res) {
	console.log("reading videos 3");
    VideoModel.find({}, function(err, videos) {
        if (!err) {
            res.json({
                videos: videos
            });
        } else {
            throw err;
        }
    });
};

exports.load = function(req, res) {
	console.log("reading raw videos");
	repository.load_videos();
	res.end('');
}
exports.read = function (req, res) {
	console.log("reading videos 1");
    repository.read_videos();
    res.end('');
}

exports.encode = function (req, res, next) {
    var user   = req.body.user;
    var key    = req.body.key;
    var name   = req.params.name;
    var frames = req.body.frames;
};

