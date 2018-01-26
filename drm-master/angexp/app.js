var express    = require('express'),
    routes     = require('./routes'),
    api        = require('./routes/api'),
    tcp_server = require('./tcp_server');

var app = module.exports = express.createServer();

app.configure(function(){
  app.set('views', __dirname + '/views');
  app.set('view engine', 'jade');
  app.set('view options', {
    layout: false
  });
  app.use(express.bodyParser());
  app.use(express.methodOverride());
  app.use(express.static(__dirname + '/public'));
  app.use(app.router);
});

app.configure('development', function(){
  app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
});

app.configure('production', function(){
  app.use(express.errorHandler());
});

// Routes

app.get('/', routes.index);
app.get('/partials/:name', routes.partials);

// JSON API

app.get('/api/videos', api.videos);
app.get('/api/read',   api.read);
app.get('/api/load',   api.load);

// redirect all others to the index (HTML5 history)
app.get('*', routes.index);

// Start server

app.listen(3000, function(){
  console.log("web server listening on port %d in %s mode", app.address().port, app.settings.env);
	console.log(app.settings);
  tcp_server.listen();
});

