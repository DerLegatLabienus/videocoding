exports.videos_dir = '';
exports.coded_dir = '';
exports.encoder_dir = '';

exports.without_path = function(path) {
    return path.replace(/^.*[\\\/]/, '');
}
exports.without_extension = function(path) {
    return path.replace(/\.[^/.]+$/, '');
}


