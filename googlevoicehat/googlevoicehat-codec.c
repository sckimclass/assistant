/*
 * Driver for the Google voiceHAT audio codec for Raspberry Pi.
 *
 * Author:	Peter Malkin <petermalkin@google.com>
 *		Copyright 2016
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>

#include <linux/i2c.h>
#include <linux/regmap.h>

#define ICS43432_RATE_MIN_HZ	7190  /* from data sheet */
#define ICS43432_RATE_MAX_HZ	52800 /* from data sheet */
/* Delay in enabling SDMODE after clock settles to remove pop */
#define SDMODE_DELAY_MS		5

static const struct reg_default voicehat_reg_defaults[] = {
	{0x00, 0x00}, //State_Control_1
	{0x01, 0x81}, //State_Control_2
	{0x02, 0x50}, //State_Control_3
	{0x03, 0x4e}, //Master_volume_control
	{0x04, 0x18}, //Channel_1_volume_control
	{0x05, 0x18}, //Channel_2_volume_control
	{0x06, 0xa2}, //Under_Voltage_selection_for_high_voltage_supply
	{0x07, 0xfe}, //State_control_4
	{0x08, 0x6a}, //DRC_limiter_attack/release_rate
	{0x09, 0x60}, //Prohibited
	{0x0c, 0x32}, //Prohibited
	{0x0d, 0x00}, //Prohibited
	{0x0e, 0x00}, //Prohibited
	{0x0f, 0x00}, //Prohibited
	{0x10, 0x20}, //Top_5_bits_of_Attack_Threshold
	{0x11, 0x00}, //Middle_8_bits_of_Attack_Threshold
	{0x12, 0x00}, //Bottom_8_bits_of_Attack_Threshold
	{0x13, 0x20}, //Top_8_bits_of_Power_Clipping
	{0x14, 0x00}, //Middle_8_bits_of_Power_Clipping
	{0x15, 0x00}, //Bottom_8_bits_of_Power_Clipping
	{0x16, 0x00}, //State_control_5
	{0x17, 0x00}, //Volume_Fine_Tune
	{0x18, 0x40}, //DDynamic_Temperature_Control
	{0x1a, 0x00}, //Top_8_bits_of_Noise_Gate_Attack_Level
	{0x1b, 0x00}, //Middle_8_bits_of_Noise_Gate_Attack_Level
	{0x1c, 0x1a}, //Bottom_8_bits_of_Noise_Gate_Attack_Level
	{0x1d, 0x00}, //Top_8_bits_of_Noise_Gate_Release_Level
	{0x1e, 0x00}, //Middle_8_bits_of_Noise_Gate_Release_Level
	{0x1f, 0x53}, //Bottom_8_bits_of_Noise_Gate_Release_Level
	{0x20, 0x00}, //Top_8_bits_of_DRC_Energy_Coefficient
	{0x21, 0x10}, //Bottom_8_bits_of_DRC_Energy_Coefficient
	{0x22, 0x08}, //Top_8_bits_of_Release_Threshold_For_DRC
	{0x23, 0x00}, //Middle_8_bits_of_Release_Threshold
	{0x24, 0x00}, //Bottom_8_bits_of_Release_Threshold
	{0x25, 0x34}, //Device Number
	{0x2e, 0x30}, //
	{0x2f, 0x06}, //
};

struct voicehat_priv {
	struct device *dev;
	struct regmap *regmap;
	struct delayed_work enable_sdmode_work;
	struct gpio_desc *sdmode_gpio;
	unsigned long sdmode_delay_jiffies;
};

static void voicehat_enable_sdmode_work(struct work_struct *work)
{
	struct voicehat_priv *voicehat = container_of(work,
						      struct voicehat_priv,
						      enable_sdmode_work.work);
	gpiod_set_value(voicehat->sdmode_gpio, 1);
}

static int voicehat_component_probe(struct snd_soc_component *component)
{
	struct voicehat_priv *voicehat =
				snd_soc_component_get_drvdata(component);

	voicehat->sdmode_gpio = devm_gpiod_get(component->dev, "sdmode",
					       GPIOD_OUT_LOW);
	if (IS_ERR(voicehat->sdmode_gpio)) {
		dev_err(component->dev, "Unable to allocate GPIO pin\n");
		return PTR_ERR(voicehat->sdmode_gpio);
	}

	INIT_DELAYED_WORK(&voicehat->enable_sdmode_work,
			  voicehat_enable_sdmode_work);
	gpiod_set_value(voicehat->sdmode_gpio, 1);
	return 0;
}

static void voicehat_component_remove(struct snd_soc_component *component)
{
	struct voicehat_priv *voicehat =
				snd_soc_component_get_drvdata(component);

	cancel_delayed_work_sync(&voicehat->enable_sdmode_work);
}

static const struct snd_soc_dapm_widget voicehat_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("Speaker"),
};

static const struct snd_soc_dapm_route voicehat_dapm_routes[] = {
	{"Speaker", NULL, "HiFi Playback"},
};

static const struct snd_soc_component_driver voicehat_component_driver = {
	.probe = voicehat_component_probe,
	.remove = voicehat_component_remove,
	.dapm_widgets = voicehat_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(voicehat_dapm_widgets),
	.dapm_routes = voicehat_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(voicehat_dapm_routes),
};

static int voicehat_daiops_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct voicehat_priv *voicehat =
				snd_soc_component_get_drvdata(component);

	if (voicehat->sdmode_delay_jiffies == 0)
		return 0;

	dev_dbg(dai->dev, "CMD             %d", cmd);
	dev_dbg(dai->dev, "Playback Active %d", dai->playback_active);
	dev_dbg(dai->dev, "Capture Active  %d", dai->capture_active);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (dai->playback_active) {
			dev_info(dai->dev, "Enabling audio amp...\n");
			queue_delayed_work(
				system_power_efficient_wq,
				&voicehat->enable_sdmode_work,
				voicehat->sdmode_delay_jiffies);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#if 0
		if (dai->playback_active) {
			cancel_delayed_work(&voicehat->enable_sdmode_work);
			dev_info(dai->dev, "Disabling audio amp...\n");
			gpiod_set_value(voicehat->sdmode_gpio, 0);
		}
#endif
		break;
	}
	return 0;
}

static const struct snd_soc_dai_ops voicehat_dai_ops = {
	.trigger = voicehat_daiops_trigger,
};

static struct snd_soc_dai_driver voicehat_dai = {
	.name = "voicehat-hifi",
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE
	},
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE
	},
	.ops = &voicehat_dai_ops,
	.symmetric_rates = 1
};

#ifdef CONFIG_OF
static const struct of_device_id voicehat_ids[] = {
		{ .compatible = "google,voicehat", }, {}
	};
	MODULE_DEVICE_TABLE(of, voicehat_ids);
#endif

#define VOICEHAT_REG_MAX 0x2F

static bool voicehat_volatile_register(struct device *dev, unsigned int reg)
{
	return true; /*all register are volatile*/
}

static const struct regmap_config voicehat_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = VOICEHAT_REG_MAX,
	.reg_defaults = voicehat_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(voicehat_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

static int voicehat_i2c_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	struct voicehat_priv *voicehat;
	unsigned int sdmode_delay;
	int ret;
	struct device *dev = &i2c->dev;
	struct regmap *regmap;
	int i;

printk("%s 1\n", __func__);
	regmap = devm_regmap_init_i2c(i2c, &voicehat_regmap_config);

	voicehat = devm_kzalloc(dev, sizeof(*voicehat), GFP_KERNEL);
	if (!voicehat)
		return -ENOMEM;

printk("%s 2\n", __func__);
	ret = device_property_read_u32(dev, "voicehat_sdmode_delay",
				       &sdmode_delay);

	if (ret) {
		sdmode_delay = SDMODE_DELAY_MS;
		dev_info(dev,
			 "property 'voicehat_sdmode_delay' not found default 5 mS");
	} else {
		dev_info(dev, "property 'voicehat_sdmode_delay' found delay= %d mS",
			 sdmode_delay);
	}
	voicehat->sdmode_delay_jiffies = msecs_to_jiffies(sdmode_delay);

printk("%s 3\n", __func__);
	voicehat->regmap = regmap;
	voicehat->dev = dev;
	dev_set_drvdata(dev, voicehat);

	for (i = 0; i < ARRAY_SIZE(voicehat_reg_defaults); i++) {
		regmap_write(regmap, voicehat_reg_defaults[i].reg, voicehat_reg_defaults[i].def);
	}

	ret =  snd_soc_register_component(dev,
					  &voicehat_component_driver,
					  &voicehat_dai,
					  1);

	printk("%s 4 %d \n", __func__, ret);
	return ret;
}

static int voicehat_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_component(&client->dev);
	return 0;
}

static const struct i2c_device_id cx2092x_i2c_id[] = {
	{"voicehat-codec", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, cx2092x_i2c_id);

static struct i2c_driver voicehat_i2c_driver = {
	.driver = {
		.name = "voicehat-codec",
		.of_match_table = of_match_ptr(voicehat_ids),
	},
//	.id_table = cx2092x_i2c_id,
	.probe = voicehat_i2c_probe,
	.remove = voicehat_i2c_remove,
};

module_i2c_driver(voicehat_i2c_driver);

MODULE_DESCRIPTION("Google voiceHAT Codec driver");
MODULE_AUTHOR("Peter Malkin <petermalkin@google.com>");
MODULE_LICENSE("GPL v2");
