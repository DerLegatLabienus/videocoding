var net         = require('net'),
    fs          = require('fs'),
    repository  = require('./repository'),
    db          = require('./dbschema'),
    encoder     = require('./encoder'),
    decryptor   = require('./decryptor'),
    directories = require('./directories'),
    q           = require('q');

var VideoModel = db.VideoModel;
var without_extension = directories.without_extension;

var coded_path = function(user, video) {
    return __dirname + '/../videos/coded/' + user+ '/' + without_extension(video.name) + '.av1';
}

var video_path = function(title) {
    return __dirname + '/../videos/raw/' + title;
}

var serialize_metadata = function(video) {
    var buf = new Buffer(12);
    buf.writeInt32LE(video.width, 0, 4);
    buf.writeInt32LE(video.height, 4, 4);
    buf.writeInt32LE(video.frames, 8, 4);
    return buf;
}

var decrypt_key = function(data, src) {
    return q.promise(function(resolve, reject) {
        resolve(decryptor.decrypt(data, function() {
            resolve(0);
        }));
    });
}

var encode_and_serve = function(socket, name, user, frames) {
	
    VideoModel.findOne({name : new RegExp('^' + name + '$', 'i')}, function(err, video) {
        if (!video) {
            console.error('no video found matching', name, '- aborting');
            return;
        }
        if (err) {
            console.error(err);
            return;
        }
        // encode the video
        encoder.encode(user, video, frames, function() {
            var encoding = { user: user, date: new Date() };
            video.encodings.push(encoding); // TODO: persist the video update
            var upsertData = video.toObject();
		console.log('saved data',upsertData);
            delete upsertData._id;
            delete upsertData.__v
            VideoModel.update({name: video.name}, upsertData, {upsert: true}, function(err) {
                if (err) {
                    console.log(err);
                }
            });

            // serve the encoded video
            var path = coded_path(user, video);
            socket.write(serialize_metadata(video));

            var f = fs.readFile(path, function(err, content) {
                if (err) throw err;
                socket.write(content);
            });

        });
    });
}

var tcp_server = net.createServer(function(socket) {
    console.log('server connected');

//	console.log(socket);
    socket.on('data', function(data) {
        var src = socket.remoteAddress;
        var user = 'aviv'; // todo: get this
	//var name = data.toString('utf8',5,20).split(" ")[0];
        var name = data.length > 256 ? data.toString('utf8', 256) : 'good.mpg';
        decrypt_key(data, src).then(function() {
	    console.log("encoding");
            encode_and_serve(socket, name, user, 100);
        });
    });
});

exports.listen = function() {
    var PORT = 7890;
    tcp_server.listen(PORT, function() {
        //listening
        console.log('tcp server listening on port ' + PORT + '\n');

        tcp_server.on('connection', function(){
            console.log('connection made...\n')
        })
    });
}
