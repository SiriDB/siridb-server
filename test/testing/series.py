class Series:
    def __init__(self, name):
        self.name = name
        self.points = []
        self._points = None

    def add_points(self, points):
        self.commit_points()
        self._points = points
        return points

    def commit_points(self):
        if self._points:
            for point in self._points:
                self.points.append(point)
            self._points = None
