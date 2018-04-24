#pragma once

#include "kutil/coord.h"
#include "font.h"
#include "screen.h"

class console_out_screen;

class console
{
public:
	console();

	void set_color(uint8_t fg, uint8_t bg);

	void puts(const char *message);
	void printf(const char *fmt, ...);

	template <typename T>
	void put_hex(T x);

	template <typename T>
	void put_dec(T x);

	static console * get();

private:
	console_out_screen *m_screen;
};

extern console g_console;
inline console * console::get() { return &g_console; }


console_out_screen * console_get_screen_out(
		const font &f, const screen &s, void *scratch, size_t len);


extern const char digits[];

template <typename T>
void console::put_hex(T x)
{
	static const int chars = sizeof(x) * 2;
	char message[chars + 1];
	for (int i=0; i<chars; ++i) {
		message[chars - i - 1] = digits[(x >> (i*4)) & 0xf];
	}
	message[chars] = 0;
	puts(message);
}

template <typename T>
void console::put_dec(T x)
{
	static const int chars = sizeof(x) * 3;
	char message[chars + 1];
	char *p = message + chars;
	*p-- = 0;
	do {
		*p-- = digits[x % 10];
		x /= 10;
	} while (x != 0);
	puts(++p);
}