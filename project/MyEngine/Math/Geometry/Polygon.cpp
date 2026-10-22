#include "Polygon.h"

#include <cmath>
#include <cstddef>
#include <numeric>
#include <algorithm>

namespace Polygon2D {
namespace {
constexpr float kMinEdgeLengthSq = 1e-8f; // これより短い辺は「長さ 0」（同じ所に点が2つある）
constexpr float kMinArea = 1e-6f;         // これより小さい面積は「面積 0」（点が一直線に並んでいる）

// 3 点の回り方。＋なら a → b → c が左回り（反時計回り）、−なら右回り、0 なら一直線
float Orient(const Vector2& a, const Vector2& b, const Vector2& c) { return Cros(b - a, c - a); }

// a・b・c が一直線のとき、c が線分 ab の上（両端も入れる）にあるか
bool IsOnSegment(const Vector2& a, const Vector2& b, const Vector2& c) {
	return (std::min)(a.x, b.x) <= c.x && c.x <= (std::max)(a.x, b.x) && (std::min)(a.y, b.y) <= c.y && c.y <= (std::max)(a.y, b.y);
}

// 線分 p1p2 と q1q2 が交わるか（端が触れているだけでも交わる）
// 線分 q の両端が、線分 p をはさんで反対側にあり（回り方の符号が違う）、逆も同じなら交わっている
bool SegmentsIntersect(const Vector2& p1, const Vector2& p2, const Vector2& q1, const Vector2& q2) {
	const float d1 = Orient(q1, q2, p1);
	const float d2 = Orient(q1, q2, p2);
	const float d3 = Orient(p1, p2, q1);
	const float d4 = Orient(p1, p2, q2);
	if (((d1 > 0.0f && d2 < 0.0f) || (d1 < 0.0f && d2 > 0.0f)) && ((d3 > 0.0f && d4 < 0.0f) || (d3 < 0.0f && d4 > 0.0f))) {
		return true;
	}
	// 一直線に並んだ点が、相手の線分の上にある（端が触れている・重なっている）
	return (d1 == 0.0f && IsOnSegment(q1, q2, p1)) || (d2 == 0.0f && IsOnSegment(q1, q2, p2)) || (d3 == 0.0f && IsOnSegment(p1, p2, q1)) || (d4 == 0.0f && IsOnSegment(p1, p2, q2));
}

// 線分 ab の上で point に一番近い点（Collision::ClosestPointOnSegment の 2D 版）
Vector2 ClosestPointOnSegment(const Vector2& point, const Vector2& a, const Vector2& b) {
	const Vector2 ab = b - a;
	const float lengthSq = LengthSq(ab);
	if (lengthSq < kMinEdgeLengthSq) {
		return a; // 長さ0の辺（点）
	}
	const float t = std::clamp(Dot(point - a, ab) / lengthSq, 0.0f, 1.0f);
	return a + ab * t;
}

// point が三角形 abc（反時計回り）の中か。辺の上も中に入れる（耳の中に別の点が「触れている」だけでも、切り取らない）
bool IsPointInTriangle(const Vector2& point, const Vector2& a, const Vector2& b, const Vector2& c) { return Orient(a, b, point) >= 0.0f && Orient(b, c, point) >= 0.0f && Orient(c, a, point) >= 0.0f; }
} // namespace

//=============================================================================
// 面積・中か・一番近い辺
//=============================================================================
float SignedArea(std::span<const Vector2> points) {
	// 靴ひもの公式：辺ごとに「原点・始点・終点の三角形」の符号付きの面積を足す（外へはみ出した分は、逆向きの三角形で打ち消される）
	float twiceArea = 0.0f;
	for (size_t i = 0; i < points.size(); ++i) {
		twiceArea += Cros(points[i], points[(i + 1) % points.size()]);
	}
	return twiceArea * 0.5f;
}

bool IsPointInside(const Vector2& point, std::span<const Vector2> points) {
	// point から右へ線を伸ばし、辺と交わる回数を数える。奇数なら中（へこんだ形でも同じ）
	bool isInside = false;
	for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
		const Vector2& a = points[i];
		const Vector2& b = points[j];
		// 辺 ab が point の高さをまたいでいる（片方だけが上）なら、その高さで辺の X を出し、point より右か見る
		if ((a.y > point.y) != (b.y > point.y)) {
			const float crossX = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
			if (point.x < crossX) {
				isInside = !isInside;
			}
		}
	}
	return isInside;
}

Vector2 ClosestPointOnEdges(const Vector2& point, std::span<const Vector2> points, size_t* edgeIndex) {
	Vector2 closest = points.empty() ? point : points[0];
	float closestDistanceSq = -1.0f; // まだ無い
	for (size_t i = 0; i < points.size(); ++i) {
		const Vector2 onEdge = ClosestPointOnSegment(point, points[i], points[(i + 1) % points.size()]);
		const float distanceSq = LengthSq(point - onEdge);
		if (closestDistanceSq < 0.0f || distanceSq < closestDistanceSq) {
			closest = onEdge;
			closestDistanceSq = distanceSq;
			if (edgeIndex != nullptr) {
				*edgeIndex = i;
			}
		}
	}
	return closest;
}

Vector2 EdgeOutwardNormal(std::span<const Vector2> points, size_t edgeIndex) {
	// 辺の向き (dx, dy) の右側 (dy, -dx)。反時計回りの多角形では、右側が外
	const Vector2 edge = points[(edgeIndex + 1) % points.size()] - points[edgeIndex];
	return Normalize(Vector2{edge.y, -edge.x});
}

//=============================================================================
// 形がおかしくないか
//=============================================================================
bool IsSimple(std::span<const Vector2> points) {
	const size_t count = points.size();
	if (count < 3 || std::abs(SignedArea(points)) < kMinArea) {
		return false;
	}
	// --- 長さ 0 の辺（同じ所に点が2つ）---
	for (size_t i = 0; i < count; ++i) {
		if (LengthSq(points[(i + 1) % count] - points[i]) < kMinEdgeLengthSq) {
			return false;
		}
	}
	// --- 隣り合っていない辺どうしが交わっていないか（隣の辺とは、点を共有しているので必ず触れている）---
	for (size_t i = 0; i < count; ++i) {
		for (size_t j = i + 2; j < count; ++j) {
			if (i == 0 && j == count - 1) {
				continue; // 最後の辺と最初の辺は隣り合っている（点 0 を共有）
			}
			if (SegmentsIntersect(points[i], points[(i + 1) % count], points[j], points[(j + 1) % count])) {
				return false;
			}
		}
	}
	return true;
}

//=============================================================================
// 三角形に分ける（耳切り法）
//=============================================================================
bool Triangulate(std::span<const Vector2> points, std::vector<uint32_t>& indices) {
	if (points.size() < 3) {
		return false;
	}
	// --- まだ切り取っていない点の番号（反時計回りに並べる）---
	std::vector<uint32_t> remaining(points.size());
	std::iota(remaining.begin(), remaining.end(), 0u); // 0, 1, 2, … と順に入れる
	if (SignedArea(points) < 0.0f) {
		std::ranges::reverse(remaining); // 時計回りなら逆に並べる
	}

	// --- 耳を1つずつ切り取る（点が n 個なら、三角形は n-2 個）---
	// 耳：となりの2点と結んだ三角形が、とがっていて（左回り）、中にほかの点が入っていない角。切り取っても、残りは多角形のまま
	while (remaining.size() > 3) {
		const size_t count = remaining.size();
		bool isCut = false;
		for (size_t i = 0; i < count && !isCut; ++i) {
			const uint32_t prev = remaining[(i + count - 1) % count];
			const uint32_t current = remaining[i];
			const uint32_t next = remaining[(i + 1) % count];
			const Vector2& a = points[prev];
			const Vector2& b = points[current];
			const Vector2& c = points[next];
			// へこんだ角（右回り）・一直線の角は、耳ではない
			if (Orient(a, b, c) <= 0.0f) {
				continue;
			}
			// 三角形の中に、ほかの点が入っていたら耳ではない（切り取ると、その点が外に出てしまう）
			const bool hasInside = std::ranges::any_of(remaining, [&](uint32_t other) { return other != prev && other != current && other != next && IsPointInTriangle(points[other], a, b, c); });
			if (hasInside) {
				continue;
			}
			indices.insert(indices.end(), {prev, current, next});
			remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
			isCut = true;
		}
		if (isCut) {
			continue;
		}
		// --- 耳が無い：一直線の角（三角形にしても面積 0）があれば、三角形を作らずに外す。それも無ければ形がおかしい ---
		size_t straight = count; // 一直線の角が何番目か（count なら無い）
		for (size_t i = 0; i < count; ++i) {
			if (Orient(points[remaining[(i + count - 1) % count]], points[remaining[i]], points[remaining[(i + 1) % count]]) == 0.0f) {
				straight = i;
				break;
			}
		}
		if (straight == count) {
			return false;
		}
		remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(straight));
	}
	indices.insert(indices.end(), remaining.begin(), remaining.end());
	return true;
}
} // namespace Polygon2D