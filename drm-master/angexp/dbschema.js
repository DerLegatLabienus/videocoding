var mongoose = require('mongoose');
exports.mongoose = mongoose;

// Database connect
var uristring = process.env.MONGOLAB_URI ||
                process.env.MONGOHQ_URL  ||
                'mongodb://localhost/drm_server';

var mongoOptions = { db: { safe: true }};

mongoose.connect(uristring, mongoOptions, function (err, res) {
    if (err) {
        console.log ('ERROR connecting to: ' + uristring + '. ' + err);
    } else {
        console.log ('Successfully connected to: ' + uristring);
    }
});

var Schema = mongoose.Schema;
var ObjectId = Schema.ObjectId;

var videoSchema = new Schema({
    name:       { type: String, required: true, unique: true },
    width:      { type: Number, required: true, min: 1 },
    height:     { type: Number, required: true, min: 1 },
    frames:     { type: Number, required: true, min: 1 },
    encodings: [{ user: String, date: Date}],
});

var VideoModel = mongoose.model('Video', videoSchema);
exports.VideoModel = VideoModel;
