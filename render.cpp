#include "includes.h"
#include <cmath>
#include <math.h>       /* round, floor, ceil, trunc */

namespace render {
	Font menu;;
	Font menu_shade;;
	Font esp;;
	Font esp_small;;
	Font hud;;
	Font cs;;
	Font indicator;;
	Font logevent;;
	Font warning;;
	Font warning2;;
}

void render::init() {
	menu = Font(XOR("Tahoma"), 12, FW_NORMAL, FONTFLAG_NONE);
	menu_shade = Font(XOR("Tahoma"), 12, FW_NORMAL, FONTFLAG_DROPSHADOW);
	//esp = Font(XOR("Verdana"), 12, FW_BOLD, FONTFLAG_DROPSHADOW);
	//esp_small = Font(XOR("Small Fonts"), 8, FW_NORMAL, FONTFLAG_OUTLINE); sup
	esp = Font(XOR("Verdana"), 12, FW_BOLD, FONTFLAG_DROPSHADOW); // | old sup style
	//esp_small = Font(XOR("Tahoma"), 12, FW_NORMAL, FONTFLAG_DROPSHADOW);
	// esp = Font(XOR("Verdana"), 12, FW_NORMAL, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW);
	esp_small = Font(XOR("Small Fonts"), 8, FW_NORMAL, FONTFLAG_OUTLINE);
	hud = Font(XOR("Tahoma"), 16, FW_NORMAL, FONTFLAG_ANTIALIAS);
	cs = Font(XOR("Counter-Strike"), 28, FW_MEDIUM, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW);
	indicator = Font(XOR("Verdana"), 26, FW_BOLD, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW);
	logevent = Font(XOR("Lucida Console"), 10, FW_DONTCARE, FONTFLAG_DROPSHADOW);
	warning = Font(XOR("undefeated"), 22, FW_SEMIBOLD, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW);
	warning2 = Font(XOR("Verdana"), 12, FW_NORMAL, FONTFLAG_DROPSHADOW);
}

//void DecreaseAlpha(Color& color, float percent)
//{
//	if (percent < 1.0f) {
//		float startTime = g_csgo.m_globals->m_curtime;
//		while (g_csgo.m_globals->m_curtime - startTime < 1.0f) {
//			float elapsed = g_csgo.m_globals->m_curtime - startTime;
//			float alpha = 180.0f - 180.0f * elapsed;
//			if (alpha < 0.0f) alpha = 0.0f;
//			color.a() = alpha;
//			Sleep(1); // sleep for 1 millisecond to avoid busy loop
//		}
//		color.a() = 0.0f;
//	}
//}

void render::gradient1337(int x, int y, int w, int h, Color color1, Color color2) {
	g_csgo.m_surface->DrawSetColor(color1);
	g_csgo.m_surface->DrawFilledRectcheat(x, y, x + w, y + h, color1.a(), 0, true);

	g_csgo.m_surface->DrawSetColor(color2);
	g_csgo.m_surface->DrawFilledRectcheat(x, y, x + w, y + h, 0, color2.a(), true);
}

void render::world_circle(vec3_t origin, float radius, float angle, Color color) {
	std::vector< Vertex > vertices{};

	float step = (1.f / radius) + math::deg_to_rad(angle);

	float lat = 1.f;
	vertices.clear();

	for (float lon{}; lon < math::pi_2; lon += step) {
		vec3_t point{
			origin.x + (radius * std::sin(lat) * std::cos(lon)),
			origin.y + (radius * std::sin(lat) * std::sin(lon)),
			origin.z + (radius * std::cos(lat) - (radius / 2))
		};

		vec2_t screen;
		if (WorldToScreen(point, screen))
			vertices.emplace_back(screen);
	}
	static int texture = g_csgo.m_surface->CreateNewTextureID(false);

	g_csgo.m_surface->DrawSetTextureRGBA(texture, &colors::white, 1, 1);
	g_csgo.m_surface->DrawSetColor(color);

	//g_csgo.m_surface->DrawSetTexture(texture);
	//g_csgo.m_surface->DrawTexturedPolygon(vertices.size(), vertices.data());

	g_csgo.m_surface->DrawTexturedPolyLine(vertices.size(), vertices.data());
}

bool render::WorldToScreen(const vec3_t& world, vec2_t& screen) {
	float w;

	const VMatrix& matrix = g_csgo.m_engine->WorldToScreenMatrix();

	// check if it's in view first.
	// note - dex; w is below 0 when world position is around -90 / +90 from the player's camera on the y axis.
	w = matrix[3][0] * world.x + matrix[3][1] * world.y + matrix[3][2] * world.z + matrix[3][3];
	if (w < 0.001f)
		return false;

	// calculate x and y.
	screen.x = matrix[0][0] * world.x + matrix[0][1] * world.y + matrix[0][2] * world.z + matrix[0][3];
	screen.y = matrix[1][0] * world.x + matrix[1][1] * world.y + matrix[1][2] * world.z + matrix[1][3];

	screen /= w;

	// calculate screen position.
	screen.x = (g_cl.m_width / 2) + (screen.x * g_cl.m_width) / 2;
	screen.y = (g_cl.m_height / 2) - (screen.y * g_cl.m_height) / 2;

	return true;
}

//void render::Font::semi_filled_text(int x, int y, Color color, const std::string& text, StringFlags_t flags, float factor, bool vertical)
//{
//	auto indicator_size = wsize(util::MultiByteToWide(text));
//	auto position = vec2_t(x, y);
//
//	wstring(x, y, Color(30, 30, 30, 200), util::MultiByteToWide(text), flags);
//	*(bool*)((DWORD)g_csgo.m_surface + 0x280) = true;
//	int x1, y1, x2, y2;
//	g_csgo.m_surface->get_drawing_area(x1, y1, x2, y2);
//	g_csgo.m_surface->limit_drawing_area(position.x, position.y, int(indicator_size.m_width* factor), (int)indicator_size.m_height);
//
//
//	wstring(x, y, color, util::MultiByteToWide(text), flags);
//
//	g_csgo.m_surface->limit_drawing_area(x1, y1, x2, y2);
//	*(bool*)((DWORD)g_csgo.m_surface - +0x280) = false;
//}

void render::line(vec2_t v0, vec2_t v1, Color color) {
	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawLine(v0.x, v0.y, v1.x, v1.y);
}

void render::circle_outline(int x, int y, int radius, int segments, Color color) {
	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawOutlinedCircle(x, y, radius, segments);
}

void render::line(int x0, int y0, int x1, int y1, Color color) {
	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawLine(x0, y0, x1, y1);
}

void render::rect(int x, int y, int w, int h, Color color) {
	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawOutlinedRect(x, y, x + w, y + h);
}

void render::rect_filled(int x, int y, int w, int h, Color color) {
	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawFilledRect(x, y, x + w, y + h);
}

void render::rect_filled_cheat(int x, int y, int w, int h, Color color, int a1, int a2) {
	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawFilledRectcheat(x, y, x + w, y + h, a1, a2, false);
}

void render::rect_outlined(int x, int y, int w, int h, Color color, Color color2) {
	rect(x, y, w, h, color);
	rect(x - 1, y - 1, w + 2, h + 2, color2);
	rect(x + 1, y + 1, w - 2, h - 2, color2);
}

void render::rect_filled_fade(int x, int y, int w, int h, Color color, int a1, int a2) {
	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawFilledRectFade(x, y, x + w, y + h, a1, a2, false);
}


void render::arccircle(int x, int y, int r1, int r2, int s, int d, Color color) {
	g_csgo.m_surface->DrawSetColor(color);
	for (int i = s; i < s + d; i++) {

		float rad = i * math::pi / 180;

		g_csgo.m_surface->DrawLine(x + cos(rad) * r1, y + sin(rad) * r1, x + cos(rad) * r2, y + sin(rad) * r2);
	}
}

void render::drawCircle(int x, int y, int angle, Color color) {
	float granularity = 2 * math::pi / 700;
	float step = 2 * math::pi / 100;
	float inner = 6;

	for (int radius = 8; inner < -1; radius--) {
		for (int angle = 0; angle * step < -1; granularity++) {
			int px = round(radius * cos(angle));
			int py = round(radius * sin(angle));

			g_csgo.m_surface->DrawLine(px + x, py + y, px + 1 + x, py + 1 + y);
		}

	}

}

void render::draw_arc(int x, int y, int radius, int start_angle, int percent, int thickness, Color color) {
	float precision = (2 * math::pi) / 30;
	float step = math::pi / 180;
	float inner = radius - thickness;
	float end_angle = (start_angle + percent) * step;
	float start_angle1337 = (start_angle * math::pi) / 180;

	for (; radius > inner; --radius) {
		for (float angle = start_angle1337; angle < end_angle; angle += precision) {
			float cx = round(x + radius * cos(angle));
			float cy = round(y + radius * sin(angle));

			float cx2 = round(x + radius * cos(angle + precision));
			float cy2 = round(y + radius * sin(angle + precision));

			g_csgo.m_surface->DrawSetColor(color);
			g_csgo.m_surface->DrawLine(cx, cy, cx2, cy2);
		}
	}
}

void render::circle(int x, int y, int radius, int segments, Color color) {
	static int texture = g_csgo.m_surface->CreateNewTextureID(true);

	g_csgo.m_surface->DrawSetTextureRGBA(texture, &colors::white, 1, 1);
	g_csgo.m_surface->DrawSetColor(color);
	g_csgo.m_surface->DrawSetTexture(texture);

	std::vector< Vertex > vertices{};

	float step = math::pi_2 / segments;
	for (float i{ 0.f }; i < math::pi_2; i += step)
		vertices.emplace_back(vec2_t{ x + (radius * std::cos(i)), y + (radius * std::sin(i)) });

	g_csgo.m_surface->DrawTexturedPolygon(vertices.size(), vertices.data());
}
void render::gradient(int x, int y, int w, int h, Color color1, Color color2) {
	g_csgo.m_surface->DrawSetColor(color1);
	g_csgo.m_surface->DrawFilledRectcheat(x, y, x + w, y + h, color1.a(), 0, false);

	g_csgo.m_surface->DrawSetColor(color2);
	g_csgo.m_surface->DrawFilledRectcheat(x, y, x + w, y + h, 0, color2.a(), false);
}

void render::sphere(vec3_t origin, float radius, float angle, float scale, Color color) {
	std::vector< Vertex > vertices{};

	// compute angle step for input radius and precision.
	float step = (1.f / radius) + math::deg_to_rad(angle);

	for (float lat{}; lat < (math::pi * scale); lat += step) {
		// reset.
		vertices.clear();

		for (float lon{}; lon < math::pi_2; lon += step) {
			vec3_t point{
				origin.x + (radius * std::sin(lat) * std::cos(lon)),
				origin.y + (radius * std::sin(lat) * std::sin(lon)),
				origin.z + (radius * std::cos(lat))
			};

			vec2_t screen;
			if (WorldToScreen(point, screen))
				vertices.emplace_back(screen);
		}

		if (vertices.empty())
			continue;

		g_csgo.m_surface->DrawSetColor(color);
		g_csgo.m_surface->DrawTexturedPolyLine(vertices.size(), vertices.data());
	}
}

Vertex render::RotateVertex(const vec2_t& p, const Vertex& v, float angle) {
	// convert theta angle to sine and cosine representations.
	float c = std::cos(math::deg_to_rad(angle));
	float s = std::sin(math::deg_to_rad(angle));

	return {
		p.x + (v.m_pos.x - p.x) * c - (v.m_pos.y - p.y) * s,
		p.y + (v.m_pos.x - p.x) * s + (v.m_pos.y - p.y) * c
	};
}

void render::Font::string(int x, int y, Color color, const std::string& text, StringFlags_t flags /*= render::ALIGN_LEFT */) {
	wstring(x, y, color, util::MultiByteToWide(text), flags);
}

void render::Font::string(int x, int y, Color color, const std::stringstream& text, StringFlags_t flags /*= render::ALIGN_LEFT */) {
	wstring(x, y, color, util::MultiByteToWide(text.str()), flags);
}

void render::Font::wstring(int x, int y, Color color, const std::wstring& text, StringFlags_t flags /*= render::ALIGN_LEFT */) {
	int w, h;

	g_csgo.m_surface->GetTextSize(m_handle, text.c_str(), w, h);
	g_csgo.m_surface->DrawSetTextFont(m_handle);
	g_csgo.m_surface->DrawSetTextColor(color);

	if (flags & ALIGN_RIGHT)
		x -= w;
	if (flags & render::ALIGN_CENTER)
		x -= w / 2;

	g_csgo.m_surface->DrawSetTextPos(x, y);
	g_csgo.m_surface->DrawPrintText(text.c_str(), (int)text.size());
}

render::FontSize_t render::Font::size(const std::string& text) {
	return wsize(util::MultiByteToWide(text));
}

render::FontSize_t render::Font::wsize(const std::wstring& text) {
	FontSize_t res;
	g_csgo.m_surface->GetTextSize(m_handle, text.data(), res.m_width, res.m_height);
	return res;
}
