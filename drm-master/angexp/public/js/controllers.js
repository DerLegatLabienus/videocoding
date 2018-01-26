'use strict';

function IndexCtrl($scope, $http) {
}

function VideosCtrl($scope, $http) {
    $scope.toJpg = function(video) {
        return video.substr(0, video.lastIndexOf('.')) + '.jpg';
    };
    $http.get('/api/videos').
      success(function(data) {
        $scope.videos = data.videos;
      });
};

function NavBarCtrl($scope, $location) {
    $scope.isActive = function(path) {
        return ($location.path().substr(0, path.length) == path) && (path != "/" || $location.path() == "/");
    }
}

