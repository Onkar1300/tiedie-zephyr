/* main.c - Application main entry point */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>

/* --- Custom Advertising Intervals --- */
/* Values are in units of 0.625 ms */
/* --- Custom Advertising Intervals --- */
/* Values are in units of 0.625 ms */
#define ADV_INTERVAL_20MS   32
#define ADV_INTERVAL_30MS   48
#define ADV_INTERVAL_50MS   80
#define ADV_INTERVAL_70MS   112
#define ADV_INTERVAL_100MS  160
#define ADV_INTERVAL_300MS  480
#define ADV_INTERVAL_500MS  800
#define ADV_INTERVAL_700MS  1120
#define ADV_INTERVAL_1000MS 1600

/* ---> SELECT YOUR DESIRED INTERVAL HERE FOR YOUR EXPERIMENTS <--- */
#define ACTIVE_ADV_INTERVAL ADV_INTERVAL_500MS

/* Custom Advertising Parameters */
#define CUSTOM_ADV_PARAMS BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, \
                                          ACTIVE_ADV_INTERVAL, \
                                          ACTIVE_ADV_INTERVAL, \
                                          NULL)
/* ------------------------------------ */

static bool hrf_ntf_enabled;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
#if defined(CONFIG_BT_EXT_ADV)
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
#endif /* CONFIG_BT_EXT_ADV */
};

#if !defined(CONFIG_BT_EXT_ADV)
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};
#endif /* !CONFIG_BT_EXT_ADV */

/* --- Custom GATT Service Start --- */
#define CUSTOM_SERVICE_VAL BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define READ_ATTR_VAL      BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define WRITE_ATTR_VAL     BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)
#define NOTIFY_ATTR_VAL    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)
#define INDICATE_ATTR_VAL  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4)

static struct bt_uuid_128 custom_svc_uuid = BT_UUID_INIT_128(CUSTOM_SERVICE_VAL);
static struct bt_uuid_128 read_uuid       = BT_UUID_INIT_128(READ_ATTR_VAL);
static struct bt_uuid_128 write_uuid      = BT_UUID_INIT_128(WRITE_ATTR_VAL);
static struct bt_uuid_128 notify_uuid     = BT_UUID_INIT_128(NOTIFY_ATTR_VAL);
static struct bt_uuid_128 indicate_uuid   = BT_UUID_INIT_128(INDICATE_ATTR_VAL);

static uint8_t read_value = 100;
static uint8_t write_value = 0;
static uint8_t notify_data = 0;
static uint8_t indicate_data = 0;
static bool notify_enabled = false;
static bool indicate_enabled = false;

static ssize_t read_custom_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;
	printk("App read request\n");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
}

static ssize_t write_custom_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	uint8_t *value = attr->user_data;
	if (len != sizeof(*value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	memcpy(value, buf, len);
	printk("App wrote value: %d\n", *value);
	return len;
}

static void notify_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	printk("Notify %s\n", notify_enabled ? "enabled" : "disabled");
}

static void indicate_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	indicate_enabled = (value == BT_GATT_CCC_INDICATE);
	printk("Indicate %s\n", indicate_enabled ? "enabled" : "disabled");
}

/* Service definition */
BT_GATT_SERVICE_DEFINE(custom_service,
	BT_GATT_PRIMARY_SERVICE(&custom_svc_uuid),
	/* Index 1, 2 */
	BT_GATT_CHARACTERISTIC(&read_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_custom_cb, NULL, &read_value),
	/* Index 3, 4 */
	BT_GATT_CHARACTERISTIC(&write_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, write_custom_cb, &write_value),
	/* Index 5, 6 */
	BT_GATT_CHARACTERISTIC(&notify_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
	/* Index 7 */
	BT_GATT_CCC(notify_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	/* Index 8, 9 */
	BT_GATT_CHARACTERISTIC(&indicate_uuid.uuid, BT_GATT_CHRC_INDICATE, BT_GATT_PERM_NONE, NULL, NULL, NULL),
	/* Index 10 */
	BT_GATT_CCC(indicate_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

static void indicate_cb(struct bt_conn *conn, struct bt_gatt_indicate_params *params, uint8_t err) {
	printk("Indication acknowledged by app.\n");
}
/* --- Custom GATT Service End --- */

enum {
	STATE_CONNECTED,
	STATE_DISCONNECTED,

	STATE_BITS,
};

static ATOMIC_DEFINE(state, STATE_BITS);

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
	} else {
		printk("Connected\n");

		(void)atomic_set_bit(state, STATE_CONNECTED);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));

	(void)atomic_set_bit(state, STATE_DISCONNECTED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void hrs_ntf_changed(bool enabled)
{
	hrf_ntf_enabled = enabled;

	printk("HRS notification status changed: %s\n",
	       enabled ? "enabled" : "disabled");
}

static struct bt_hrs_cb hrs_cb = {
	.ntf_changed = hrs_ntf_changed,
};

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

static void bas_notify(void)
{
	uint8_t battery_level = bt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_bas_set_battery_level(battery_level);
}

static void hrs_notify(void)
{
	static uint8_t heartrate = 90U;

	/* Heartrate measurements simulation */
	heartrate++;
	if (heartrate == 160U) {
		heartrate = 90U;
	}

	if (hrf_ntf_enabled) {
		bt_hrs_notify(heartrate);
	}
}

#if defined(CONFIG_GPIO)
/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
#include <zephyr/drivers/gpio.h>
#define HAS_LED     1
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define BLINK_ONOFF K_MSEC(500)

static struct k_work_delayable blink_work;
static bool                  led_is_on;

static void blink_timeout(struct k_work *work)
{
	led_is_on = !led_is_on;
	gpio_pin_set(led.port, led.pin, (int)led_is_on);

	k_work_schedule(&blink_work, BLINK_ONOFF);
}

static int blink_setup(void)
{
	int err;

	printk("Checking LED device...");
	if (!gpio_is_ready_dt(&led)) {
		printk("failed.\n");
		return -EIO;
	}
	printk("done.\n");

	printk("Configuring GPIO pin...");
	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (err) {
		printk("failed.\n");
		return -EIO;
	}
	printk("done.\n");

	k_work_init_delayable(&blink_work, blink_timeout);

	return 0;
}

static void blink_start(void)
{
	printk("Start blinking LED...\n");
	led_is_on = false;
	gpio_pin_set(led.port, led.pin, (int)led_is_on);
	k_work_schedule(&blink_work, BLINK_ONOFF);
}

static void blink_stop(void)
{
	struct k_work_sync work_sync;

	printk("Stop blinking LED.\n");
	k_work_cancel_delayable_sync(&blink_work, &work_sync);

	/* Keep LED on */
	led_is_on = true;
	gpio_pin_set(led.port, led.pin, (int)led_is_on);
}
#endif /* LED0_NODE */
#endif /* CONFIG_GPIO */

int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	bt_conn_auth_cb_register(&auth_cb_display);

	bt_hrs_cb_register(&hrs_cb);

#if !defined(CONFIG_BT_EXT_ADV)
	printk("Starting Legacy Advertising (connectable and scannable)\n");
	err = bt_le_adv_start(CUSTOM_ADV_PARAMS, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return 0;
	}

#else /* CONFIG_BT_EXT_ADV */
	struct bt_le_adv_param adv_param = {
		.id = BT_ID_DEFAULT,
		.sid = 0U,
		.secondary_max_skip = 0U,
		.options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_CODED),
		.interval_min = ACTIVE_ADV_INTERVAL,
		.interval_max = ACTIVE_ADV_INTERVAL,
		.peer = NULL,
	};
	struct bt_le_ext_adv *adv;

	printk("Creating a Coded PHY connectable non-scannable advertising set\n");
	err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
	if (err) {
		printk("Failed to create Coded PHY extended advertising set (err %d)\n", err);

		printk("Creating a non-Coded PHY connectable non-scannable advertising set\n");
		adv_param.options &= ~BT_LE_ADV_OPT_CODED;
		err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
		if (err) {
			printk("Failed to create extended advertising set (err %d)\n", err);
			return 0;
		}
	}

	printk("Setting extended advertising data\n");
	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Failed to set extended advertising data (err %d)\n", err);
		return 0;
	}

	printk("Starting Extended Advertising (connectable non-scannable)\n");
	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start extended advertising set (err %d)\n", err);
		return 0;
	}
#endif /* CONFIG_BT_EXT_ADV */

	printk("Advertising successfully started\n");

#if defined(HAS_LED)
	err = blink_setup();
	if (err) {
		return 0;
	}

	blink_start();
#endif /* HAS_LED */

	/* Implement notification. */
	while (1) {
		k_sleep(K_SECONDS(1));

		/* Heartrate measurements simulation */
		hrs_notify();

		/* Battery level simulation */
		bas_notify();

		/* Custom GATT Service Notification / Indication simulation */
		if (notify_enabled) {
			notify_data++;
			bt_gatt_notify(NULL, &custom_service.attrs[6], &notify_data, sizeof(notify_data));
		}

		if (indicate_enabled) {
			static struct bt_gatt_indicate_params ind_params;
			indicate_data++;
			ind_params.attr = &custom_service.attrs[9];
			ind_params.func = indicate_cb;
			ind_params.destroy = NULL;
			ind_params.data = &indicate_data;
			ind_params.len = sizeof(indicate_data);
			bt_gatt_indicate(NULL, &ind_params);
		}

		if (atomic_test_and_clear_bit(state, STATE_CONNECTED)) {
			/* Connected callback executed */

#if defined(HAS_LED)
			blink_stop();
#endif /* HAS_LED */
		} else if (atomic_test_and_clear_bit(state, STATE_DISCONNECTED)) {
#if !defined(CONFIG_BT_EXT_ADV)
			printk("Starting Legacy Advertising (connectable and scannable)\n");
			err = bt_le_adv_start(CUSTOM_ADV_PARAMS, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
			if (err) {
				printk("Advertising failed to start (err %d)\n", err);
				return 0;
			}

#else /* CONFIG_BT_EXT_ADV */
			printk("Starting Extended Advertising (connectable and non-scannable)\n");
			err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
			if (err) {
				printk("Failed to start extended advertising set (err %d)\n", err);
				return 0;
			}
#endif /* CONFIG_BT_EXT_ADV */

#if defined(HAS_LED)
			blink_start();
#endif /* HAS_LED */
		}
	}

	return 0;
}