// Scene.cpp: see Scene.h. Each function names the TSoundView function it
// follows.
#include "Scene.h"
#include "Palette.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pmx {

namespace {

const int kAnchor = 0x400;			// owner of a track's anchor vertex
const uint32_t kTagItop = 0x69746f70;	// 'itop'
const uint32_t kTagTtop = 0x74746f70;	// 'ttop', the level bar's top
const int kFrameSlack = 2 * Scene::kWidth + 8;	// lines and spans overrun a row

// 3dmiX converts with C casts (gcc2: fistp with the FPU set to truncate),
// although decompilers show them as ROUND(): truncate toward zero.
int Trunc(double v) { return int(v); }

// b_sqrt: 3dmiX's fast square root, a guess from the float's bits and one
// refinement step. The projection divides by it, so its error shows.
long double FastSqrt(float x)
{
	uint32_t bits;
	memcpy(&bits, &x, 4);
	bits = (bits >> 1) + 0x1fc00000;
	float guess;
	memcpy(&guess, &bits, 4);
	long double y = guess;
	long double a = y * y + x;
	long double b = y * y - x;
	return ((a * a + a * a) - b * b) / (y * 4.0L * a);
}

uint32_t sRandom = 1;
int Random()
{
	sRandom = sRandom * 1103515245 + 12345;
	return int((sRandom >> 16) & 0x7fff);
}

}	// namespace

Scene::Scene()
	:
	fFrame(kWidth * kHeight + kFrameSlack, 0),
	fRoll(0),
	fPitch(0.32f),
	fYaw(0.1f),
	fFocal(300),
	fDistance(-430),
	fCenterY(256),
	fSpinPitch(0),
	fSpinYaw(0),
	fSpinning(false),
	fDemo(false),
	fParticles(false)
{
	BuildVectors();

	// constructor: 2048 particles resting on the floor, red, green, blue or
	// white; the first move recycles them all
	static const uint8_t kColors[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
	fParticleList.resize(0x800);
	for (Particle& p : fParticleList) {
		p.x = float(Random() % 300 - 150);
		p.y = 0;
		p.z = float(Random() % 300 - 150);
		const uint8_t* c = kColors[Random() & 3];
		p.color = IndexForColor(c[0], c[1], c[2]);
		p.sx = p.sy = 0;
	}
	fParticleVelocity.assign(fParticleList.size() * 3, 0.0f);
}

// TSoundView's constructor: the floor grid and the listener's arrows with
// the letters L and R, drawn as vectors.
void
Scene::BuildVectors()
{
	auto add = [this](float x1, float y1, float z1, float x2, float y2, float z2, int color) {
		Vector v = {x1, y1, z1, x2, y2, z2, color, 0, 0, 0, 0};
		fVectors.push_back(v);
	};
	for (float f = 0; f <= 512.1f; f += 21.333334f) {
		add(f - 256, 0, -256, f - 256, 0, 256, -1);
		add(-256, 0, f - 256, 256, 0, f - 256, -1);
	}
	// x arrow in front of the listener
	add(-140, 0, 290, 140, 0, 290, -1);
	add(0, 0, 282, 0, 0, 288, -1);
	// L
	add(-160, -12, 290, -160, 0, 290, 200);
	add(-160, 0, 290, -151, 0, 290, 200);
	// R
	add(151, -12, 290, 151, 0, 290, 200);
	add(151, -12, 290, 160, -12, 290, 200);
	add(160, -12, 290, 160, -6, 290, 200);
	add(160, -6, 290, 151, -6, 290, 200);
	add(151, -6, 290, 160, 0, 290, 200);
	// arrowheads
	add(-140, 0, 290, -130, 0, 295, -1);
	add(-140, 0, 290, -130, 0, 285, -1);
	add(130, 0, 285, 140, 0, 290, -1);
	add(130, 0, 295, 140, 0, 290, -1);
	// z arrow on the right
	add(290, 0, -140, 290, 0, 140, -1);
	add(290, 0, -140, 295, 0, -130, -1);
	add(290, 0, -140, 285, 0, -130, -1);
	add(285, 0, 130, 290, 0, 140, -1);
	add(295, 0, 130, 290, 0, 140, -1);
}

int
Scene::FrameOffset(float windowWidth)
{
	int offset = Trunc((Trunc(windowWidth) - kWidth) * 0.5);
	return offset > 0 ? 0 : offset;
}

// #pragma mark - Tracks

// do_add_channel(Channel*, float, float): a 16x16 box from y = -3 to -75,
// the anchor (-90) and label point (-101.25) above it, and the level bar.
void
Scene::AddTrack(const std::string& name, float x, float y)
{
	const int n = int(fTracks.size());
	Track track;
	track.name = name;
	fTracks.push_back(track);

	auto vertex = [this](float vx, float vy, float vz, int owner, uint32_t tag) {
		Vertex v = {vx, vy, vz, 0, 0, 0, owner, tag};
		fVertices.push_back(v);
		return int(fVertices.size()) - 1;
	};
	int b0 = vertex(-8, -3, -8, n, 0x626f6d6d);		// 'bomm'
	int b1 = vertex(8, -3, -8, n, 0x626f706d);		// 'bopm'
	int b2 = vertex(8, -3, 8, n, 0x626f7070);		// 'bopp'
	int b3 = vertex(-8, -3, 8, n, 0x626f6d70);		// 'bomp'
	int t0 = vertex(-8, -75, -8, n, kTagItop);
	int t1 = vertex(8, -75, -8, n, kTagItop);
	int t2 = vertex(8, -75, 8, n, kTagItop);
	int t3 = vertex(-8, -75, 8, n, kTagItop);
	vertex(0, -90, 0, n + kAnchor, kTagItop);
	vertex(0, -101.25f, 0, n, kTagItop);

	const int mapper = (n % 3) * 4 + 2;
	auto poly = [this, n](int a, int b, int c, int d, int m) {
		Poly p = {{a, b, c, d}, m, m, n};
		fPolys.push_back(p);
	};
	poly(t0, t1, t2, t3, mapper);
	poly(b0, b1, t1, t0, mapper);
	poly(b2, b3, t3, t2, mapper);
	poly(b1, b2, t2, t1, mapper);
	poly(b0, b3, t3, t0, mapper);
	poly(b0, b1, b2, b3, mapper);

	int l0 = vertex(-5, -4, 0, n, 0);
	int l1 = vertex(5, -4, 0, n, 0);
	int l2 = vertex(5, -30, 0, n, kTagTtop);
	int l3 = vertex(-5, -30, 0, n, kTagTtop);
	poly(l0, l1, l2, l3, kHueGreen * 4);

	MoveTrack(n, x, 0, y);
}

// delete_channel: drop the track's geometry and renumber the ones after it
void
Scene::RemoveTrack(int index)
{
	if (index < 0 || index >= CountTracks())
		return;
	std::vector<int> remap(fVertices.size(), -1);
	std::vector<Vertex> vertices;
	for (size_t i = 0; i < fVertices.size(); i++) {
		Vertex v = fVertices[i];
		int track = v.owner >= kAnchor ? v.owner - kAnchor : v.owner;
		if (track == index)
			continue;
		if (track > index)
			v.owner--;
		remap[i] = int(vertices.size());
		vertices.push_back(v);
	}
	std::vector<Poly> polys;
	for (Poly p : fPolys) {
		if (p.owner == index)
			continue;
		if (p.owner > index) {
			// colours follow the track number, as do_add_channel picked them
			p.owner--;
		}
		for (int& v : p.v)
			v = remap[v];
		polys.push_back(p);
	}
	fVertices.swap(vertices);
	fPolys.swap(polys);
	fTracks.erase(fTracks.begin() + index);
}

void
Scene::RemoveAllTracks()
{
	fVertices.clear();
	fPolys.clear();
	fTracks.clear();
}

void
Scene::SetName(int index, const std::string& name)
{
	if (index >= 0 && index < CountTracks())
		fTracks[index].name = name;
}

void
Scene::GetPosition(int index, float& x, float& y) const
{
	x = y = 0;
	for (const Vertex& v : fVertices) {
		if (v.owner == index + kAnchor) {
			x = v.x;
			y = v.z;
			return;
		}
	}
}

void
Scene::SetPosition(int index, float x, float y)
{
	float oldX, oldY;
	GetPosition(index, oldX, oldY);
	MoveTrack(index, x - oldX, 0, y - oldY);
}

// MovePoly
void
Scene::MoveTrack(int index, float dx, float dy, float dz)
{
	for (Vertex& v : fVertices) {
		if (v.owner == index || v.owner == index + kAnchor) {
			v.x += dx;
			v.y += dy;
			v.z += dz;
		}
	}
}

// SetChannelLevel(index, -level / 11)
void
Scene::SetLevel(int index, float level)
{
	if (index < 0 || index >= CountTracks())
		return;
	Track& track = fTracks[index];
	track.level = level;
	float top = track.silent ? -3.0f : float(Trunc(-level / 11.0f * 40.0f - 3.0f));
	for (Vertex& v : fVertices) {
		if (v.owner == index && v.tag == kTagTtop)
			v.y = top;
	}
}

void
Scene::SetSilent(int index, bool silent)
{
	if (index < 0 || index >= CountTracks())
		return;
	fTracks[index].silent = silent;
	SetLevel(index, fTracks[index].level);
}

// #pragma mark - Selection

bool
Scene::IsSelected(int index) const
{
	return index >= 0 && index < CountTracks() && fTracks[index].selected;
}

// SetChannelColor: a selected track's faces are one level brighter
void
Scene::UpdateMappers(int index)
{
	const int add = fTracks[index].selected ? 1 : 0;
	for (Poly& p : fPolys) {
		if (p.owner == index)
			p.mapper = p.base + add;
	}
}

void
Scene::Select(int index, bool selected)
{
	if (index < 0 || index >= CountTracks() || fTracks[index].selected == selected)
		return;
	fTracks[index].selected = selected;
	UpdateMappers(index);
}

void
Scene::SelectAll()
{
	for (int i = 0; i < CountTracks(); i++)
		Select(i, true);
}

void
Scene::DeselectAll()
{
	for (int i = 0; i < CountTracks(); i++)
		Select(i, false);
}

bool
Scene::HasSelection() const
{
	for (const Track& track : fTracks) {
		if (track.selected)
			return true;
	}
	return false;
}

// #pragma mark - Camera

// MouseDown on the floor: yaw follows x, pitch y
void
Scene::Rotate(float dx, float dy)
{
	fYaw -= dx / 500.0f;
	fPitch += dy / 1000.0f;
	fCenterY = 256;
	fSpinning = false;
}

void
Scene::SetSpin(float pitch, float yaw)
{
	fSpinPitch = pitch;
	fSpinYaw = yaw;
	fSpinning = true;
}

void
Scene::ZoomIn()
{
	fFocal *= 1.05f;
	if (fFocal > 900)
		fFocal = 900;
}

void
Scene::ZoomOut()
{
	fFocal /= 1.05f;
	if (fFocal <= 30)
		fFocal = 30;
}

void
Scene::MoveCameraBack()
{
	fDistance += 5;
}

// KeyDown '.': the original clamps the wrong way, so the camera can't come
// nearer than -300 and any step from beyond it jumps there.
void
Scene::MoveCameraNearer()
{
	fDistance -= 5;
	if (fDistance > -300)
		fDistance = -300;
}

// Animate: the demo's slow rotation, and the spin after a flick that slows
// down by 2% a frame
void
Scene::Step()
{
	if (fDemo) {
		fPitch += 0.01f;
		fYaw += 0.0064f;
	}
	if (fSpinning) {
		fPitch -= fSpinPitch;
		fYaw -= fSpinYaw;
		fSpinPitch *= 0.98f;
		fSpinYaw *= 0.98f;
		if (std::fabs(fSpinPitch) <= 0.004f && std::fabs(fSpinYaw) <= 0.004f)
			fSpinning = false;
	}
}

// change_view_point: angles (roll, -pitch, yaw), camera at fDistance
void
Scene::Project(float x, float y, float z, float& sx, float& sy, float& depth) const
{
	const float a = fRoll, b = -fPitch, c = fYaw;
	const double sa = std::sin(double(a)), ca = std::cos(double(a));
	const double sb = std::sin(double(b)), cb = std::cos(double(b));
	const double sc = std::sin(double(c)), cc = std::cos(double(c));
	double x1 = x * float(ca) - y * float(sa);
	double y1 = float(sa) * x + y * float(ca);
	double z1 = x1 * float(sc) + z * float(cc);
	double d = y1 * float(sb) + float(cb) * z1;
	long double w = d + fDistance;
	double s = double(fFocal / FastSqrt(float(w * w)));
	sx = float((x1 * float(cc) - z * float(sc)) * s);
	sy = float((y1 * float(cb) - z1 * float(sb)) * s);
	depth = float(d);
}

// #pragma mark - Moving tracks

// handle_hit/move_selection: soft walls at +-250
void
Scene::KeepInside()
{
	for (int i = 0; i < CountTracks(); i++) {
		if (!fTracks[i].selected)
			continue;
		float x, y;
		GetPosition(i, x, y);
		float dx = 0, dz = 0;
		if (x >= 250)
			dx = (250 - x) * 0.9f - 1;
		if (x < -250)
			dx = -(x + 250) * 0.9f + 1;
		if (y >= 250)
			dz = (250 - y) * 0.9f - 1;
		if (y < -250)
			dz = -(y + 250) * 0.9f + 1;
		if (dx != 0 || dz != 0) {
			for (int k = 0; k < CountTracks(); k++) {
				if (fTracks[k].selected)
					MoveTrack(k, dx, 0, dz);
			}
		}
	}
}

void
Scene::DragSelection(float dx, float dy, bool depthOnly)
{
	float s = float(std::sin(double(-fYaw)));
	float c = float(std::cos(double(-fYaw)));
	float dz = s * dx + dy * c;
	float mx = dx * c - dy * s;
	if (depthOnly)
		mx = 0;
	for (int i = 0; i < CountTracks(); i++) {
		if (fTracks[i].selected)
			MoveTrack(i, mx, 0, dz);
	}
	KeepInside();
}

void
Scene::MoveSelection(float dx, float dz)
{
	for (int i = 0; i < CountTracks(); i++) {
		if (fTracks[i].selected)
			MoveTrack(i, dx, 0, dz);
	}
	KeepInside();
}

// #pragma mark - Drawing

void
Scene::Clear()
{
	std::fill(fFrame.begin(), fFrame.end(), 0);
}

// fline: clip to the frame, then draw
void
Scene::Line(int x1, int y1, int x2, int y2, const uint8_t* ramp, bool antialias)
{
	// top point (topX, topY), bottom (botX, botY)
	int topX = x1, topY = y1, botX = x2, botY = y2;
	if (y2 < y1) {
		topX = x2; topY = y2; botX = x1; botY = y1;
	}
	// left (lx, ly), right (rx, ry)
	int lx = topX, ly = topY, rx = botX, ry = botY;
	if (botX < topX) {
		lx = botX; ly = botY; rx = topX; ry = topY;
	}
	int dy = ry - ly, dx = rx - lx;
	if (lx < 0) {
		if (dx == 0)
			return;
		ly = Trunc(double(dy) * (double(-lx) / dx) + ly);
		lx = 0;
	}
	const int right = kWidth - 1;
	if (lx > right)
		return;
	if (rx > right) {
		ry = Trunc(ry - double(dy) * double(rx + 1 - kWidth) / dx);
		rx = right;
	}
	// top again
	int tx = lx, ty = ly, bx = rx, by = ry;
	if (ry < ly) {
		tx = rx; ty = ry; bx = lx; by = ly;
	}
	int ddy = by - ty, ddx = bx - tx;
	if (ty < 0) {
		if (ddy == 0)
			return;
		tx = Trunc(double(ddx) * (double(-ty) / ddy) + tx);
		ty = 0;
	}
	const int bottom = kHeight - 1 - 5;
	if (ty > bottom)
		return;
	if (by > bottom) {
		bx = Trunc(bx - double(ddx) * double(by + 5 - (kHeight - 1)) / ddy);
		by = bottom;
	}
	if (antialias)
		AntiLine(tx + 1, ty + 1, bx + 1, by + 1, ramp);
	else
		PlainLine(tx + 1, ty + 1, bx + 1, by + 1, ramp);
}

// anti_fline: two pixels per step, ramp[31 - f] and ramp[f]
void
Scene::AntiLine(int x1, int y1, int x2, int y2, const uint8_t* ramp)
{
	if (!(unsigned(y1 - 1) < 0x1de && y2 > 0 && y2 < 0x1df && unsigned(x1) < 0x401
			&& x2 >= 0 && x2 < 0x401))
		return;
	int dx = x2 - x1, dy = y2 - y1;
	int step = kWidth;
	uint8_t* p = fFrame.data() + x1 + y1 * kWidth;
	if (dx == 0 && dy == 0) {
		*p = ramp[0];
		return;
	}
	if (dy < 0) {
		dy = -dy;
		step = -kWidth;
	}
	int acc = dy >> 1;
	if (dx < 1) {
		dx = -dx;
		if (dy < dx) {
			double k = double(0x1f0000 / dx);
			int a = dx >> 1;
			int limit = Trunc(k * dx);
			int add = Trunc(k * dy);
			while (dx >= 0) {
				int f = a >> 16;
				*p = ramp[31 - f];
				dx--;
				a += add;
				p[step] = ramp[f];
				p--;
				if (limit <= a) {
					p += step;
					a -= limit;
				}
			}
		} else {
			double k = double(0x1f0000 / dy);
			int limit = Trunc(k * dy);
			int add = Trunc(k * dx);
			while (dy >= 0) {
				int f = acc >> 16;
				p[-1] = ramp[f];
				dy--;
				acc += add;
				*p = ramp[31 - f];
				p += step;
				if (limit <= acc) {
					p--;
					acc -= limit;
				}
			}
		}
	} else if (dy < dx) {
		double k = double(0x1f0000 / dx);
		int a = dx >> 1;
		int limit = Trunc(k * dx);
		int add = Trunc(k * dy);
		while (dx >= 0) {
			int f = a >> 16;
			*p = ramp[31 - f];
			dx--;
			a += add;
			p[step] = ramp[f];
			p++;
			if (limit <= a) {
				p += step;
				a -= limit;
			}
		}
	} else {
		double k = double(0x1f0000 / dy);
		int limit = Trunc(k * dy);
		int add = Trunc(k * dx);
		while (dy >= 0) {
			int f = acc >> 16;
			*p = ramp[31 - f];
			dy--;
			acc += add;
			p[1] = ramp[f];
			p += step;
			if (limit <= acc) {
				p++;
				acc -= limit;
			}
		}
	}
}

// no_anti_fline: one pixel per step in ramp[31]
void
Scene::PlainLine(int x1, int y1, int x2, int y2, const uint8_t* ramp)
{
	if (!(unsigned(y1 - 1) < 0x1de && y2 > 0 && y2 < 0x1df && unsigned(x1) < 0x401
			&& x2 >= 0 && x2 < 0x401))
		return;
	int dx = x2 - x1, dy = y2 - y1;
	int step = kWidth;
	uint8_t* p = fFrame.data() + x1 + y1 * kWidth;
	const uint8_t color = ramp[31];
	if (dx == 0 && dy == 0) {
		*p = ramp[0];
		return;
	}
	if (dy < 0) {
		dy = -dy;
		step = -kWidth;
	}
	int acc = dy >> 1;
	if (dx < 1) {
		int adx = -dx;
		if (adx == dy || adx < dy) {
			double k = double(0x1f0000 / dy);
			int limit = Trunc(k * dy);
			int add = Trunc(k * adx);
			while (dy >= 0) {
				dy--;
				*p = color;
				p += step;
				acc += add;
				if (limit <= acc) {
					p--;
					acc -= limit;
				}
			}
		} else {
			double k = double(0x1f0000 / adx);
			int a = adx >> 1;
			int limit = Trunc(k * adx);
			int add = Trunc(k * dy);
			while (adx >= 0) {
				adx--;
				*p = color;
				p--;
				a += add;
				if (limit <= a) {
					p += step;
					a -= limit;
				}
			}
		}
	} else if (dy < dx) {
		double k = double(0x1f0000 / dx);
		int a = dx >> 1;
		int limit = Trunc(k * dx);
		int add = Trunc(k * dy);
		while (dx >= 0) {
			dx--;
			*p = color;
			p++;
			a += add;
			if (limit <= a) {
				p += step;
				a -= limit;
			}
		}
	} else {
		double k = double(0x1f0000 / dy);
		int limit = Trunc(k * dy);
		int add = Trunc(k * dx);
		while (dy >= 0) {
			dy--;
			*p = color;
			p += step;
			acc += add;
			if (limit <= acc) {
				p++;
				acc -= limit;
			}
		}
	}
}

// fill_4's edge walk: per row, the leftmost and rightmost x of the quad's
// outline. A horizontal edge sets its row's span outright.
bool
Scene::Spans(const float* xs, const float* ys, int& top, int& bottom)
{
	int X[5], Y[5];
	for (int i = 0; i < 4; i++) {
		X[i] = Trunc(xs[i]);
		Y[i] = Trunc(ys[i]);
	}
	X[4] = X[0];
	Y[4] = Y[0];
	top = 10000;
	bottom = -10000;
	for (int i = 0; i < 4; i++) {
		bottom = std::max(bottom, Y[i]);
		top = std::min(top, Y[i]);
	}
	if (top > kHeight - 1 || bottom < 0 || bottom - top > 20000)
		return false;
	const size_t rows = size_t(bottom - top + 1);
	fSpanLeft.assign(rows, 0x3ff);
	fSpanRight.assign(rows, -1);
	int* left = fSpanLeft.data();
	int* right = fSpanRight.data();

	for (int i = 0; i < 4; i++) {
		int xa = X[i], ya = Y[i], xb = X[i + 1], yb = Y[i + 1];
		if (xb < xa) {
			std::swap(xa, xb);
			std::swap(ya, yb);
		}
		// xa <= xb
		int endRow = yb - top;
		left[endRow] = std::min(left[endRow], xb);
		right[endRow] = std::max(right[endRow], xb);
		int row = ya - top;
		int* l = left + row;
		int* r = right + row;
		*l = std::min(*l, xa);
		*r = std::max(*r, xa);
		int adx = std::abs(xb - xa);
		int dyv = yb - ya;
		int ady = std::abs(dyv);
		int sign = dyv < 0 ? -1 : 1;
		int x = xa;
		if (ady == 0) {
			*l = xa;
			*r = xb;
		} else if (adx < ady) {
			int acc = ady / 2;
			do {
				if (ady <= acc) {
					acc -= ady;
					x++;
				}
				*l = std::min(*l, x);
				l += sign;
				*r = std::max(*r, x);
				acc += adx;
				r += sign;
				row += sign;
			} while (row != endRow);
		} else {
			int acc = adx / 2;
			for (; x <= xb; x++) {
				if (adx <= acc) {
					acc -= adx;
					l += sign;
					r += sign;
				}
				*l = std::min(*l, x);
				acc += ady;
				*r = std::max(*r, x);
			}
		}
	}
	return true;
}

// fill_4: remaps the pixels under a quad. Its fast path handles 8 pixels
// as two 32-bit words and writes map[byte 3] to the first pixel: right on
// big-endian PowerPC, but on x86 (the BeOS the reference runs on) every
// group of four pixels from the span's start comes out mirrored. Kept.
void
Scene::Fill4(const float* xs, const float* ys, const uint8_t* map)
{
	int top, bottom;
	if (!Spans(xs, ys, top, bottom))
		return;
	for (int y = top; y <= bottom && y <= kHeight - 1; y++) {
		int xl = std::max(0, fSpanLeft[y - top]);
		int xr = std::min(kWidth, fSpanRight[y - top]);
		if (y < 0 || y >= kHeight - 2)
			continue;
		uint8_t* p = fFrame.data() + y * kWidth + xl;
		int n = xr - xl;
		for (; n > 7; n -= 8) {
			for (int w = 0; w < 2; w++, p += 4) {
				if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 0) {
					p[0] = p[1] = p[2] = p[3] = map[0];
				} else {
					uint8_t a = p[0], b = p[1], c = p[2], d = p[3];
					p[0] = map[d];
					p[1] = map[c];
					p[2] = map[b];
					p[3] = map[a];
				}
			}
		}
		for (; n >= 0; n--, p++)
			*p = map[*p];
	}
}

// sort_polys: Shell sort by mean depth, far to near (stable enough to match
// the original's order for equal depths)
void
Scene::SortPolys()
{
	const int count = int(fPolys.size());
	std::vector<float> depth(count);
	for (int i = 0; i < count; i++) {
		const Poly& p = fPolys[i];
		depth[i] = (fVertices[p.v[0]].depth + fVertices[p.v[1]].depth
			+ fVertices[p.v[2]].depth + fVertices[p.v[3]].depth) * 0.25f;
	}
	for (int gap = count / 2; gap > 0; gap /= 2) {
		for (int i = gap; i < count; i++) {
			for (int j = i - gap; j >= 0 && depth[j + gap] < depth[j]; j -= gap) {
				std::swap(fPolys[j], fPolys[j + gap]);
				std::swap(depth[j], depth[j + gap]);
			}
		}
	}
}

// move_pixels, recycle_pixel and draw_particules: fountains of coloured
// dots from the loudest tracks
void
Scene::DrawParticles()
{
	if (!fParticles)
		return;
	static int sSpread = 1000;	// recycle_pixel's vv
	for (size_t i = 0; i < fParticleList.size(); i++) {
		Particle& p = fParticleList[i];
		float* v = &fParticleVelocity[i * 3];
		p.x += v[0];
		p.y += v[1];
		p.z += v[2];
		v[1] += 0.1211f;
		if (p.y >= -4.0f) {
			v[1] = -v[1] * 0.4f;
			if (std::fabs(v[1]) <= 2.7f) {
				// recycle
				p.y = -80;
				if (fTracks.empty()) {
					p.x = float(Random() % 8);
					p.z = float(Random() % 8);
					v[0] = float(Random() % 1000 - 500) / 800.0f;
					sSpread += 10;
					if (sSpread > 25000)
						sSpread = 1000;
					v[1] = float(-(Random() % sSpread)) / 1800.0f;
					v[2] = float(Random() % (sSpread / 10) - 500) / 800.0f;
				} else {
					const int count = CountTracks();
					auto loudness = [this](int t) {
						return fTracks[t].silent ? 0.0f : -fTracks[t].level / 11.0f;
					};
					int t = -1;
					for (int tries = 5; tries > 0 && t < 0; tries--) {
						int k = Random() % count;
						if (loudness(k) < -0.2f)
							t = k;
					}
					for (int k = 0; t < 0 && k < count; k++) {
						if (loudness(k) < -0.1f)
							t = k;
					}
					if (t < 0)
						t = Random() % count;
					float tx, tz;
					GetPosition(t, tx, tz);
					p.x = tx - 4 + float(Random() % 8);
					p.z = tz - 4 + float(Random() % 8);
					v[0] = float(Random() % 1000 - 500) / 800.0f;
					int r = Random();
					float l = loudness(t);
					v[1] = (float((r / 1000) * 1000 - r) / 1800.0f - std::fabs(l * l) * 5.0f) * 0.55f;
					v[2] = float(Random() % 1000 - 500) / 800.0f;
				}
				p.color = uint8_t(Random());
			}
		}
	}
	for (Particle& p : fParticleList) {
		float sx, sy, depth;
		Project(p.x, p.y, p.z, sx, sy, depth);
		int x = Trunc(sx) + 512;
		int y = Trunc(sy) + fCenterY;
		if (x < 0 || x > kWidth || y < 0 || y > kHeight - 2)
			continue;
		uint8_t* q = fFrame.data() + y * kWidth + x;
		q[0] = q[1] = p.color;
		if (x > 0 || y > 0)
			q[-1] = p.color;
		q[kWidth] = p.color;
		if (y > 0)
			q[-kWidth] = p.color;
	}
}

// Animate's frame: clear, vectors, particles, faces, label lines
void
Scene::Render(const std::function<float(const std::string&)>& textWidth)
{
	const ColorTables& tables = ColorTables::Get();
	Clear();

	for (Vector& v : fVectors) {
		float d;
		Project(v.x1, v.y1, v.z1, v.sx1, v.sy1, d);
		Project(v.x2, v.y2, v.z2, v.sx2, v.sy2, d);
		const uint8_t* ramp;
		if (v.color == 200)
			ramp = tables.RampAt(kRampRed).index;
		else if (v.color == 100)
			ramp = tables.RampAt(kRampGreen).index;
		else if (v.z2 == v.z1)
			ramp = tables.RampAt(kRampDepthBlue + 19).index;
		else
			ramp = tables.RampAt(kRampBlue).index;
		Line(Trunc(v.sx1 + 512), Trunc(v.sy1 + fCenterY), Trunc(v.sx2 + 512),
			Trunc(v.sy2 + fCenterY), ramp, true);
	}

	DrawParticles();

	for (Vertex& v : fVertices)
		Project(v.x, v.y, v.z, v.sx, v.sy, v.depth);
	SortPolys();
	for (const Poly& p : fPolys) {
		float xs[4], ys[4];
		for (int k = 0; k < 4; k++) {
			xs[k] = fVertices[p.v[k]].sx + 512;
			ys[k] = fVertices[p.v[k]].sy + fCenterY;
		}
		Fill4(xs, ys, tables.MapperAt(p.mapper).index);
	}

	// draw_labels: a grey line from the anchor up to the label, labels
	// nudged up while they overlap one placed before
	fLabels.clear();
	std::vector<int> placedX, placedY, placedHalf;
	const uint8_t* grey = tables.RampAt(kRampGrey).index;
	for (size_t i = 0; i + 1 < fVertices.size(); i++) {
		const Vertex& anchor = fVertices[i];
		if (anchor.owner < kAnchor)
			continue;
		const int track = anchor.owner - kAnchor;
		const Vertex& point = fVertices[i + 1];
		int x = Trunc(point.sx + 512.0f);
		int y = Trunc(float(fCenterY) + point.sy);
		std::string text = std::to_string(std::min(track, 99)) + "-" + fTracks[track].name;
		int half = Trunc(textWidth(text)) / 2;
		for (bool moved = true; moved && !placedX.empty();) {
			moved = false;
			for (size_t k = 0; k < placedX.size(); k++) {
				int ddx = std::abs(placedX[k] - x);
				int ddy = std::abs(placedY[k] - y);
				int w = std::abs(half + placedHalf[k]);
				if (ddx - w < 15 && ddy < 15) {
					y -= 2;
					moved = true;
					break;
				}
			}
		}
		placedX.push_back(x);
		placedY.push_back(y);
		placedHalf.push_back(half);
		Line(Trunc(anchor.sx + 512.0f), Trunc(anchor.sy + float(fCenterY)), x, y, grey, false);
		Label label = {x - half, y - 10, text, fTracks[track].selected};
		fLabels.push_back(label);
	}
}

// find_hit and test_hit: the first face in drawing order (farthest first)
// whose outline contains the point
int
Scene::HitTest(float px, float py)
{
	for (Vertex& v : fVertices)
		Project(v.x, v.y, v.z, v.sx, v.sy, v.depth);
	SortPolys();
	const int x = Trunc(px), y = Trunc(py);
	for (const Poly& p : fPolys) {
		float xs[4], ys[4];
		for (int k = 0; k < 4; k++) {
			xs[k] = fVertices[p.v[k]].sx + 512;
			ys[k] = fVertices[p.v[k]].sy + fCenterY;
		}
		int top, bottom;
		if (!Spans(xs, ys, top, bottom) || y < top || y > bottom)
			continue;
		if (x >= fSpanLeft[y - top] && x <= fSpanRight[y - top])
			return p.owner;
	}
	return -1;
}

}	// namespace pmx
