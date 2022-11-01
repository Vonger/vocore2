#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MEM_DEF "/dev/mem"
char mem_fn[PATH_MAX];
int mem_fd = -1;

#define SYSCTL_ADDR_START 0x10000000

struct {
	const char chip_id[8];
} *mt7628_reg = NULL;

#define FACTORY_MTD_NAME "factory"

// a pice of shit, from ralink's code, change it carefully.
#define EEPROM_SIZE 0x200

#define THADC_ANALOG_PART 0x53
#define THADC_SLOP 0x54
#define THERMO_SLOPE_VARIATION_MASK (0x1f)

#define TEMP_SENSOR_CAL_MASK (0x7f << 8)
#define TEMP_SENSOR_CAL_EN (1 << 15)

#define TEMP_SENSOR_CAL 0x55
#define TEMPERATURE_SENSOR_CALIBRATION 0x55
#define THERMO_REF_ADC_VARIATION_MASK (0x7f)

#define THERMAL_COMPENSATION_OFFSET 0xF8

#define RTMP_MAC_CSR_ADDR       (0xB0300000 - 0xA0000000)

#define MT_PSE_BASE_ADDR                0xa0000000

#define MT_TOP_REMAP_ADDR               0x80000000	//TOP start address 8002-0000, but only can remap to 8000-0000
#define MT_TOP_REMAP_ADDR_THEMAL         0xa2000	//Get Thermal sensor adc cal value: 0x80022000 bits(8,14), Offset 0x80000 + 0x22000 = 0xa2000
#define MT_TOP_THEMAL_ADC       0x80022000	//Get Thermal sensor adc cal value: 0x80022000 bits(8,14)
#define MCU_CFG_BASE            0x2000
#define MCU_PCIE_REMAP_1        (MCU_CFG_BASE + 0x500)
#define REMAP_1_OFFSET_MASK (0x3ffff)
#define GET_REMAP_1_OFFSET(p) (((p) & REMAP_1_OFFSET_MASK))
#define REMAP_1_BASE_MASK       (0x3fff << 18)
#define GET_REMAP_1_BASE(p) (((p) & REMAP_1_BASE_MASK) >> 18)
#define MCU_PCIE_REMAP_2        (MCU_CFG_BASE + 0x504)
#define REMAP_2_OFFSET_MASK (0x7ffff)
#define GET_REMAP_2_OFFSET(p) (((p) & REMAP_2_OFFSET_MASK))
#define REMAP_2_BASE_MASK (0x1fff << 19)
#define GET_REMAP_2_BASE(p) (((p) & REMAP_2_BASE_MASK) >> 19)

FILE *mtd_table_fp = NULL;
char factory_mtd_fn[PATH_MAX] = { 0 };

int factory_mtd_fd = -1;
unsigned char *eeprom = NULL;

unsigned char *csr_mem = NULL;
size_t csr_size = (10 * 1024 * 1024);

#define LINEMAX 512
int eeprom_init(void)
{
	mtd_table_fp = fopen("/proc/mtd", "r");
	if (mtd_table_fp == NULL) {
		fprintf(stderr, "can't open /proc/mtd, REASON: %s\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	char line[LINEMAX];
	factory_mtd_fn[0] = '\0';
	while (fgets(line, LINEMAX, mtd_table_fp) != NULL) {
		if (strstr(line, FACTORY_MTD_NAME) != NULL) {
			snprintf(factory_mtd_fn, PATH_MAX, "/dev/%s",
				 strtok(line, ":"));
			break;
		}
	}
	factory_mtd_fd = open(factory_mtd_fn, O_RDONLY);
	if (factory_mtd_fd < 0) {
		fprintf(stderr, "can't open %s, REASON: %s\n", factory_mtd_fn,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	eeprom = (unsigned char *)malloc(EEPROM_SIZE);
	if (eeprom == NULL) {
		fprintf(stderr, "malloc() failed, REASON: %s\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (pread(factory_mtd_fd, eeprom, EEPROM_SIZE, 0) != EEPROM_SIZE) {
		fprintf(stderr, "can't read %s, REASON: %s\n",
			factory_mtd_fn, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return 1;
}

void eeprom_uninit(void)
{
	if (eeprom != NULL)
		free(eeprom);
	eeprom = NULL;
	if (factory_mtd_fd >= 0)
		close(factory_mtd_fd);
	factory_mtd_fd = 0;
	if (mtd_table_fp != NULL)
		fclose(mtd_table_fp);
	mtd_table_fp = NULL;
}

int mem_init(void)
{
	strncpy(mem_fn, MEM_DEF, PATH_MAX);
	char *t = getenv("MEM");
	if (t != NULL)
		strncpy(mem_fn, t, PATH_MAX);
	mem_fd = open(mem_fn, O_RDWR | O_SYNC);
	if (mem_fd < 0) {
		fprintf(stderr, "can't open %s,REASON: %s\n",
			mem_fn, strerror(errno));
		return -1;
	}
	mt7628_reg = mmap(NULL, sizeof(*mt7628_reg),
			  PROT_READ | PROT_WRITE,
			  MAP_SHARED, mem_fd, SYSCTL_ADDR_START);
	if (mt7628_reg == MAP_FAILED) {
		fprintf(stderr, "can't mmap %x,REASON: %s\n",
			SYSCTL_ADDR_START, strerror(errno));
		return -1;
	}
	csr_mem = mmap(NULL, csr_size,
		       PROT_READ | PROT_WRITE,
		       MAP_SHARED, mem_fd, RTMP_MAC_CSR_ADDR);
	if (csr_mem == MAP_FAILED) {
		fprintf(stderr, "can't mmap %x,REASON: %s\n",
			RTMP_MAC_CSR_ADDR, strerror(errno));
		return -1;
	}
	return 1;
}

void mem_uninit(void)
{
	if (mem_fd >= 0)
		close(mem_fd);
}

static inline uint32_t __attribute__((optimize("O0"))) RTMP_IO_READ32(uint32_t addr)
{
	uint8_t *mem = csr_mem + addr;
	return (*(volatile uint32_t *)mem);
}

static inline void __attribute__((optimize("O0"))) RTMP_IO_WRITE32(uint32_t addr, uint32_t value)
{
	uint8_t *mem = csr_mem + addr;
	*(volatile uint32_t *)mem = value;
}

int32_t get_temperature(void)
{

	int32_t result = 0;

	uint32_t mac_val = 0;
	uint32_t mac_restore_val = 0;

	mac_restore_val = RTMP_IO_READ32(MCU_PCIE_REMAP_2);
	RTMP_IO_WRITE32(MCU_PCIE_REMAP_2, MT_TOP_REMAP_ADDR);
	mac_val = RTMP_IO_READ32(MT_TOP_REMAP_ADDR_THEMAL);
	result = (mac_val & 0x00007f00) >> 8;
	RTMP_IO_WRITE32(MCU_PCIE_REMAP_2, mac_restore_val);

	int32_t g_ucThermoRefAdcVal, g_cThermoSlopeVariation,
	    g_cThermoRefOffset = 0;

	if (eeprom[TEMPERATURE_SENSOR_CALIBRATION] & 0x80) {
		g_ucThermoRefAdcVal =
		    eeprom[TEMPERATURE_SENSOR_CALIBRATION] &
		    THERMO_REF_ADC_VARIATION_MASK;
	} else {
		g_ucThermoRefAdcVal = 52;
	}

	if (eeprom[THADC_ANALOG_PART] & 0x80) {
		g_cThermoSlopeVariation =
		    eeprom[THADC_SLOP] & THERMO_SLOPE_VARIATION_MASK;
		if (g_cThermoSlopeVariation > 16) {
			g_cThermoSlopeVariation -= 32;
		}
	} else {
		g_cThermoSlopeVariation = 0;
	}

	g_cThermoRefOffset = eeprom[THERMAL_COMPENSATION_OFFSET] + 28;
	result =
	    (((result - g_ucThermoRefAdcVal) * (56 +
						g_cThermoSlopeVariation)) /
	     30) + g_cThermoRefOffset;
	return result;
}


void exit_hook(void)
{
	mem_uninit();
	eeprom_uninit();
}

int main(int argc, char *argv[])
{
	atexit(exit_hook);
	if (argc < 2) {
		fprintf(stderr, "usage: %s action\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if (mem_init() < 0)
		exit(EXIT_FAILURE);
	if (eeprom_init() < 0) {
		exit(EXIT_FAILURE);
	}

	if (strcmp(argv[1], "chip_name") == 0) {
		fwrite(mt7628_reg->chip_id, 8, 1, stdout);
	} else if (strcmp(argv[1], "temperature") == 0) {
		printf("%d\n", get_temperature());
	} else {
		fprintf(stderr,"list of action: \n");
		fprintf(stderr,"chip_name temperature\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}
