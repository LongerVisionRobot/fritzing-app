/*******************************************************************

Part of the Fritzing project - https://fritzing.org
Copyright (c) 2015 Fritzing

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************


********************************************************************/

#ifndef CLIPPERHELPERS_H
#define CLIPPERHELPERS_H


#include <clipper.h>
#include <QPaintEngine>
#include <fstream>

inline Clipper2Lib::JoinType qtToClipperJoinType(Qt::PenJoinStyle style) {
	switch (style) {
		case Qt::MiterJoin:
		case Qt::SvgMiterJoin:
			return Clipper2Lib::JoinType::Miter;
		case Qt::BevelJoin:
			return Clipper2Lib::JoinType::Square;
		case Qt::RoundJoin:
		default:
			return Clipper2Lib::JoinType::Round;
	}
}

inline Clipper2Lib::EndType qtToClipperEndType(Qt::PenCapStyle style, bool open, bool fill) {
	if (open)
		switch (style) {
			case Qt::FlatCap:
				return Clipper2Lib::EndType::Butt;
			case Qt::SquareCap:
				return Clipper2Lib::EndType::Square;
			case Qt::RoundCap:
			default:
				return Clipper2Lib::EndType::Round;
		}
	else if (fill)
		return Clipper2Lib::EndType::Polygon;
	else
		return Clipper2Lib::EndType::Joined;
}


inline Clipper2Lib::FillRule qtToClipperFillType(QPaintEngine::PolygonDrawMode mode) {
	switch (mode) {
		case QPaintEngine::OddEvenMode:
			return Clipper2Lib::FillRule::EvenOdd;
		case QPaintEngine::WindingMode:
		case QPaintEngine::ConvexMode:
		case QPaintEngine::PolylineMode:
		default:
			return Clipper2Lib::FillRule::NonZero;
	}
}

inline Clipper2Lib::Path64 pointsToClipper(const QPointF *points, int pointCount, QTransform matrix = QTransform()) {
	Clipper2Lib::Path64 clipperPath;
	for (int i = 0; i < pointCount; i++) {
		const QPointF p2 = matrix.map(points[i]);
		clipperPath << Clipper2Lib::Point64((int64_t)(p2.x()), (int64_t)(p2.y()));
	}
	return clipperPath;
}

inline Clipper2Lib::Path64 polygonToClipper(QPolygonF poly, const QTransform &matrix = QTransform()) {
	return pointsToClipper(poly.data(), poly.size(), matrix);
}

inline Clipper2Lib::Paths64 polygonsToClipper(QList<QPolygonF> polys, const QTransform &matrix = QTransform()) {
	Clipper2Lib::Paths64 paths;
	for (int i = 0; i < polys.size(); i++) {
		paths << polygonToClipper(polys[i], matrix);
	}
	return paths;
}

inline QRectF boundingBox(Clipper2Lib::Paths64 paths) {
	int minX = std::numeric_limits<int>::max();
	int minY = std::numeric_limits<int>::max();
	int maxX = std::numeric_limits<int>::min();
	int maxY = std::numeric_limits<int>::min();
	for (size_t i = 0; i < paths.size(); i++) {
		for (size_t j = 0; j < paths[i].size(); j++) {
			const auto &pt = paths[i][j];
			minX = std::min(minX, (int) pt.x);
			minY = std::min(minY, (int) pt.y);
			maxX = std::max(maxX, (int) pt.x);
			maxY = std::max(maxY, (int) pt.y);
		}
	}
	return QRectF(minX, minY, maxX - minX, maxY - minY);
}

inline QString clipperPathsToSVG(Clipper2Lib::Paths64 &paths, double clipperDPI, bool withSVGElement = true) {
	QRectF bounds = boundingBox(paths);
	QStringList pSvg;

	if (withSVGElement)
		pSvg << TextUtils::makeSVGHeader(1, clipperDPI, bounds.width() / clipperDPI, bounds.height() / clipperDPI);
	pSvg << QString("<path fill='black' stroke='none' stroke-width='0' d='");
	for (size_t i = 0; i < paths.size(); i++) {
		for (size_t j = 0; j < paths[i].size(); j++) {
			const auto &pt = paths[i][j];
			pSvg << QString(j == 0 ? "M" : "L");
			pSvg << QString("%1,%2 ").arg(pt.x - bounds.left()).arg(pt.y - bounds.top());
		}
		pSvg << "Z";
	}
	pSvg << "'/>\n";
	if (withSVGElement)
		pSvg << "</svg>\n";
	return pSvg.join("");
}

inline void clipperPathsToSVGFile(Clipper2Lib::Paths64 &paths, double clipperDPI, QString fileName) {
	std::ofstream file(fileName.toStdString().c_str());
	file << clipperPathsToSVG(paths, clipperDPI).toStdString();
}

inline QString imageToSVGPath(QImage &image, double res) {
	// Clipper2Lib::Paths64 vectorized;
	Clipper2Lib::Clipper64 cp;
	Clipper2Lib::Paths64 lineSpans;

	// bool isIndexed = (image.format() == QImage::Format_Indexed8);

	for (int j = 0; j < image.height(); j++) {
		bool previousPix = false;
		int startIndex = 0;
		for (int i = 0; i < image.width(); i++) {
			bool pix = qGray(image.pixel(i, j)) != 0;
			if (pix != previousPix || i == image.width() - 1) {
				if (pix ^ (i == image.width() - 1)) {
					startIndex = i;
				} else {
					Clipper2Lib::Path64 span;
					span << Clipper2Lib::Point64(startIndex, j)
						 << Clipper2Lib::Point64(i, j)
						 << Clipper2Lib::Point64(i, j + 1)
						 << Clipper2Lib::Point64(startIndex, j + 1);

					lineSpans << span;
				}
			}
			previousPix = pix;
		}
	}

	cp.AddOpenSubject(lineSpans);  // new implementation
	Clipper2Lib::Paths64 result;
	cp.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, result);
	return clipperPathsToSVG(result, res, false);
}

#endif // CLIPPERHELPERS_H
