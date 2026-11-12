// AboutWindow.cpp: see AboutWindow.h. TAboutView::pre_render draws the
// name at 128 points into a bitmap, fill_parray turns its pixels into
// particles thrown by sin/cos of their place, do_anim brings them home,
// do_scroll scrolls text below. This follows that, with PowerMix's text.
#include "AboutWindow.h"

#include <Bitmap.h>
#include <Catalog.h>
#include <MessageRunner.h>
#include <String.h>
#include <View.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "AudioEngine.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AboutWindow"

namespace {

const float kWidth = 480, kHeight = 350;
enum { kMsgTick = 'tick' };

struct Dot {
	float x, y, vx, vy;
	float homeX, homeY;
	rgb_color color;
};

class AboutView : public BView {
public:
	AboutView(BRect frame)
		:
		BView(frame, "about_view", B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
		fFrame(0),
		fScatter(false),
		fRunner(NULL)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		fBitmap = new BBitmap(Bounds(), B_RGB32, true);
		fPen = new BView(Bounds(), "pen", B_FOLLOW_NONE, 0);
		fBitmap->AddChild(fPen);
		BuildDots();
		const char* lines[] = {
			"PowerMix 0.2",
			B_TRANSLATE("A 3D sound mixer in the spirit of 3dmiX,"),
			B_TRANSLATE("the mixer that came with BeOS 5."),
			"",
			B_TRANSLATE("Drag the tracks around the floor to mix them."),
			"",
			"\xC2\xA9 2026 the PowerMix authors",
			B_TRANSLATE("MIT licence"),
			"",
			B_TRANSLATE("3dmiX was made by Be, Inc."),
			NULL
		};
		for (int i = 0; lines[i] != NULL; i++)
			fCredits.push_back(lines[i]);
	}

	virtual ~AboutView()
	{
		delete fRunner;
		delete fBitmap;
	}

	virtual void AttachedToWindow()
	{
		BMessage tick(kMsgTick);
		fRunner = new BMessageRunner(BMessenger(this), &tick, 30000);
	}

	// pre_render and fill_parray
	void BuildDots()
	{
		fBitmap->Lock();
		fPen->SetHighColor(0, 0, 0);
		fPen->FillRect(Bounds());
		BFont font(be_bold_font);
		float size = 128;
		font.SetSize(size);
		while (font.StringWidth("PowerMix") > kWidth - 30 && size > 20) {
			size -= 4;
			font.SetSize(size);
		}
		fPen->SetFont(&font);
		fPen->SetHighColor(255, 255, 255);
		float x = (kWidth - font.StringWidth("PowerMix")) / 2;
		fPen->DrawString("PowerMix", BPoint(x, 190));
		fPen->Sync();
		const uint8* bits = static_cast<const uint8*>(fBitmap->Bits());
		const int32 bpr = fBitmap->BytesPerRow();
		for (int y = 0; y < int(kHeight); y += 2) {
			for (int x2 = 0; x2 < int(kWidth); x2 += 2) {
				if (bits[y * bpr + x2 * 4 + 1] < 128)
					continue;
				Dot d;
				d.homeX = x2;
				d.homeY = y;
				d.x = x2;
				d.y = y;
				d.vx = (std::sin(x2 / 10.0f) + float(rand() % 1000 - 500) / 3000.0f) * 8;
				d.vy = (std::cos(x2 / 10.0f) + float(rand() % 1000 - 500) / 3000.0f) * 8;
				// spin them out first; they come home in do_anim
				for (int k = 0; k < 60; k++) {
					d.x -= d.vx;
					d.y -= d.vy;
				}
				uint8 v = uint8(std::max(120, std::min(255, 150 + (y - 100) * 105 / 100)));
				d.color = rgb_color{uint8(v / 3), uint8(v * 2 / 3), v, 255};
				fDots.push_back(d);
			}
		}
		fBitmap->Unlock();
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what != kMsgTick) {
			BView::MessageReceived(message);
			return;
		}
		fFrame++;
		for (Dot& d : fDots) {
			if (fScatter) {
				d.x += d.vx;
				d.y += d.vy;
				d.vx *= 1.08f;
				d.vy *= 1.08f;
			} else {
				d.x += (d.homeX - d.x) * 0.07f;
				d.y += (d.homeY - d.y) * 0.07f;
			}
		}
		Render();
		Invalidate();
		if (fScatter && fFrame > fScatterFrame + 25)
			Window()->PostMessage(B_QUIT_REQUESTED);
	}

	void Render()
	{
		fBitmap->Lock();
		// cleared directly: a FillRect would reach the app_server after the
		// dots are written
		uint32* bits = static_cast<uint32*>(fBitmap->Bits());
		for (int32 i = 0; i < int32(fBitmap->BitsLength() / 4); i++)
			bits[i] = 0xff000000;
		const int32 stride = fBitmap->BytesPerRow() / 4;
		for (const Dot& d : fDots) {
			int x = int(d.x), y = int(d.y);
			if (x < 0 || y < 0 || x >= int(kWidth) - 1 || y >= int(kHeight) - 1)
				continue;
			uint32 c = 0xff000000 | uint32(d.color.red) << 16 | uint32(d.color.green) << 8
				| d.color.blue;
			bits[y * stride + x] = c;
			bits[y * stride + x + 1] = c;
			bits[(y + 1) * stride + x] = c;
		}
		// do_scroll: the credits after the name has formed
		if (fFrame > 90 && !fScatter) {
			BFont font(be_plain_font);
			font.SetSize(12);
			fPen->SetFont(&font);
			fPen->SetDrawingMode(B_OP_OVER);
			float lineHeight = 18;
			float top = kHeight - (fFrame - 90) * 0.7f;
			float total = fCredits.size() * lineHeight;
			while (top + total < 220)
				top += total + 120;
			for (size_t i = 0; i < fCredits.size(); i++) {
				float y = top + i * lineHeight;
				if (y < 225 || y > kHeight - 4)
					continue;
				float fade = std::min(1.0f, std::min((y - 225) / 30, (kHeight - y) / 30));
				uint8 v = uint8(220 * std::max(0.0f, fade));
				fPen->SetHighColor(v, v, v);
				const char* text = fCredits[i].String();
				fPen->DrawString(text, BPoint((kWidth - font.StringWidth(text)) / 2, y));
			}
			fPen->SetDrawingMode(B_OP_COPY);
		}
		fPen->Sync();
		fBitmap->Unlock();
	}

	virtual void Draw(BRect updateRect)
	{
		DrawBitmap(fBitmap, BPoint(0, 0));
	}

	// TAboutView::MouseDown and KeyDown: scatter and close
	virtual void MouseDown(BPoint where)
	{
		Scatter();
	}

	virtual void KeyDown(const char* bytes, int32 count)
	{
		Scatter();
	}

	void Scatter()
	{
		if (fScatter)
			return;
		fScatter = true;
		fScatterFrame = fFrame;
		for (Dot& d : fDots) {
			d.vx = (d.x - kWidth / 2) / 60 + float(rand() % 100 - 50) / 50.0f;
			d.vy = (d.y - kHeight / 2) / 60 + float(rand() % 100 - 50) / 50.0f;
		}
	}

private:
	BBitmap* fBitmap;
	BView* fPen;
	std::vector<Dot> fDots;
	std::vector<BString> fCredits;
	int fFrame;
	int fScatterFrame = 0;
	bool fScatter;
	BMessageRunner* fRunner;
};

}	// namespace

// do_about: BRect(280, 200, 760, 550)
PowerMixAboutWindow::PowerMixAboutWindow(AudioEngine* engine)
	:
	BWindow(BRect(280, 200, 280 + kWidth - 1, 200 + kHeight - 1), B_TRANSLATE("About PowerMix"),
		B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_CLOSE_ON_ESCAPE),
	fEngine(engine)
{
	AboutView* view = new AboutView(Bounds());
	AddChild(view);
	view->MakeFocus(true);
	fEngine->StartAbout();
}

bool
PowerMixAboutWindow::QuitRequested()
{
	fEngine->FadeOutAbout();
	return true;
}
