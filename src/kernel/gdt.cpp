#include <stdint.h>

#include "kutil/assert.h"
#include "kutil/memory.h"
#include "kutil/no_construct.h"
#include "console.h"
#include "cpu.h"
#include "gdt.h"
#include "log.h"
#include "tss.h"

extern "C" void gdt_write(const void *gdt_ptr, uint16_t cs, uint16_t ds, uint16_t tr);

static constexpr uint8_t kern_cs_index   = 1;
static constexpr uint8_t kern_ss_index   = 2;
static constexpr uint8_t user_cs32_index = 3;
static constexpr uint8_t user_ss_index   = 4;
static constexpr uint8_t user_cs64_index = 5;
static constexpr uint8_t tss_index       = 6; // Note that this takes TWO GDT entries

// The BSP's GDT is initialized _before_ global constructors are called,
// so we don't want it to have a global constructor, lest it overwrite
// the previous initialization.
static kutil::no_construct<GDT> __g_bsp_gdt_storage;
GDT &g_bsp_gdt = __g_bsp_gdt_storage.value;


GDT::GDT(TSS *tss) :
	m_tss(tss)
{
	kutil::memset(this, 0, sizeof(GDT));

	m_ptr.limit = sizeof(m_entries) - 1;
	m_ptr.base = &m_entries[0];

	// Kernel CS/SS - always 64bit
	set(kern_cs_index, 0, 0xfffff, true, gdt_type::read_write | gdt_type::execute);
	set(kern_ss_index, 0, 0xfffff, true, gdt_type::read_write);

	// User CS32/SS/CS64 - layout expected by SYSRET
	set(user_cs32_index, 0, 0xfffff, false, gdt_type::ring3 | gdt_type::read_write | gdt_type::execute);
	set(user_ss_index,   0, 0xfffff, true,  gdt_type::ring3 | gdt_type::read_write);
	set(user_cs64_index, 0, 0xfffff, true,  gdt_type::ring3 | gdt_type::read_write | gdt_type::execute);

	set_tss(tss);
}

GDT &
GDT::current()
{
	cpu_data &cpu = current_cpu();
	return *cpu.gdt;
}

void
GDT::install() const
{
	gdt_write(
		static_cast<const void*>(&m_ptr),
		kern_cs_index << 3,
		kern_ss_index << 3,
		tss_index << 3);
}

void
GDT::set(uint8_t i, uint32_t base, uint64_t limit, bool is64, gdt_type type)
{
	m_entries[i].limit_low = limit & 0xffff;
	m_entries[i].size = (limit >> 16) & 0xf;
	m_entries[i].size |= (is64 ? 0xa0 : 0xc0);

	m_entries[i].base_low = base & 0xffff;
	m_entries[i].base_mid = (base >> 16) & 0xff;
	m_entries[i].base_high = (base >> 24) & 0xff;

	m_entries[i].type = type | gdt_type::system | gdt_type::present;
}

struct tss_descriptor
{
	uint16_t limit_low;
	uint16_t base_00;
	uint8_t base_16;
	gdt_type type;
	uint8_t size;
	uint8_t base_24;
	uint32_t base_32;
	uint32_t reserved;
} __attribute__ ((packed));

void
GDT::set_tss(TSS *tss)
{
	tss_descriptor tssd;

	size_t limit = sizeof(TSS);
	tssd.limit_low = limit & 0xffff;
	tssd.size = (limit >> 16) & 0xf;

	uintptr_t base = reinterpret_cast<uintptr_t>(tss);
	tssd.base_00 = base & 0xffff;
	tssd.base_16 = (base >> 16) & 0xff;
	tssd.base_24 = (base >> 24) & 0xff;
	tssd.base_32 = (base >> 32) & 0xffffffff;
	tssd.reserved = 0;

	tssd.type =
		gdt_type::accessed |
		gdt_type::execute |
		gdt_type::ring3 |
		gdt_type::present;

	kutil::memcpy(&m_entries[tss_index], &tssd, sizeof(tss_descriptor));
}

void
GDT::dump(unsigned index) const
{
	console *cons = console::get();

	unsigned start = 0;
	unsigned count = (m_ptr.limit + 1) / sizeof(descriptor);
	if (index != -1) {
		start = index;
		count = 1;
	} else {
		cons->printf("         GDT: loc:%lx size:%d\n", m_ptr.base, m_ptr.limit+1);
	}

	const descriptor *gdt =
		reinterpret_cast<const descriptor *>(m_ptr.base);

	for (int i = start; i < start+count; ++i) {
		uint32_t base =
			(gdt[i].base_high << 24) |
			(gdt[i].base_mid << 16) |
			gdt[i].base_low;

		uint32_t limit =
			static_cast<uint32_t>(gdt[i].size & 0x0f) << 16 |
			gdt[i].limit_low;

		cons->printf("          %02d:", i);
		if (! bitfield_has(gdt[i].type, gdt_type::present)) {
			cons->puts(" Not Present\n");
			continue;
		}

		cons->printf(" Base %08x  limit %05x ", base, limit);

		switch (gdt[i].type & gdt_type::ring3) {
			case gdt_type::ring3: cons->puts("ring3"); break;
			case gdt_type::ring2: cons->puts("ring2"); break;
			case gdt_type::ring1: cons->puts("ring1"); break;
			default: cons->puts("ring0"); break;
		}

		cons->printf(" %s %s %s %s %s %s %s\n",
			bitfield_has(gdt[i].type, gdt_type::accessed) ? "A" : " ",
			bitfield_has(gdt[i].type, gdt_type::read_write) ? "RW" : "  ",
			bitfield_has(gdt[i].type, gdt_type::conforming) ? "C" : " ",
			bitfield_has(gdt[i].type, gdt_type::execute) ? "EX" : "  ",
			bitfield_has(gdt[i].type, gdt_type::system) ? "S" : " ",
			(gdt[i].size & 0x80) ? "KB" : " B",
			(gdt[i].size & 0x60) == 0x20 ? "64" :
				(gdt[i].size & 0x60) == 0x40 ? "32" : "16");
	}
}
