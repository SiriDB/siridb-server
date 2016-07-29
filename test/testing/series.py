class Series:
    def __init__(self, name):
        self.name = name
        self.points = []

    def add_points(self, points):
        self.points.append(points)
        return points