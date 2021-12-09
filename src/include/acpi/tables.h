#ifndef __ACPI_TABLES_H__
#define __ACPI_TABLES_H__

#include <stdint.h>

typedef struct rsdp {
  char signature[8];
  uint8_t checksum;
  char oem_id[6];
  uint8_t revision;
  uint32_t rsdt_address;

  /* 2.0 */
  uint32_t length;
  uint64_t xsdt_address;
  uint8_t echecksum;
  uint8_t reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct sdt {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__((packed)) sdt_t;

typedef struct rsdt {
  sdt_t h;
  uint32_t sptr[];
} __attribute__((packed)) rsdt_t;

typedef struct xsdt {
  sdt_t h;
  uint64_t sptr[];
} __attribute__((packed)) xsdt_t;

typedef struct madt {
  sdt_t header;
  uint32_t local_controller_address;
  uint32_t flags;
  uint8_t entries[0];
} __attribute__((packed)) madt_t;

typedef struct madt_header {
  uint8_t id;
  uint8_t length;
} __attribute__((packed)) madt_header_t;

typedef struct madt_lapic {
  madt_header_t header;
  uint8_t processor_id;
  uint8_t apic_id;
  uint32_t flags;
} __attribute__((packed)) madt_lapic_t;

typedef struct madt_ioapic {
  madt_header_t header;
  uint8_t apic_id;
  uint8_t reserved;
  uint32_t address;
  uint32_t gsi;
} __attribute__((packed)) madt_ioapic_t;

typedef struct madt_iso {
  madt_header_t header;
  uint8_t bus_source;
  uint8_t irq_source;
  uint16_t flags;
  uint32_t gsi;
} __attribute__((packed)) madt_iso_t;

typedef struct madt_nmi {
  madt_header_t header;
  uint8_t processor;
  uint8_t lint;
  uint16_t flags;
} __attribute__((packed)) madt_nmi_t;

typedef struct mcfg_entry {
  uint64_t base_address;
  uint16_t seg_grp;
  uint8_t sbus, ebus;
  uint32_t reserved;
} __attribute__((packed)) mcfg_entry_t;

typedef struct _mcfg {
  sdt_t h;
  uint64_t reserved;
  mcfg_entry_t entries[];
} __attribute__((packed)) mcfg_t;

// typedef struct GenericAddressStructure
//  {
//  uint8_t AddressSpace;
//  uint8_t BitWidth;
//  uint8_t BitOffset;
//  uint8_t AccessSize;
//  uint64_t Address;
// } GenericAddressStructure;

// struct ACPISDTHeader {
// char Signature[4];
// uint32_t Length;
// uint8_t Revision;
// uint8_t Checksum;
// char OEMID[6];
// char OEMTableID[8];
// uint32_t OEMRevision;
// uint32_t CreatorID;
// uint32_t CreatorRevision;
// };

// typedef struct FADT
// {
// struct   ACPISDTHeader h;
// uint32_t FirmwareCtrl;
// uint32_t Dsdt;

// // field used in ACPI 1.0; no longer in use, for compatibility only
// uint8_t  Reserved;

// uint8_t  PreferredPowerManagementProfile;
// uint16_t SCI_Interrupt;
// uint32_t SMI_CommandPort;
// uint8_t  AcpiEnable;
// uint8_t  AcpiDisable;
// uint8_t  S4BIOS_REQ;
// uint8_t  PSTATE_Control;
// uint32_t PM1aEventBlock;
// uint32_t PM1bEventBlock;
// uint32_t PM1aControlBlock;
// uint32_t PM1bControlBlock;
// uint32_t PM2ControlBlock;
// uint32_t PMTimerBlock;
// uint32_t GPE0Block;
// uint32_t GPE1Block;
// uint8_t  PM1EventLength;
// uint8_t  PM1ControlLength;
// uint8_t  PM2ControlLength;
// uint8_t  PMTimerLength;
// uint8_t  GPE0Length;
// uint8_t  GPE1Length;
// uint8_t  GPE1Base;
// uint8_t  CStateControl;
// uint16_t WorstC2Latency;
// uint16_t WorstC3Latency;
// uint16_t FlushSize;
// uint16_t FlushStride;
// uint8_t  DutyOffset;
// uint8_t  DutyWidth;
// uint8_t  DayAlarm;
// uint8_t  MonthAlarm;
// uint8_t  Century;

// // reserved in ACPI 1.0; used since ACPI 2.0+
// uint16_t BootArchitectureFlags;

// uint8_t  Reserved2;
// uint32_t Flags;

// // 12 byte structure; see below for details
// GenericAddressStructure ResetReg;

// uint8_t  ResetValue;
// uint8_t  Reserved3[3];

// // 64bit pointers - Available on ACPI 2.0+
// uint64_t                X_FirmwareControl;
// uint64_t                X_Dsdt;

// GenericAddressStructure X_PM1aEventBlock;
// GenericAddressStructure X_PM1bEventBlock;
// GenericAddressStructure X_PM1aControlBlock;
// GenericAddressStructure X_PM1bControlBlock;
// GenericAddressStructure X_PM2ControlBlock;
// GenericAddressStructure X_PMTimerBlock;
// GenericAddressStructure X_GPE0Block;
// GenericAddressStructure X_GPE1Block;
// } fadt_t;

typedef struct generic_address_struct {
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  uint8_t AccessSize;
  uint64_t Address;
} generic_address_struct_t;

typedef struct fadt {
  sdt_t h;
  uint32_t firmware_ctrl;
  uint32_t dsdt;

  uint8_t reserved1;

  uint8_t preferred_power_management_profile;
  uint16_t sci_interrupt;
  uint32_t smi_command_port;
  uint8_t acpi_enable;
  uint8_t acpi_disable;
  uint8_t s4bios_req;
  uint8_t pstate_control;
  uint32_t pm1_aevent_block;
  uint32_t pm1_bevent_block;
  uint32_t pm1_acontrol_block;
  uint32_t pm1_bcontrol_block;
  uint32_t pm2_control_block;
  uint32_t pm_timer_block;
  uint32_t gpe0_block;
  uint32_t gpe1_block;
  uint8_t pm1_event_length;
  uint8_t pm1_control_length;
  uint8_t pm2_control_length;
  uint8_t pm_timer_length;
  uint8_t gpe0_length;
  uint8_t gpe1_length;
  uint8_t gpe1_base;
  uint8_t cstate_control;
  uint16_t worstc2_latency;
  uint16_t worstc3_latency;
  uint16_t flush_size;
  uint16_t flush_stride;
  uint8_t duty_offset;
  uint8_t duty_width;
  uint8_t day_alarm;
  uint8_t month_alarm;
  uint8_t century;

  uint16_t boot_architecture_flags;

  uint8_t reserved2;
  uint32_t flags;

  generic_address_struct_t reset_reg;

  uint8_t reset_value;
  uint8_t reserved3[3];

  uint64_t x_firmware_control;
  uint64_t x_dsdt;

  generic_address_struct_t x_pm1_aeventblock;
  generic_address_struct_t x_pm1_beventblock;
  generic_address_struct_t x_pm1_acontrolblock;
  generic_address_struct_t x_pm1_bcontrolblock;
  generic_address_struct_t x_pm2_controlblock;
  generic_address_struct_t x_pm_timerblock;
  generic_address_struct_t x_gpe0_block;
  generic_address_struct_t x_gpe1_block;
} fadt_t;

#endif
