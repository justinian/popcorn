#include <stddef.h>
#include <stdint.h>

#include "kutil/memory.h"
#include "acpi_tables.h"
#include "assert.h"
#include "device_manager.h"
#include "log.h"

static const char expected_signature[] = "RSD PTR ";

struct acpi1_rsdp
{
	char signature[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;
} __attribute__ ((packed));

struct acpi2_rsdp
{
	char signature[8];
	uint8_t checksum10;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;

	uint32_t length;
	uint64_t xsdt_address;
	uint8_t checksum20;
	uint8_t reserved[3];
} __attribute__ ((packed));


uint8_t
acpi_checksum(const void *p, size_t len, size_t off = 0)
{
	uint8_t sum = 0;
	const uint8_t *c = reinterpret_cast<const uint8_t *>(p);
	for (int i = off; i < len; ++i) sum += c[i];
	return sum;
}

bool
acpi_table_header::validate(uint32_t expected_type) const
{
	if (acpi_checksum(this, length) != 0) return false;
	return !expected_type || (expected_type == type);
}

device_manager::device_manager(const void *root_table) :
	m_local_apic(nullptr),
	m_io_apic(nullptr),
	m_global_interrupt_base(0)
{
	kassert(root_table != 0, "ACPI root table pointer is null.");

	const acpi1_rsdp *acpi1 =
		reinterpret_cast<const acpi1_rsdp *>(root_table);

	for (int i = 0; i < sizeof(acpi1->signature); ++i)
		kassert(acpi1->signature[i] == expected_signature[i],
				"ACPI RSDP table signature mismatch");

	uint8_t sum = acpi_checksum(acpi1, sizeof(acpi1_rsdp), 0);
	kassert(sum == 0, "ACPI 1.0 RSDP checksum mismatch.");

	kassert(acpi1->revision > 1, "ACPI 1.0 not supported.");

	const acpi2_rsdp *acpi2 =
		reinterpret_cast<const acpi2_rsdp *>(acpi1);

	sum = acpi_checksum(acpi2, sizeof(acpi2_rsdp), sizeof(acpi1_rsdp));
	kassert(sum == 0, "ACPI 2.0 RSDP checksum mismatch.");

	load_xsdt(reinterpret_cast<const acpi_xsdt *>(acpi2->xsdt_address));
}

static void
put_sig(char *into, uint32_t type)
{
	for (int j=0; j<4; ++j) into[j] = reinterpret_cast<char *>(&type)[j];
}

void
device_manager::load_xsdt(const acpi_xsdt *xsdt)
{
	kassert(xsdt && acpi_validate(xsdt), "Invalid ACPI XSDT.");

	char sig[5] = {0,0,0,0,0};
	log::info(logs::devices, "ACPI 2.0 tables loading:");

	put_sig(sig, xsdt->header.type);
	log::info(logs::devices, "  Found table %s", sig);

	size_t num_tables = acpi_table_entries(xsdt, sizeof(void*));
	for (size_t i = 0; i < num_tables; ++i) {
		const acpi_table_header *header = xsdt->headers[i];

		put_sig(sig, header->type);
		log::info(logs::devices, "  Found table %s", sig);

		kassert(header->validate(), "Table failed validation.");

		switch (header->type) {
		case acpi_apic::type_id:
			load_apic(reinterpret_cast<const acpi_apic *>(header));
			break;

		default:
			break;
		}
	}
}

void
device_manager::load_apic(const acpi_apic *apic)
{
	m_local_apic = reinterpret_cast<uint32_t *>(apic->local_address);
	log::info(logs::devices, "    APIC local address %lx", apic->local_address);

	uint8_t const *p = apic->controller_data;
	uint8_t const *end = p + acpi_table_entries(apic, 1);

	while (p < end) {
		const uint8_t type = p[0];
		const uint8_t length = p[1];

		switch (type) {
		case 0: // Local APIC
			break;

		case 1: // I/O APIC
			m_io_apic = reinterpret_cast<uint32_t *>(kutil::read_from<uint32_t>(p+4));
			m_global_interrupt_base = kutil::read_from<uint32_t>(p+8);
			log::info(logs::devices, "    IO APIC address %lx base %d",
					m_io_apic, m_global_interrupt_base);
			break;

		case 2: // Interrupt source override
			log::info(logs::devices, "    Intr source override IRQ %d -> %d Pol %d Tri %d",
					kutil::read_from<uint8_t>(p+3),
					kutil::read_from<uint32_t>(p+4),
					kutil::read_from<uint16_t>(p+8) & 0x3,
					(kutil::read_from<uint16_t>(p+8) >> 2) & 0x3);
			break;

		case 4: // Interrupt source override
			log::info(logs::devices, "    LAPIC NMI Proc %d LINT%d Pol %d Tri %d",
					kutil::read_from<uint8_t>(p+2),
					kutil::read_from<uint8_t>(p+5),
					kutil::read_from<uint16_t>(p+3) & 0x3,
					(kutil::read_from<uint16_t>(p+3) >> 2) & 0x3);
			break;

		default:
			log::debug(logs::devices, "    APIC entry type %d", type);
		}

		p += length;
	}
}
