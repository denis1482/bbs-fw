#include "cfgstore.h"
#include "stc15.h"
#include "eeprom.h"
#include "eventlog.h"
#include "uart.h"

#include <string.h>


#define CONFIG_EEPROM_PAGE		0


typedef struct
{
	uint8_t version;
	uint8_t length;
	uint8_t checksum;
} config_header_t;


static config_t __xdata config;


static bool read_config();
static bool write_config();
static void load_default_config();

void cfgstore_init()
{
	if (!read_config())
	{
		load_default_config();
		if (write_config())
		{
			eventlog_write(EVT_MSG_CONFIG_RESET);
		}
	}
	else
	{
		load_default_config();
		eventlog_write(EVT_MSG_CONFIG_READ);
	}
}


config_t* cfgstore_get()
{
	return &config;
}

bool cfgstore_save()
{
	return write_config();
}


static bool read_config()
{
	config_header_t header;
	uint8_t read_offset = 0;
	uint8_t* ptr = 0;
	uint8_t i = 0;
	int data;

	if (!eeprom_select_page(CONFIG_EEPROM_PAGE))
	{
		eventlog_write(EVT_ERROR_CONFIG_READ_EEPROM);
		return false;
	}

	ptr = (uint8_t*)&header;
	for (i = 0; i < sizeof(config_header_t); ++i)
	{
		data = eeprom_read_byte(read_offset);
		if (data < 0)
		{
			eventlog_write(EVT_ERROR_CONFIG_READ_EEPROM);
			return false;
		}
		*ptr = (uint8_t)data;
		++read_offset;
		++ptr;
	}

	// verify header ok
	if (header.version != CONFIG_VERSION)
	{
		eventlog_write(EVT_ERROR_CONFIG_VERSION);
		return false;
	}

	if (header.length != sizeof(config_t))
	{
		eventlog_write(EVT_ERROR_CONFIG_VERSION);
		return false;
	}

	uint8_t checksum = 0;

	ptr = (uint8_t*)&config;
	for (i = 0; i < sizeof(config_t); ++i)
	{
		data = eeprom_read_byte(read_offset);
		if (data < 0)
		{
			eventlog_write(EVT_ERROR_CONFIG_READ_EEPROM);
			return false;
		}

		checksum += (uint8_t)data;
		*ptr = (uint8_t)data;
		++read_offset;
		++ptr;
	}

	if (header.checksum != checksum)
	{
		eventlog_write(EVT_ERROR_CONFIG_CHECKSUM);
		return false;
	}

	return true;
}

static bool write_config()
{
	config_header_t header;
	uint8_t write_offset = 0;
	uint8_t* ptr = 0;
	uint8_t i = 0;

	header.version = CONFIG_VERSION;
	header.length = sizeof(config_t);
	header.checksum = 0;

	if (!eeprom_select_page(CONFIG_EEPROM_PAGE))
	{
		eventlog_write(EVT_ERROR_CONFIG_WRITE_EEPROM);
		return false;
	}

	if (!eeprom_erase_page())
	{
		eventlog_write(EVT_ERROR_CONFIG_ERASE_EEPROM);
		return false;
	}

	write_offset += sizeof(config_header_t);

	ptr = (uint8_t*)&config;
	for (i = 0; i < sizeof(config_t); ++i)
	{
		if (!eeprom_write_byte(write_offset, *ptr))
		{
			eventlog_write(EVT_ERROR_CONFIG_WRITE_EEPROM);
			return false;
		}

		header.checksum += *ptr;
		++write_offset;
		++ptr;
	}

	write_offset = 0;
	ptr = (uint8_t*)&header;
	for (i = 0; i < sizeof(config_header_t); ++i)
	{
		if (!eeprom_write_byte(write_offset, *ptr))
		{
			eventlog_write(EVT_ERROR_CONFIG_WRITE_EEPROM);
			return false;
		}

		++write_offset;
		++ptr;
	}

	return true;
}

static void load_default_config()
{
	config.max_current_amps = 30;
	config.low_voltage_cut_off = 24;		// :TODO: change!!!

	config.use_speed_sensor = 1;
	config.use_display = 1;
	config.use_push_walk = 1;

	config.wheel_size_inch_x10 = 280;
	config.speed_sensor_signals = 1;
	config.max_speed_kph = 40;

	config.pas_start_delay_pulses = 5;
	config.pas_stop_delay_ms = 200;

	config.throttle_start_voltage_mv = 1100;
	config.throttle_end_voltage_mv = 4000;
	config.throttle_start_percent = 10;

	config.assist_mode_select = ASSIST_MODE_SELECT_OFF;
	config.assist_startup_level = 3;

	memset(&config.assist_levels, 0, 20 * sizeof(assist_level_t));

	uint8_t current_limits[9] = { 25, 34, 43, 51, 60, 68, 74, 82, 90 };
	for (uint8_t i = 0; i < 9; ++i)
	{
		config.assist_levels[0][i+1].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_THROTTLE;
		config.assist_levels[0][i+1].target_current_percent = current_limits[i];
		config.assist_levels[0][i+1].max_speed_percent = 100;
		config.assist_levels[0][i+1].max_throttle_current_percent = 100;
	}
}
